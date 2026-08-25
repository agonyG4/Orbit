use astrea_launch::{
    config_path, default_config_text, ensure_default_config, history_path, launchd_socket_path,
    load_config, parse_cli_request, read_history, run_launch_via_daemon, serve_launchd,
};

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let result = match args.first().map(String::as_str) {
        Some("daemon") | Some("launchd") if args.len() == 1 => {
            serve_launchd().map(|_| empty_record())
        }
        Some("doctor") if args.len() == 1 => {
            doctor();
            Ok(empty_record())
        }
        Some("history") if args.len() == 1 => {
            for line in read_history(40) {
                println!("{line}");
            }
            Ok(empty_record())
        }
        Some("--desktop") | Some("--command") | Some("--argv-json") | Some("--file")
        | Some("--url") | Some("--steam") => {
            parse_cli_request(&args).and_then(run_launch_via_daemon)
        }
        _ => {
            usage();
            Err("invalid command".into())
        }
    };

    match result {
        Ok(record) if !record.kind.is_empty() => {
            println!(
                "{}",
                serde_json::to_string(&record).unwrap_or_else(|_| "{\"status\":\"ok\"}".into())
            );
        }
        Ok(_) => {}
        Err(err) => {
            eprintln!("astrea-launch: {err}");
            std::process::exit(1);
        }
    }
}

fn doctor() {
    let (config, warning) = load_config();
    println!("config: {}", config_path().display());
    if config_path().exists() {
        println!("config_status: present");
    } else {
        println!("config_status: missing, default follows");
        print!("{}", default_config_text());
        if let Err(err) = ensure_default_config() {
            println!("config_write: {err}");
        }
    }
    if let Some(warning) = warning {
        println!("config_warning: {warning}");
    }
    println!("history: {}", history_path().display());
    println!("launchd_socket: {}", launchd_socket_path().display());
    println!(
        "launchd_socket_status: {}",
        if launchd_socket_path().exists() {
            "present"
        } else {
            "missing"
        }
    );
    println!("latency_enabled: {}", config.latency.enabled);
    for socket in &config.latency.socket_paths {
        println!(
            "latency_socket: {} {}",
            socket,
            if std::path::Path::new(socket).exists() {
                "present"
            } else {
                "missing"
            }
        );
    }
    println!("rules: {}", config.rules.len());
}

fn usage() {
    eprintln!("Usage:");
    eprintln!("  astrea-launch --desktop <desktop-id> [--file <path>]... [--url <uri>]...");
    eprintln!("  astrea-launch --command <cmd>");
    eprintln!("  astrea-launch --argv-json '[\"program\",\"arg\"]'");
    eprintln!("  astrea-launch --file <path>");
    eprintln!("  astrea-launch --url <url>");
    eprintln!("  astrea-launch --steam <steam-uri>");
    eprintln!("  astrea-launch daemon");
    eprintln!("  astrea-launch doctor");
    eprintln!("  astrea-launch history");
}

fn empty_record() -> astrea_launch::LaunchRecord {
    astrea_launch::LaunchRecord {
        timestamp_ms: 0,
        kind: String::new(),
        target: String::new(),
        argv: Vec::new(),
        pid: None,
        status: String::new(),
        detail: String::new(),
    }
}
