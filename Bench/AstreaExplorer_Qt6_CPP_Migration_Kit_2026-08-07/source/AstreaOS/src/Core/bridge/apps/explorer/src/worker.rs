use std::collections::VecDeque;
use std::env;
use std::io::{self, BufRead, BufWriter, Read, Write};
use std::process::{Child, Command, ExitStatus, Stdio};
use std::sync::mpsc::{self, RecvTimeoutError, Sender};
use std::thread::{self, JoinHandle};
use std::time::Duration;

use serde::{Deserialize, Serialize};

const MAX_REQUEST_LINE_BYTES: usize = 4 * 1024 * 1024;
const MAX_STREAM_BYTES: usize = 3 * 1024 * 1024;
const CANCEL_OPERATION: &str = "cancel";

#[derive(Debug, Deserialize)]
struct Request {
    version: u32,
    id: String,
    arguments: Vec<String>,
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
}

enum Incoming {
    Request(Request),
    ProtocolError(String),
}

struct CapturedStream {
    bytes: Vec<u8>,
    exceeded: bool,
}

struct ActiveRequest {
    request: Request,
    child: Child,
    stdout: JoinHandle<io::Result<CapturedStream>>,
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
    let mut child = match Command::new(executable)
        .args(&request.arguments)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
    {
        Ok(child) => child,
        Err(error) => return Err((request, format!("run backend operation: {error}"))),
    };

    let stdout = match child.stdout.take() {
        Some(stream) => thread::spawn(move || read_stream(stream)),
        None => {
            let _ = child.kill();
            let _ = child.wait();
            return Err((request, "backend stdout pipe was unavailable".into()));
        }
    };
    let stderr = match child.stderr.take() {
        Some(stream) => thread::spawn(move || read_stream(stream)),
        None => {
            let _ = child.kill();
            let _ = child.wait();
            let _ = stdout.join();
            return Err((request, "backend stderr pipe was unavailable".into()));
        }
    };

    Ok(ActiveRequest {
        request,
        child,
        stdout,
        stderr,
    })
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
    Ok(CapturedStream { bytes, exceeded })
}

fn write_finished_response(
    stdout: &mut impl Write,
    active: ActiveRequest,
    status: ExitStatus,
) -> Result<(), String> {
    let ActiveRequest {
        request,
        mut child,
        stdout: stdout_thread,
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

    if captured_stdout.exceeded || captured_stderr.exceeded {
        return write_error_response(
            stdout,
            request.id,
            "output_limit_exceeded",
            "backend operation output exceeded the worker limit".into(),
        );
    }
    if status.success() {
        return write_response(
            stdout,
            Response {
                version: 1,
                id: request.id,
                ok: true,
                payload: Some(String::from_utf8_lossy(&captured_stdout.bytes).into_owned()),
                error_code: None,
                error: None,
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

fn terminate_request(active: ActiveRequest) {
    let ActiveRequest {
        mut child,
        stdout,
        stderr,
        ..
    } = active;
    let _ = child.kill();
    let _ = child.wait();
    let _ = stdout.join();
    let _ = stderr.join();
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
        },
    )
}

fn write_response(stdout: &mut impl Write, response: Response) -> Result<(), String> {
    serde_json::to_writer(&mut *stdout, &response).map_err(|error| error.to_string())?;
    stdout.write_all(b"\n").map_err(|error| error.to_string())?;
    stdout.flush().map_err(|error| error.to_string())
}
