use astrea_screentime::{
    DEFAULT_EVENT_RETENTION_LINES, DEFAULT_INTERVAL_SECONDS, MonitorOptions, Paths,
    RUST_SERVICE_NAME, atomic_write_json, compact_events, doctor_json, empty_state,
    install_signal_handlers, privacy_auth_json, remove_app, run_monitor, service_action,
    service_status, set_hidden_app, settings_json, snapshot_json, status_json,
};
use serde_json::{Value, json, to_string_pretty};
use std::env;
use std::fs;
use std::process::ExitCode;

fn main() -> ExitCode {
    match run() {
        Ok(code) => code,
        Err(error) => {
            eprintln!("{error}");
            ExitCode::FAILURE
        }
    }
}

fn run() -> astrea_screentime::Result<ExitCode> {
    let mut args = env::args().skip(1);
    let command = args.next().unwrap_or_else(|| "status".to_string());
    match command.as_str() {
        "monitor" => {
            install_signal_handlers();
            let interval = parse_interval(args.collect());
            run_monitor(MonitorOptions {
                interval_seconds: interval,
                paths: Paths::default_for_project(),
            })?;
            Ok(ExitCode::SUCCESS)
        }
        "snapshot" => {
            let options = parse_snapshot_args(args.collect());
            let snapshot = snapshot_json(
                &Paths::default_for_project(),
                options.day.as_deref(),
                options.week.as_deref(),
                options.limit,
            );
            println!("{}", to_string_pretty(&snapshot)?);
            Ok(ExitCode::SUCCESS)
        }
        "service" => print_service(args.collect()),
        "auth" => print_auth(args.collect()),
        "doctor" => print_doctor(args.collect()),
        "maintenance" => print_maintenance(args.collect()),
        "settings" => print_settings(args.collect()),
        "status" => {
            let status = status_json(&Paths::default_for_project());
            println!("{}", to_string_pretty(&status)?);
            Ok(ExitCode::SUCCESS)
        }
        "report" => {
            let options = parse_snapshot_args(args.collect());
            print_report(
                options.day.as_deref(),
                options.week.as_deref(),
                options.limit,
            )?;
            Ok(ExitCode::SUCCESS)
        }
        "path" => {
            let paths = Paths::default_for_project();
            println!("{}", paths.state_path.display());
            println!("{}", paths.events_path.display());
            println!("{}", paths.rules_path.display());
            println!("{}", paths.settings_path.display());
            Ok(ExitCode::SUCCESS)
        }
        "reset" => {
            let paths = Paths::default_for_project();
            atomic_write_json(&paths.state_path, &empty_state())?;
            if paths.events_path.exists() {
                fs::remove_file(paths.events_path)?;
            }
            Ok(ExitCode::SUCCESS)
        }
        _ => {
            eprintln!(
                "Usage: astrea-screentime [monitor [--interval SECONDS]|snapshot|service|auth|doctor|maintenance|settings|status|report|path|reset]"
            );
            Ok(ExitCode::FAILURE)
        }
    }
}

fn parse_interval(args: Vec<String>) -> f64 {
    let mut interval = DEFAULT_INTERVAL_SECONDS;
    let mut index = 0;
    while index < args.len() {
        if args[index] == "--interval" {
            if let Some(value) = args.get(index + 1).and_then(|raw| raw.parse::<f64>().ok()) {
                interval = value;
            }
            index += 2;
            continue;
        }
        index += 1;
    }
    interval.max(1.0)
}

struct SnapshotOptions {
    day: Option<String>,
    week: Option<String>,
    limit: usize,
}

fn parse_snapshot_args(args: Vec<String>) -> SnapshotOptions {
    let mut options = SnapshotOptions {
        day: None,
        week: None,
        limit: 12,
    };
    let mut index = 0;
    while index < args.len() {
        match args[index].as_str() {
            "--day" => {
                options.day = args.get(index + 1).cloned();
                index += 2;
            }
            "--week" => {
                options.week = args.get(index + 1).cloned();
                index += 2;
            }
            "--limit" => {
                if let Some(limit) = args
                    .get(index + 1)
                    .and_then(|raw| raw.parse::<usize>().ok())
                {
                    options.limit = limit.max(1);
                }
                index += 2;
            }
            "--json" => {
                index += 1;
            }
            _ => {
                index += 1;
            }
        }
    }
    options
}

fn parse_service_args(args: Vec<String>) -> (String, bool) {
    let mut action = "status".to_string();
    let mut as_json = false;
    for arg in args {
        if arg == "--json" {
            as_json = true;
        } else if !arg.starts_with("--") {
            action = arg;
        }
    }
    (action, as_json)
}

fn parse_settings_args(args: Vec<String>) -> (String, String, bool) {
    let mut action = "status".to_string();
    let mut app_id = String::new();
    let mut as_json = false;
    for arg in args {
        if arg == "--json" {
            as_json = true;
        } else if !arg.starts_with("--") && action == "status" {
            action = arg;
        } else if !arg.starts_with("--") && app_id.is_empty() {
            app_id = arg;
        }
    }
    (action, app_id, as_json)
}

fn parse_maintenance_args(args: Vec<String>) -> (String, usize, bool) {
    let mut action = "status".to_string();
    let mut max_lines = DEFAULT_EVENT_RETENTION_LINES;
    let mut as_json = false;
    let mut index = 0;
    while index < args.len() {
        match args[index].as_str() {
            "--json" => {
                as_json = true;
                index += 1;
            }
            "--max-lines" => {
                if let Some(value) = args
                    .get(index + 1)
                    .and_then(|raw| raw.parse::<usize>().ok())
                {
                    max_lines = value.max(1);
                }
                index += 2;
            }
            value if !value.starts_with("--") && action == "status" => {
                action = value.to_string();
                index += 1;
            }
            _ => index += 1,
        }
    }
    (action, max_lines, as_json)
}

fn print_settings(args: Vec<String>) -> astrea_screentime::Result<ExitCode> {
    let paths = Paths::default_for_project();
    let (action, app_id, as_json) = parse_settings_args(args);
    let result = match action.as_str() {
        "status" => settings_json(&paths),
        "hide-app" => settings_action_result(&action, set_hidden_app(&paths, &app_id, true)?),
        "show-app" => settings_action_result(&action, set_hidden_app(&paths, &app_id, false)?),
        "remove-app" => settings_action_result(&action, remove_app(&paths, &app_id)?),
        _ => {
            eprintln!("Unsupported settings action: {action}");
            return Ok(ExitCode::FAILURE);
        }
    };
    if as_json {
        println!("{}", to_string_pretty(&result)?);
    } else {
        println!("{}", to_string_pretty(&result)?);
    }
    Ok(ExitCode::SUCCESS)
}

fn settings_action_result(action: &str, settings: Value) -> Value {
    json!({
        "ok": true,
        "action": action,
        "settings": settings,
    })
}

fn print_auth(args: Vec<String>) -> astrea_screentime::Result<ExitCode> {
    let as_json = args.iter().any(|arg| arg == "--json");
    let result = privacy_auth_json()?;
    if as_json {
        println!("{}", to_string_pretty(&result)?);
    } else {
        println!("Authenticated");
    }
    Ok(ExitCode::SUCCESS)
}

fn print_doctor(args: Vec<String>) -> astrea_screentime::Result<ExitCode> {
    let as_json = args.iter().any(|arg| arg == "--json");
    let result = doctor_json(&Paths::default_for_project());
    if as_json {
        println!("{}", to_string_pretty(&result)?);
    } else {
        println!(
            "Doctor: {}",
            if result.get("ok").and_then(Value::as_bool).unwrap_or(false) {
                "ok"
            } else {
                "issues found"
            }
        );
    }
    Ok(ExitCode::SUCCESS)
}

fn print_maintenance(args: Vec<String>) -> astrea_screentime::Result<ExitCode> {
    let paths = Paths::default_for_project();
    let (action, max_lines, as_json) = parse_maintenance_args(args);
    let result = match action.as_str() {
        "compact-events" => compact_events(&paths, max_lines)?,
        _ => {
            eprintln!("Unsupported maintenance action: {action}");
            return Ok(ExitCode::FAILURE);
        }
    };
    if as_json {
        println!("{}", to_string_pretty(&result)?);
    } else {
        println!("{}", to_string_pretty(&result)?);
    }
    Ok(ExitCode::SUCCESS)
}

fn print_service(args: Vec<String>) -> astrea_screentime::Result<ExitCode> {
    let (action, as_json) = parse_service_args(args);
    match service_action(&action) {
        Ok(result) => {
            if as_json {
                println!("{}", to_string_pretty(&result)?);
            } else {
                println!(
                    "{}: {}",
                    result
                        .get("unit")
                        .and_then(Value::as_str)
                        .unwrap_or(RUST_SERVICE_NAME),
                    result
                        .get("display")
                        .and_then(|display| display.get("state"))
                        .and_then(Value::as_str)
                        .unwrap_or("")
                );
            }
            Ok(ExitCode::SUCCESS)
        }
        Err(error) => {
            let mut result = service_status(RUST_SERVICE_NAME);
            if let Some(map) = result.as_object_mut() {
                map.insert("ok".to_string(), json!(false));
                map.insert("action".to_string(), Value::String(action.clone()));
                map.insert("last_error".to_string(), Value::String(error.to_string()));
            }
            if as_json {
                println!("{}", to_string_pretty(&result)?);
            } else {
                eprintln!("{error}");
            }
            Ok(ExitCode::FAILURE)
        }
    }
}

fn print_report(
    day: Option<&str>,
    week: Option<&str>,
    limit: usize,
) -> astrea_screentime::Result<()> {
    let snapshot = snapshot_json(&Paths::default_for_project(), day, week, limit);
    let root = snapshot.get("day").unwrap_or(&Value::Null);
    let title = snapshot
        .get("display")
        .and_then(|display| display.get("selected_day"))
        .and_then(Value::as_str)
        .unwrap_or("ScreenTime");
    println!("{title}");
    println!(
        "Total: {}",
        root.get("duration").and_then(Value::as_str).unwrap_or("0s")
    );
    println!("\nCategories");
    for row in root
        .get("top_categories")
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
        .take(limit)
    {
        let label = row
            .get("label")
            .or_else(|| row.get("id"))
            .and_then(Value::as_str)
            .unwrap_or("");
        let duration = row.get("duration").and_then(Value::as_str).unwrap_or("0s");
        println!("  {label}: {duration}");
    }
    println!("\nApps");
    for row in root
        .get("top_apps")
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
        .take(limit)
    {
        let app = row.get("id").and_then(Value::as_str).unwrap_or("");
        let duration = row.get("duration").and_then(Value::as_str).unwrap_or("0s");
        println!("  {app}: {duration}");
    }
    Ok(())
}
