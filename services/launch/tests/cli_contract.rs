use std::fs;
use std::os::unix::fs::PermissionsExt;
use std::process::Command;
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
