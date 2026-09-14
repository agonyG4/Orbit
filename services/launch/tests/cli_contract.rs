use std::fs;
use std::os::unix::fs::PermissionsExt;
use std::process::Command;
use std::process::Stdio;
use std::thread;
use std::time::Duration;

#[test]
fn cli_desktop_launch_forwards_targets_to_final_argv() {
    let root =
        std::env::temp_dir().join(format!("astrea-launch-cli-contract-{}", std::process::id()));
    let applications = root.join("data/applications");
    let config = root.join("config");
    let state = root.join("state");
    let recorder = root.join("record-argv");
    let output = root.join("argv.txt");
    fs::create_dir_all(&applications).unwrap();
    fs::create_dir_all(&config).unwrap();
    fs::create_dir_all(config.join("AstreaOS/system")).unwrap();
    fs::create_dir_all(&state).unwrap();
    fs::write(
        &recorder,
        "#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$ASTREA_RECORDER_OUTPUT\"\n",
    )
    .unwrap();
    fs::set_permissions(&recorder, fs::Permissions::from_mode(0o755)).unwrap();
    fs::write(
        applications.join("org.example.Editor.desktop"),
        format!(
            "[Desktop Entry]\nType=Application\nName=Editor\nExec={} %F\n",
            recorder.display()
        ),
    )
    .unwrap();
    fs::write(
        config.join("AstreaOS/system/launch.json"),
        "{\"isolate_launches\":false,\"latency\":{\"enabled\":false}}",
    )
    .unwrap();

    let target_one = root.join("one file.txt");
    let target_two = root.join("two.txt");
    let result = Command::new(env!("CARGO_BIN_EXE_astrea-launch"))
        .args([
            "--desktop",
            "org.example.Editor.desktop",
            "--file",
            target_one.to_str().unwrap(),
            "--file",
            target_two.to_str().unwrap(),
        ])
        .env("HOME", &root)
        .env("XDG_DATA_HOME", root.join("data"))
        .env("XDG_CONFIG_HOME", &config)
        .env("XDG_STATE_HOME", &state)
        .env("XDG_RUNTIME_DIR", root.join("runtime"))
        .env("ASTREA_RECORDER_OUTPUT", &output)
        .env("PATH", &root)
        .output()
        .expect("run astrea-launch CLI");
    assert!(
        result.status.success(),
        "stderr: {}",
        String::from_utf8_lossy(&result.stderr)
    );

    for _ in 0..40 {
        if output.is_file() {
            break;
        }
        thread::sleep(Duration::from_millis(25));
    }
    let recorded = fs::read_to_string(&output).expect("recorder output");
    assert_eq!(
        recorded.lines().collect::<Vec<_>>(),
        vec![target_one.to_str().unwrap(), target_two.to_str().unwrap(),]
    );
    let _ = fs::remove_dir_all(root);
}

#[test]
fn cli_windows_launch_dispatches_through_umu_with_safe_default_identity() {
    let root = std::env::temp_dir().join(format!(
        "astrea-launch-windows-contract-{}",
        std::process::id()
    ));
    let bin = root.join("bin");
    let config = root.join("config/AstreaOS/gaming");
    let launch_config = root.join("config/AstreaOS/system");
    let data = root.join("data");
    let state = root.join("state");
    let output = root.join("launch.txt");
    let target = root.join("Games/game.exe");
    fs::create_dir_all(&bin).unwrap();
    fs::create_dir_all(&config).unwrap();
    fs::create_dir_all(&launch_config).unwrap();
    fs::create_dir_all(&data).unwrap();
    fs::create_dir_all(&state).unwrap();
    fs::create_dir_all(target.parent().unwrap()).unwrap();

    let mut pe = vec![0u8; 0x90];
    pe[0..2].copy_from_slice(b"MZ");
    pe[0x3c..0x40].copy_from_slice(&(0x80u32).to_le_bytes());
    pe[0x80..0x84].copy_from_slice(b"PE\0\0");
    pe[0x84..0x86].copy_from_slice(&(0x8664u16).to_le_bytes());
    fs::write(&target, pe).unwrap();

    let umu = bin.join("umu-run");
    fs::write(
        &umu,
        "#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$ASTREA_WINDOWS_RECORD\"\nprintf 'UMU_ID=%s\\n' \"$UMU_ID\" >> \"$ASTREA_WINDOWS_RECORD\"\nprintf 'GAMEID=%s\\n' \"$GAMEID\" >> \"$ASTREA_WINDOWS_RECORD\"\nprintf 'SECRET_VALUE=%s\\n' \"$SECRET_VALUE\" >> \"$ASTREA_WINDOWS_RECORD\"\n",
    )
    .unwrap();
    fs::set_permissions(&umu, fs::Permissions::from_mode(0o755)).unwrap();
    fs::write(
        config.join("compatibility.json"),
        r#"{"runner":"proton","use_proton_profile":false,"gamemode":false,"mangohud":false,"gamescope":false,"extra_env":"SECRET_VALUE=must-not-be-argv","extra_prefix":""}"#,
    )
    .unwrap();
    fs::write(
        launch_config.join("launch.json"),
        r#"{"isolate_launches":false,"latency":{"enabled":false}}"#,
    )
    .unwrap();

    let result = Command::new(env!("CARGO_BIN_EXE_astrea-launch"))
        .args(["--windows", target.to_str().unwrap()])
        .env("HOME", &root)
        .env("XDG_CONFIG_HOME", root.join("config"))
        .env("XDG_DATA_HOME", &data)
        .env("XDG_STATE_HOME", &state)
        .env("XDG_RUNTIME_DIR", root.join("runtime"))
        .env("ASTREA_WINDOWS_RECORD", &output)
        .env("PATH", &bin)
        .output()
        .expect("run astrea-launch Windows CLI");
    assert!(
        result.status.success(),
        "stderr: {}",
        String::from_utf8_lossy(&result.stderr)
    );

    for _ in 0..40 {
        if output.is_file() {
            break;
        }
        thread::sleep(Duration::from_millis(25));
    }
    let recorded = fs::read_to_string(&output).unwrap();
    assert!(recorded.contains(target.to_str().unwrap()));
    assert!(recorded.contains("UMU_ID=umu-default"));
    assert!(recorded.contains("GAMEID=umu-default"));
    assert!(recorded.contains("SECRET_VALUE=must-not-be-argv"));

    let history = fs::read_to_string(state.join("Astrea/launch/history.jsonl")).unwrap();
    assert!(!history.contains("SECRET_VALUE"));
    assert!(!history.contains("UMU_ID"));
    assert!(!history.contains("GAMEID"));
    let _ = fs::remove_dir_all(root);
}

#[test]
fn daemon_path_reads_compatibility_json_for_each_windows_launch() {
    let root = std::env::temp_dir().join(format!(
        "astrea-launch-windows-refresh-{}",
        std::process::id()
    ));
    let bin = root.join("bin");
    let config = root.join("config/AstreaOS/gaming");
    let launch_config = root.join("config/AstreaOS/system");
    let data = root.join("data");
    let state = root.join("state");
    let output = root.join("launch.txt");
    let target = root.join("Games/game.exe");
    fs::create_dir_all(&bin).unwrap();
    fs::create_dir_all(&config).unwrap();
    fs::create_dir_all(&launch_config).unwrap();
    fs::create_dir_all(&data).unwrap();
    fs::create_dir_all(&state).unwrap();
    fs::create_dir_all(target.parent().unwrap()).unwrap();
    fs::write(&target, {
        let mut pe = vec![0u8; 0x90];
        pe[0..2].copy_from_slice(b"MZ");
        pe[0x3c..0x40].copy_from_slice(&(0x80u32).to_le_bytes());
        pe[0x80..0x84].copy_from_slice(b"PE\0\0");
        pe[0x84..0x86].copy_from_slice(&(0x8664u16).to_le_bytes());
        pe
    })
    .unwrap();
    fs::write(
        bin.join("umu-run"),
        "#!/bin/sh\nprintf 'umu\\n' >> \"$ASTREA_WINDOWS_RUNNER_RECORD\"\n",
    )
    .unwrap();
    fs::set_permissions(bin.join("umu-run"), fs::Permissions::from_mode(0o755)).unwrap();
    fs::write(
        bin.join("wine"),
        "#!/bin/sh\nprintf 'wine\\n' >> \"$ASTREA_WINDOWS_RUNNER_RECORD\"\n",
    )
    .unwrap();
    fs::set_permissions(bin.join("wine"), fs::Permissions::from_mode(0o755)).unwrap();
    fs::write(
        launch_config.join("launch.json"),
        r#"{"isolate_launches":false,"latency":{"enabled":false}}"#,
    )
    .unwrap();
    fs::write(
        config.join("compatibility.json"),
        r#"{"runner":"proton","use_proton_profile":false,"gamemode":false}"#,
    )
    .unwrap();

    let mut daemon = Command::new(env!("CARGO_BIN_EXE_astrea-launch"))
        .arg("daemon")
        .env("HOME", &root)
        .env("XDG_CONFIG_HOME", root.join("config"))
        .env("XDG_DATA_HOME", &data)
        .env("XDG_STATE_HOME", &state)
        .env("XDG_RUNTIME_DIR", root.join("runtime"))
        .env("ASTREA_WINDOWS_RUNNER_RECORD", &output)
        .env("PATH", &bin)
        .stderr(Stdio::piped())
        .spawn()
        .expect("start astrea-launch daemon");
    let socket = root.join("runtime/Astrea/astrea-launchd.sock");
    for _ in 0..40 {
        if socket.exists() {
            break;
        }
        thread::sleep(Duration::from_millis(25));
    }
    if !socket.exists() {
        let failure = daemon.wait_with_output().expect("collect daemon failure");
        panic!(
            "daemon did not create socket: {}",
            String::from_utf8_lossy(&failure.stderr)
        );
    }

    let run = || {
        Command::new(env!("CARGO_BIN_EXE_astrea-launch"))
            .args(["--windows", target.to_str().unwrap()])
            .env("HOME", &root)
            .env("XDG_CONFIG_HOME", root.join("config"))
            .env("XDG_DATA_HOME", &data)
            .env("XDG_STATE_HOME", &state)
            .env("XDG_RUNTIME_DIR", root.join("runtime"))
            .env("ASTREA_WINDOWS_RUNNER_RECORD", &output)
            .env("PATH", &bin)
            .output()
            .expect("run astrea-launch Windows CLI")
    };
    let first = run();
    assert!(
        first.status.success(),
        "first launch failed: {}",
        String::from_utf8_lossy(&first.stderr)
    );
    fs::write(
        config.join("compatibility.json"),
        r#"{"runner":"wine","use_proton_profile":false,"gamemode":false}"#,
    )
    .unwrap();
    let second = run();
    assert!(
        second.status.success(),
        "second launch failed: {}",
        String::from_utf8_lossy(&second.stderr)
    );

    for _ in 0..40 {
        let lines = fs::read_to_string(&output).unwrap_or_default();
        if lines.lines().count() >= 2 {
            break;
        }
        thread::sleep(Duration::from_millis(25));
    }
    let recorded = fs::read_to_string(output).unwrap();
    assert_eq!(recorded.lines().collect::<Vec<_>>(), vec!["umu", "wine"]);
    let _ = daemon.kill();
    let _ = daemon.wait();
    let _ = fs::remove_dir_all(root);
}
