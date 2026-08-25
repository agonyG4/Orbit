use std::fs;
use std::path::Path;

use astrea_launch::{
    DesktopLaunchContext, LaunchConfig, LaunchRequest, LaunchTarget, Rule,
    command_from_desktop_file, command_from_desktop_file_with_context, extract_steam_appid,
    matching_rule, parse_cli_request, parse_exec_line_with_context,
};

#[test]
fn expands_desktop_exec_field_codes_against_context() {
    let context = DesktopLaunchContext {
        targets: vec![
            LaunchTarget::File {
                path: "/tmp/one file.txt".into(),
            },
            LaunchTarget::File {
                path: "/tmp/two.txt".into(),
            },
            LaunchTarget::Url {
                url: "https://example.test/item".into(),
            },
        ],
        files: vec!["/tmp/one file.txt".into(), "/tmp/two.txt".into()],
        urls: vec!["https://example.test/item".into()],
        name: Some("Example Editor".into()),
        icon: Some("accessories-text-editor".into()),
        desktop_file: "/tmp/example.desktop".into(),
    };
    let args = parse_exec_line_with_context(
        r#"editor %F --url %U %i %c %k %%"#,
        &context,
    )
    .expect("exec line should parse");

    assert_eq!(
        args,
        vec![
            "editor",
            "/tmp/one file.txt",
            "/tmp/two.txt",
            "--url",
            "file:///tmp/one%20file.txt",
            "file:///tmp/two.txt",
            "https://example.test/item",
            "--icon",
            "accessories-text-editor",
            "Example Editor",
            "/tmp/example.desktop",
            "%",
        ]
    );
}

#[test]
fn rejects_unknown_or_embedded_desktop_field_codes() {
    assert!(parse_exec_line_with_context(
        "editor --name=%c",
        &DesktopLaunchContext::default()
    )
    .is_err());
    assert!(parse_exec_line_with_context(
        "editor %x",
        &DesktopLaunchContext::default()
    )
    .is_err());
}

#[test]
fn desktop_command_keeps_ordered_targets() {
    let dir = std::env::temp_dir().join(format!("astrea-launch-context-{}", std::process::id()));
    let desktop = dir.join("example.desktop");
    fs::create_dir_all(&dir).unwrap();
    fs::write(
        &desktop,
        "[Desktop Entry]\nType=Application\nName=Example\nIcon=example\nExec=example %F %U %i %c %k\n",
    )
    .unwrap();

    let command = command_from_desktop_file_with_context(
        &desktop,
        &DesktopLaunchContext {
            targets: vec![
                LaunchTarget::File {
                    path: "/tmp/one".into(),
                },
                LaunchTarget::File {
                    path: "/tmp/two".into(),
                },
                LaunchTarget::Url {
                    url: "https://example.test/item".into(),
                },
            ],
            files: vec!["/tmp/one".into(), "/tmp/two".into()],
            urls: vec!["https://example.test/item".into()],
            name: Some("Example".into()),
            icon: Some("example".into()),
            desktop_file: desktop.clone(),
        },
    )
    .expect("desktop command");
    assert_eq!(
        command.argv,
        vec![
            "example".to_string(), "/tmp/one".to_string(), "/tmp/two".to_string(),
            "file:///tmp/one".to_string(), "file:///tmp/two".to_string(),
            "https://example.test/item".to_string(), "--icon".to_string(),
            "example".to_string(), "Example".to_string(), desktop.to_string_lossy().to_string()
        ]
    );

    let _ = fs::remove_dir_all(dir);
}

#[test]
fn resolves_desktop_file_to_command_and_working_dir() {
    let dir = std::env::temp_dir().join(format!("astrea-launch-test-{}", std::process::id()));
    let desktop = dir.join("example.desktop");
    fs::create_dir_all(&dir).unwrap();
    fs::write(
        &desktop,
        "[Desktop Entry]\nType=Application\nName=Example\nExec=/usr/bin/example --flag %U\nPath=/tmp\n",
    )
    .unwrap();

    let command = command_from_desktop_file(&desktop).expect("desktop command");

    assert_eq!(command.argv, vec!["/usr/bin/example", "--flag"]);
    assert_eq!(command.working_dir.as_deref(), Some(Path::new("/tmp")));

    let _ = fs::remove_dir_all(dir);
}

#[test]
fn matches_rules_by_desktop_executable_command_and_steam_appid() {
    let config = LaunchConfig {
        rules: vec![
            Rule {
                desktop_id: Some("steam.desktop".into()),
                env: vec![("STEAM_FORCE_DESKTOPUI_SCALING".into(), "1".into())],
                ..Rule::default()
            },
            Rule {
                executable: Some("heroic".into()),
                working_dir: Some("/games".into()),
                ..Rule::default()
            },
            Rule {
                command_substring: Some("--profile gaming".into()),
                launch_boost_ms: Some(1200),
                ..Rule::default()
            },
            Rule {
                steam_appid: Some("264710".into()),
                game_mode_preset: Some("gamemode".into()),
                ..Rule::default()
            },
        ],
        ..LaunchConfig::default()
    };

    assert_eq!(
        matching_rule(
            &config,
            &LaunchRequest::Desktop {
                id: "steam.desktop".into(),
                targets: Vec::new(),
            },
            Some("steam"),
            ""
        )
        .unwrap()
        .env[0]
            .0,
        "STEAM_FORCE_DESKTOPUI_SCALING"
    );
    assert_eq!(
        matching_rule(
            &config,
            &LaunchRequest::Command {
                command: "heroic --profile gaming".into()
            },
            Some("/usr/bin/heroic"),
            "heroic --profile gaming"
        )
        .unwrap()
        .working_dir
        .as_deref(),
        Some("/games")
    );
    assert_eq!(
        matching_rule(
            &config,
            &LaunchRequest::Steam {
                uri: "steam://rungameid/264710".into()
            },
            Some("steam"),
            ""
        )
        .unwrap()
        .game_mode_preset
        .as_deref(),
        Some("gamemode")
    );
}

#[test]
fn extracts_steam_appid_from_supported_urls() {
    assert_eq!(
        extract_steam_appid("steam://rungameid/264710").as_deref(),
        Some("264710")
    );
    assert_eq!(
        extract_steam_appid("https://store.steampowered.com/app/413150/Stardew_Valley/").as_deref(),
        Some("413150")
    );
}

#[test]
fn serializes_launch_requests_for_daemon_protocol() {
    let request = LaunchRequest::Desktop {
        id: "org.gnome.Nautilus".into(),
        targets: vec![
            LaunchTarget::File {
                path: "/tmp/one file.txt".into(),
            },
            LaunchTarget::Url {
                url: "https://example.test/item".into(),
            },
        ],
    };
    let text = serde_json::to_string(&request).expect("request json");

    assert_eq!(
        text,
        r#"{"kind":"desktop","id":"org.gnome.Nautilus","targets":[{"kind":"file","path":"/tmp/one file.txt"},{"kind":"url","url":"https://example.test/item"}]}"#
    );
    assert_eq!(
        serde_json::from_str::<LaunchRequest>(&text).expect("request parse"),
        request
    );

    let argv_request = LaunchRequest::Argv {
        argv: vec!["/usr/bin/true".into(), "--flag".into()],
        working_dir: Some("/tmp".into()),
    };
    let argv_text = serde_json::to_string(&argv_request).expect("argv request json");
    assert_eq!(
        argv_text,
        r#"{"kind":"argv","argv":["/usr/bin/true","--flag"],"working_dir":"/tmp"}"#
    );
    assert_eq!(
        serde_json::from_str::<LaunchRequest>(&argv_text).expect("argv request parse"),
        argv_request
    );
}

#[test]
fn parses_legacy_desktop_request_without_targets() {
    let request: LaunchRequest =
        serde_json::from_str(r#"{"kind":"desktop","id":"org.gnome.Nautilus"}"#)
            .expect("legacy request parse");
    assert_eq!(
        request,
        LaunchRequest::Desktop {
            id: "org.gnome.Nautilus".into(),
            targets: Vec::new(),
        }
    );
}

#[test]
fn rejects_trailing_or_unknown_cli_arguments() {
    assert!(parse_cli_request(&vec!["--file".into(), "/tmp/a".into(), "extra".into()]).is_err());
    assert!(parse_cli_request(&vec![
        "--desktop".into(),
        "org.example.Editor.desktop".into(),
        "--unknown".into(),
        "value".into(),
    ])
    .is_err());
}
