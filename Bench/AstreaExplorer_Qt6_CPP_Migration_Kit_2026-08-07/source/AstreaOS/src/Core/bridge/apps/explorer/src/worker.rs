use std::env;
use std::io::{self, BufRead, Write};
use std::process::Command;

use serde::{Deserialize, Serialize};

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

pub fn run() -> Result<(), String> {
    let executable =
        env::current_exe().map_err(|error| format!("resolve worker executable: {error}"))?;
    let stdin = io::stdin();
    let mut stdout = io::BufWriter::new(io::stdout());
    for line in stdin.lock().lines() {
        let line = line.map_err(|error| format!("read worker request: {error}"))?;
        if line.len() > 4 * 1024 * 1024 {
            continue;
        }
        let request: Request = match serde_json::from_str(&line) {
            Ok(request) => request,
            Err(error) => {
                write_response(
                    &mut stdout,
                    Response {
                        version: 1,
                        id: String::new(),
                        ok: false,
                        payload: None,
                        error_code: Some("protocol_error".into()),
                        error: Some(error.to_string()),
                    },
                )?;
                continue;
            }
        };
        if request.version != 1 || request.arguments.is_empty() {
            write_response(
                &mut stdout,
                Response {
                    version: 1,
                    id: request.id,
                    ok: false,
                    payload: None,
                    error_code: Some("protocol_version".into()),
                    error: Some("unsupported or empty backend request".into()),
                },
            )?;
            continue;
        }

        let output = Command::new(&executable)
            .args(&request.arguments)
            .output()
            .map_err(|error| format!("run backend operation: {error}"))?;
        if output.status.success() {
            write_response(
                &mut stdout,
                Response {
                    version: 1,
                    id: request.id,
                    ok: true,
                    payload: Some(String::from_utf8_lossy(&output.stdout).into_owned()),
                    error_code: None,
                    error: None,
                },
            )?;
        } else {
            write_response(
                &mut stdout,
                Response {
                    version: 1,
                    id: request.id,
                    ok: false,
                    payload: None,
                    error_code: Some("backend_exit".into()),
                    error: Some(String::from_utf8_lossy(&output.stderr).trim().to_string()),
                },
            )?;
        }
    }
    Ok(())
}

fn write_response(stdout: &mut impl Write, response: Response) -> Result<(), String> {
    serde_json::to_writer(&mut *stdout, &response).map_err(|error| error.to_string())?;
    stdout.write_all(b"\n").map_err(|error| error.to_string())?;
    stdout.flush().map_err(|error| error.to_string())
}
