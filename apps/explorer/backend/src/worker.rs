use std::collections::VecDeque;
use std::env;
use std::io::{self, BufRead, BufReader, BufWriter, Read, Write};
use std::path::PathBuf;
use std::process::{Child, Command, ExitStatus, Stdio};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::mpsc::{self, RecvTimeoutError, Sender};
use std::thread::{self, JoinHandle};
use std::time::Duration;

use serde::{Deserialize, Serialize};

const MAX_REQUEST_LINE_BYTES: usize = 4 * 1024 * 1024;
const MAX_STREAM_BYTES: usize = 3 * 1024 * 1024;
const MAX_DIRECTORY_METRICS_EVENT_BYTES: usize = 64 * 1024;
const DIRECTORY_METRICS_PROGRESS_QUEUE_CAPACITY: usize = 32;
const CANCEL_OPERATION: &str = "cancel";
const COOPERATIVE_CANCEL_WAIT_STEPS: usize = 500;
const MAX_STDIN_PAYLOAD_BYTES: usize = 64 * 1024;

static CANCEL_MARKER_COUNTER: AtomicU64 = AtomicU64::new(0);

#[derive(Debug, Deserialize)]
struct Request {
    version: u32,
    id: String,
    arguments: Vec<String>,
    #[serde(rename = "stdinPayload", default)]
    stdin_payload: Option<String>,
}

#[derive(Debug, Serialize)]
struct Response {
    version: u32,
    id: String,
    ok: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    payload: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "errorCode")]
    error_code: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    error: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    stream: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    done: Option<bool>,
}

enum Incoming {
    Request(Request),
    ProtocolError(String),
}

struct CapturedStream {
    bytes: Vec<u8>,
    exceeded: bool,
    protocol_error: bool,
    terminal_seen: bool,
}

enum StreamSender {
    Lossless(Sender<io::Result<Vec<u8>>>),
    Bounded(mpsc::SyncSender<io::Result<Vec<u8>>>),
}

#[derive(Clone, Copy)]
enum StreamCapture {
    All,
    LastLine,
}

struct ActiveRequest {
    request: Request,
    child: Child,
    cancel_file: PathBuf,
    stdout: JoinHandle<io::Result<CapturedStream>>,
    stdout_lines: mpsc::Receiver<io::Result<Vec<u8>>>,
    stderr: JoinHandle<io::Result<CapturedStream>>,
}

pub fn run() -> Result<(), String> {
    let executable =
        env::current_exe().map_err(|error| format!("resolve worker executable: {error}"))?;
    let (incoming_sender, incoming_receiver) = mpsc::channel();
    let stdin = io::stdin();
    thread::spawn(move || read_requests(stdin.lock(), incoming_sender));

    let stdout = io::stdout();
    let mut stdout = BufWriter::new(stdout.lock());
    let mut queue = VecDeque::new();
    let mut active = None;
    let mut input_closed = false;

    loop {
        if active.is_none() {
            if let Some(request) = queue.pop_front() {
                active = match spawn_request(&executable, request) {
                    Ok(request) => Some(request),
                    Err((request, message)) => {
                        write_error_response(&mut stdout, request.id, "backend_spawn", message)?;
                        None
                    }
                };
                continue;
            }

            if input_closed {
                break;
            }

            match incoming_receiver.recv() {
                Ok(message) => handle_incoming(message, &mut active, &mut queue, &mut stdout)?,
                Err(_) => input_closed = true,
            }
            continue;
        }

        while let Ok(message) = incoming_receiver.try_recv() {
            handle_incoming(message, &mut active, &mut queue, &mut stdout)?;
        }

        if let Some(active_request) = active.as_mut() {
            drain_stdout_lines(active_request, &mut stdout)?;
        }

        let completed = active
            .as_mut()
            .expect("active worker request")
            .child
            .try_wait()
            .map_err(|error| format!("wait for backend operation: {error}"))?;
        if let Some(status) = completed {
            let request = active.take().expect("active worker request");
            write_finished_response(&mut stdout, request, status)?;
            continue;
        }

        match incoming_receiver.recv_timeout(Duration::from_millis(10)) {
            Ok(message) => {
                handle_incoming(message, &mut active, &mut queue, &mut stdout)?;
            }
            Err(RecvTimeoutError::Timeout) => {}
            Err(RecvTimeoutError::Disconnected) => {
                input_closed = true;
            }
        }
    }

    Ok(())
}

fn read_requests<R: BufRead>(reader: R, sender: Sender<Incoming>) {
    for line in reader.lines() {
        let Ok(line) = line else {
            break;
        };
        if line.len() > MAX_REQUEST_LINE_BYTES {
            if sender
                .send(Incoming::ProtocolError("request exceeds line limit".into()))
                .is_err()
            {
                break;
            }
            continue;
        }
        let message = match serde_json::from_str(&line) {
            Ok(request) => Incoming::Request(request),
            Err(error) => Incoming::ProtocolError(error.to_string()),
        };
        if sender.send(message).is_err() {
            break;
        }
    }
}

fn handle_incoming(
    message: Incoming,
    active: &mut Option<ActiveRequest>,
    queue: &mut VecDeque<Request>,
    stdout: &mut impl Write,
) -> Result<(), String> {
    let request = match message {
        Incoming::ProtocolError(error) => {
            return write_error_response(stdout, String::new(), "protocol_error", error);
        }
        Incoming::Request(request) => request,
    };

    if request.version != 1 || request.id.is_empty() || request.arguments.is_empty() {
        return write_error_response(
            stdout,
            request.id,
            "protocol_version",
            "unsupported, empty, or unidentified backend request".into(),
        );
    }

    if request.arguments.first().map(String::as_str) == Some(CANCEL_OPERATION) {
        return handle_cancel(request, active, queue, stdout);
    }

    queue.push_back(request);
    Ok(())
}

fn handle_cancel(
    request: Request,
    active: &mut Option<ActiveRequest>,
    queue: &mut VecDeque<Request>,
    stdout: &mut impl Write,
) -> Result<(), String> {
    let Some(target) = request.arguments.get(1).filter(|value| !value.is_empty()) else {
        return write_error_response(
            stdout,
            request.id,
            "protocol_error",
            "cancel request is missing its target id".into(),
        );
    };

    if active.as_ref().map(|item| item.request.id.as_str()) == Some(target.as_str()) {
        let request = active.take().expect("active worker request");
        terminate_request(request);
        return write_cancelled_response(stdout, target.clone());
    }

    if let Some(index) = queue
        .iter()
        .position(|item| item.id.as_str() == target.as_str())
    {
        queue.remove(index);
        return write_cancelled_response(stdout, target.clone());
    }

    Ok(())
}

fn spawn_request(
    executable: &std::path::Path,
    request: Request,
) -> Result<ActiveRequest, (Request, String)> {
    let cancel_file = cancellation_marker_path();
    let stdin_payload = request.stdin_payload.clone();
    let stream_capture =
        if request.arguments.first().map(String::as_str) == Some("directory-metrics") {
            StreamCapture::LastLine
        } else {
            StreamCapture::All
        };
    let mut command = Command::new(executable);
    command
        .args(&request.arguments)
        .env("ASTREA_CANCEL_FILE", &cancel_file);
    if stdin_payload.is_some() {
        command.env("ASTREA_STDIN_PAYLOAD", "1");
    }
    let mut child = match command
        .stdin(if stdin_payload.is_some() {
            Stdio::piped()
        } else {
            Stdio::null()
        })
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
    {
        Ok(child) => child,
        Err(error) => return Err((request, format!("run backend operation: {error}"))),
    };

    let (stdout_sender, stdout_lines) = match stream_capture {
        StreamCapture::All => {
            let (sender, receiver) = mpsc::channel();
            (StreamSender::Lossless(sender), receiver)
        }
        StreamCapture::LastLine => {
            let (sender, receiver) = mpsc::sync_channel(DIRECTORY_METRICS_PROGRESS_QUEUE_CAPACITY);
            (StreamSender::Bounded(sender), receiver)
        }
    };
    let stdout = match child.stdout.take() {
        Some(stream) => {
            thread::spawn(move || read_stream_lines(stream, stdout_sender, stream_capture))
        }
        None => {
            let _ = child.kill();
            let _ = child.wait();
            let _ = std::fs::remove_file(&cancel_file);
            return Err((request, "backend stdout pipe was unavailable".into()));
        }
    };

    if let Some(payload) = stdin_payload {
        if payload.len() > MAX_STDIN_PAYLOAD_BYTES {
            let _ = child.kill();
            let _ = child.wait();
            let _ = std::fs::remove_file(&cancel_file);
            return Err((
                request,
                "backend stdin payload exceeded the worker limit".into(),
            ));
        }
        let Some(mut stdin) = child.stdin.take() else {
            let _ = child.kill();
            let _ = child.wait();
            let _ = std::fs::remove_file(&cancel_file);
            return Err((request, "backend stdin pipe was unavailable".into()));
        };
        if let Err(error) = stdin.write_all(payload.as_bytes()) {
            let _ = child.kill();
            let _ = child.wait();
            let _ = std::fs::remove_file(&cancel_file);
            return Err((request, format!("write backend stdin payload: {error}")));
        }
    }
    let stderr = match child.stderr.take() {
        Some(stream) => thread::spawn(move || read_stream(stream)),
        None => {
            let _ = child.kill();
            let _ = child.wait();
            let _ = stdout.join();
            let _ = std::fs::remove_file(&cancel_file);
            return Err((request, "backend stderr pipe was unavailable".into()));
        }
    };

    Ok(ActiveRequest {
        request,
        child,
        cancel_file,
        stdout,
        stdout_lines,
        stderr,
    })
}

fn read_stream_lines(
    stream: impl Read,
    sender: StreamSender,
    capture: StreamCapture,
) -> io::Result<CapturedStream> {
    let mut reader = BufReader::new(stream);
    let mut captured = CapturedStream {
        bytes: Vec::new(),
        exceeded: false,
        protocol_error: false,
        terminal_seen: false,
    };
    loop {
        let line = match capture {
            StreamCapture::All => {
                let mut line = Vec::new();
                let count = reader.read_until(b'\n', &mut line)?;
                if count == 0 {
                    break;
                }
                line
            }
            StreamCapture::LastLine => {
                match read_bounded_line(&mut reader, MAX_DIRECTORY_METRICS_EVENT_BYTES) {
                    Ok(Some(line)) => line,
                    Ok(None) => break,
                    Err(_) => {
                        captured.exceeded = true;
                        break;
                    }
                }
            }
        };
        let count = line.len();
        match capture {
            StreamCapture::All => {
                if captured.bytes.len() < MAX_STREAM_BYTES {
                    let remaining = MAX_STREAM_BYTES - captured.bytes.len();
                    captured
                        .bytes
                        .extend_from_slice(&line[..count.min(remaining)]);
                }
                if captured.bytes.len() >= MAX_STREAM_BYTES
                    && count > MAX_STREAM_BYTES.saturating_sub(captured.bytes.len())
                {
                    captured.exceeded = true;
                }
            }
            StreamCapture::LastLine => {
                capture_directory_metrics_event(&mut captured, &line);
            }
        }
        if !send_stream_line(&sender, line) {
            break;
        }
    }
    Ok(captured)
}

fn capture_directory_metrics_event(captured: &mut CapturedStream, line: &[u8]) {
    let value = match serde_json::from_slice::<serde_json::Value>(line) {
        Ok(value) => value,
        Err(_) => {
            captured.protocol_error = true;
            return;
        }
    };
    let Some(object) = value.as_object() else {
        captured.protocol_error = true;
        return;
    };
    let event = object.get("event").and_then(serde_json::Value::as_str);
    let operation = object.get("operation").and_then(serde_json::Value::as_str);
    let is_progress = event == Some("progress") && operation == Some("directory-metrics");
    let is_terminal = event == Some("result") && operation == Some("directory-metrics");
    if captured.terminal_seen || (!is_progress && !is_terminal) {
        captured.protocol_error = true;
    }
    if is_terminal && !captured.terminal_seen {
        captured.terminal_seen = true;
        captured.bytes.clear();
        captured.bytes.extend_from_slice(line);
    }
}

fn read_bounded_line<R: BufRead>(reader: &mut R, max_bytes: usize) -> io::Result<Option<Vec<u8>>> {
    let mut line = Vec::with_capacity(max_bytes.min(8192));
    loop {
        let buffer = reader.fill_buf()?;
        if buffer.is_empty() {
            return if line.is_empty() {
                Ok(None)
            } else {
                Ok(Some(line))
            };
        }

        let chunk_length = buffer
            .iter()
            .position(|byte| *byte == b'\n')
            .map_or(buffer.len(), |position| position + 1);
        let line_length = line
            .len()
            .checked_add(chunk_length)
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidData, "event line is too large"))?;
        if line_length > max_bytes {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "event line exceeds its limit",
            ));
        }

        let ends_line = buffer[chunk_length - 1] == b'\n';
        line.extend_from_slice(&buffer[..chunk_length]);
        reader.consume(chunk_length);
        if ends_line {
            return Ok(Some(line));
        }
    }
}

fn send_stream_line(sender: &StreamSender, line: Vec<u8>) -> bool {
    match sender {
        StreamSender::Lossless(sender) => sender.send(Ok(line)).is_ok(),
        StreamSender::Bounded(sender) => match sender.try_send(Ok(line)) {
            Ok(()) | Err(mpsc::TrySendError::Full(_)) => true,
            Err(mpsc::TrySendError::Disconnected(_)) => false,
        },
    }
}

fn drain_stdout_lines(active: &mut ActiveRequest, stdout: &mut impl Write) -> Result<(), String> {
    while let Ok(line) = active.stdout_lines.try_recv() {
        let line = line.map_err(|error| format!("read backend stdout: {error}"))?;
        if !line.is_empty() {
            write_stream_response(stdout, &active.request.id, &line)?;
        }
    }
    Ok(())
}

fn read_stream(mut stream: impl Read) -> io::Result<CapturedStream> {
    let mut bytes = Vec::new();
    let mut exceeded = false;
    let mut buffer = [0_u8; 8192];
    loop {
        let count = stream.read(&mut buffer)?;
        if count == 0 {
            break;
        }
        if bytes.len() < MAX_STREAM_BYTES {
            let remaining = MAX_STREAM_BYTES - bytes.len();
            bytes.extend_from_slice(&buffer[..count.min(remaining)]);
        }
        if bytes.len() >= MAX_STREAM_BYTES && count > MAX_STREAM_BYTES.saturating_sub(bytes.len()) {
            exceeded = true;
        }
    }
    Ok(CapturedStream {
        bytes,
        exceeded,
        protocol_error: false,
        terminal_seen: false,
    })
}

fn write_finished_response(
    stdout: &mut impl Write,
    active: ActiveRequest,
    status: ExitStatus,
) -> Result<(), String> {
    let ActiveRequest {
        request,
        mut child,
        cancel_file,
        stdout: stdout_thread,
        stdout_lines,
        stderr: stderr_thread,
    } = active;
    let _ = child.wait();
    let captured_stdout = stdout_thread
        .join()
        .map_err(|_| "backend stdout reader panicked".to_string())?
        .map_err(|error| format!("read backend stdout: {error}"))?;
    let captured_stderr = stderr_thread
        .join()
        .map_err(|_| "backend stderr reader panicked".to_string())?
        .map_err(|error| format!("read backend stderr: {error}"))?;
    let _ = std::fs::remove_file(cancel_file);

    while let Ok(line) = stdout_lines.try_recv() {
        let line = line.map_err(|error| format!("read backend stdout: {error}"))?;
        if !line.is_empty() {
            write_stream_response(stdout, &request.id, &line)?;
        }
    }

    if captured_stdout.exceeded || captured_stderr.exceeded {
        return write_error_response(
            stdout,
            request.id,
            "output_limit_exceeded",
            "backend operation output exceeded the worker limit".into(),
        );
    }
    if captured_stdout.protocol_error {
        return write_error_response(
            stdout,
            request.id,
            "protocol_error",
            "directory metrics emitted an invalid event sequence".into(),
        );
    }
    if status.success() {
        let payload = completion_payload(&request, &captured_stdout);
        return write_response(
            stdout,
            Response {
                version: 1,
                id: request.id,
                ok: true,
                payload,
                error_code: None,
                error: None,
                stream: None,
                done: Some(true),
            },
        );
    }

    write_error_response(
        stdout,
        request.id,
        "backend_exit",
        String::from_utf8_lossy(&captured_stderr.bytes)
            .trim()
            .to_string(),
    )
}

fn completion_payload(request: &Request, captured_stdout: &CapturedStream) -> Option<String> {
    if request.arguments.first().map(String::as_str) != Some("directory-metrics")
        || captured_stdout.protocol_error
        || !captured_stdout.terminal_seen
    {
        return None;
    }
    Some(String::from_utf8_lossy(&captured_stdout.bytes).into_owned())
}

fn terminate_request(active: ActiveRequest) {
    let ActiveRequest {
        cancel_file,
        mut child,
        stdout,
        stdout_lines: _,
        stderr,
        ..
    } = active;
    let _ = std::fs::write(&cancel_file, b"cancelled\n");
    let mut exited = false;
    for _ in 0..COOPERATIVE_CANCEL_WAIT_STEPS {
        match child.try_wait() {
            Ok(Some(_)) => {
                exited = true;
                break;
            }
            Ok(None) => thread::sleep(Duration::from_millis(10)),
            Err(_) => break,
        }
    }
    if !exited {
        let _ = child.kill();
    }
    let _ = child.wait();
    let _ = stdout.join();
    let _ = stderr.join();
    let _ = std::fs::remove_file(cancel_file);
}

fn cancellation_marker_path() -> PathBuf {
    let serial = CANCEL_MARKER_COUNTER.fetch_add(1, Ordering::Relaxed);
    std::env::temp_dir().join(format!(
        ".astrea-explorer-cancel-{}-{serial}",
        std::process::id()
    ))
}

fn write_cancelled_response(stdout: &mut impl Write, id: String) -> Result<(), String> {
    write_error_response(stdout, id, "cancelled", "request cancelled".into())
}

fn write_error_response(
    stdout: &mut impl Write,
    id: String,
    code: &str,
    message: String,
) -> Result<(), String> {
    write_response(
        stdout,
        Response {
            version: 1,
            id,
            ok: false,
            payload: None,
            error_code: Some(code.into()),
            error: Some(message),
            stream: None,
            done: Some(true),
        },
    )
}

fn write_stream_response(stdout: &mut impl Write, id: &str, payload: &[u8]) -> Result<(), String> {
    write_response(
        stdout,
        Response {
            version: 1,
            id: id.to_string(),
            ok: true,
            payload: Some(String::from_utf8_lossy(payload).into_owned()),
            error_code: None,
            error: None,
            stream: Some(true),
            done: None,
        },
    )
}

fn write_response(stdout: &mut impl Write, response: Response) -> Result<(), String> {
    serde_json::to_writer(&mut *stdout, &response).map_err(|error| error.to_string())?;
    stdout.write_all(b"\n").map_err(|error| error.to_string())?;
    stdout.flush().map_err(|error| error.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Cursor;
    use std::sync::Arc;
    use std::sync::atomic::AtomicUsize;

    struct CountingReader {
        bytes: Vec<u8>,
        offset: usize,
        bytes_read: Arc<AtomicUsize>,
    }

    impl Read for CountingReader {
        fn read(&mut self, buffer: &mut [u8]) -> io::Result<usize> {
            let remaining = self.bytes.len().saturating_sub(self.offset);
            let count = remaining.min(buffer.len());
            if count == 0 {
                return Ok(0);
            }
            buffer[..count].copy_from_slice(&self.bytes[self.offset..self.offset + count]);
            self.offset += count;
            self.bytes_read.fetch_add(count, Ordering::Relaxed);
            Ok(count)
        }
    }

    #[test]
    fn directory_metrics_progress_does_not_fill_terminal_capture() {
        let progress_line = br#"{"event":"progress","operation":"directory-metrics","state":"running","bytes":1,"fileCount":1,"directoryCount":0,"unreadableCount":0,"scannedEntryCount":1}
"#;
        let terminal_line = br#"{"event":"result","operation":"directory-metrics","state":"success","bytes":42,"fileCount":42,"directoryCount":0,"unreadableCount":0,"scannedEntryCount":42}
"#;
        let mut input = Vec::new();
        for _ in 0..40_000 {
            input.extend_from_slice(progress_line);
        }
        input.extend_from_slice(terminal_line);
        assert!(input.len() > MAX_STREAM_BYTES);

        let (sender, receiver) = mpsc::sync_channel(DIRECTORY_METRICS_PROGRESS_QUEUE_CAPACITY);
        let captured = read_stream_lines(
            Cursor::new(input),
            StreamSender::Bounded(sender),
            StreamCapture::LastLine,
        )
        .expect("read streamed directory metrics");

        assert!(!captured.exceeded);
        assert_eq!(captured.bytes, terminal_line);
        assert!(receiver.iter().count() <= DIRECTORY_METRICS_PROGRESS_QUEUE_CAPACITY);
    }

    #[test]
    fn directory_metrics_event_line_is_rejected_before_unbounded_allocation() {
        let bytes_read = Arc::new(AtomicUsize::new(0));
        let reader = CountingReader {
            bytes: vec![b'x'; MAX_DIRECTORY_METRICS_EVENT_BYTES * 4],
            offset: 0,
            bytes_read: Arc::clone(&bytes_read),
        };
        let (sender, _receiver) = mpsc::sync_channel(DIRECTORY_METRICS_PROGRESS_QUEUE_CAPACITY);

        let captured = read_stream_lines(
            reader,
            StreamSender::Bounded(sender),
            StreamCapture::LastLine,
        )
        .expect("bounded event reader should return a capture result");

        assert!(captured.exceeded);
        assert!(bytes_read.load(Ordering::Relaxed) <= MAX_DIRECTORY_METRICS_EVENT_BYTES + 8192);
    }

    #[test]
    fn directory_metrics_completion_uses_captured_terminal_record() {
        let request = Request {
            version: 1,
            id: "metrics".into(),
            arguments: vec!["directory-metrics".into()],
            stdin_payload: None,
        };
        let captured_stdout = CapturedStream {
            bytes: br#"{"event":"result","operation":"directory-metrics","state":"success"}
"#
            .to_vec(),
            exceeded: false,
            protocol_error: false,
            terminal_seen: true,
        };

        assert_eq!(
            completion_payload(&request, &captured_stdout),
            Some(String::from_utf8_lossy(&captured_stdout.bytes).into_owned())
        );
    }

    #[test]
    fn ordinary_completion_does_not_retain_stdout_as_payload() {
        let request = Request {
            version: 1,
            id: "list".into(),
            arguments: vec!["list".into()],
            stdin_payload: None,
        };
        let captured_stdout = CapturedStream {
            bytes: b"ordinary output".to_vec(),
            exceeded: false,
            protocol_error: false,
            terminal_seen: false,
        };

        assert_eq!(completion_payload(&request, &captured_stdout), None);
    }

    #[test]
    fn directory_metrics_missing_terminal_has_no_completion_payload() {
        let progress_line =
            br#"{"event":"progress","operation":"directory-metrics","state":"running"}
"#;
        let (sender, _receiver) = mpsc::sync_channel(DIRECTORY_METRICS_PROGRESS_QUEUE_CAPACITY);
        let captured = read_stream_lines(
            Cursor::new(progress_line),
            StreamSender::Bounded(sender),
            StreamCapture::LastLine,
        )
        .expect("read progress-only metrics output");
        let request = Request {
            version: 1,
            id: "metrics".into(),
            arguments: vec!["directory-metrics".into()],
            stdin_payload: None,
        };

        assert!(captured.bytes.is_empty());
        assert_eq!(completion_payload(&request, &captured), None);
    }

    #[test]
    fn directory_metrics_malformed_or_late_events_mark_capture_invalid() {
        let terminal_line =
            br#"{"event":"result","operation":"directory-metrics","state":"success"}
"#;
        let late_line = br#"{"event":"progress","operation":"directory-metrics","state":"running"}
"#;
        let mut input = b"not json\n".to_vec();
        input.extend_from_slice(terminal_line);
        input.extend_from_slice(late_line);
        let (sender, _receiver) = mpsc::sync_channel(DIRECTORY_METRICS_PROGRESS_QUEUE_CAPACITY);

        let captured = read_stream_lines(
            Cursor::new(input),
            StreamSender::Bounded(sender),
            StreamCapture::LastLine,
        )
        .expect("read invalid metrics output");

        assert!(captured.protocol_error);
    }
}
