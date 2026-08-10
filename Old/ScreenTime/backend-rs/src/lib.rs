use chrono::{
    DateTime, Datelike, Duration as ChronoDuration, Local, LocalResult, NaiveDate, SecondsFormat,
    TimeZone, Timelike, Utc,
};
use serde::{Deserialize, Serialize};
use serde_json::{Map, Value, json};
use std::collections::{HashMap, HashSet, VecDeque};
use std::env;
use std::fs::{self, File, OpenOptions};
use std::io::{self, BufRead, BufReader, Write};
use std::os::fd::AsRawFd;
use std::os::unix::fs as unix_fs;
use std::os::unix::net::UnixStream;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::OnceLock;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};
use thiserror::Error;

pub const STATE_SCHEMA_VERSION: u64 = 4;
pub const SETTINGS_SCHEMA_VERSION: u64 = 1;
pub const DEFAULT_INTERVAL_SECONDS: f64 = 15.0;
pub const MAX_SAMPLE_SECONDS: f64 = 60.0;
pub const EVENT_SOCKET_RETRY_SECONDS: f64 = 30.0;
pub const HEALTH_STALE_FLOOR_SECONDS: f64 = 90.0;
pub const DEFAULT_EVENT_RETENTION_LINES: usize = 50_000;
pub const RUST_SERVICE_NAME: &str = "astrea-screentimed.service";
pub const PYTHON_SERVICE_NAME: &str = "astrea-screentimed-legacy.service";
pub const LEGACY_SERVICE_NAMES: [&str; 2] =
    ["bench-screentime.service", "astrea-screentimed-rs.service"];

const MONTH_NAMES_PT: [&str; 12] = [
    "janeiro",
    "fevereiro",
    "marco",
    "abril",
    "maio",
    "junho",
    "julho",
    "agosto",
    "setembro",
    "outubro",
    "novembro",
    "dezembro",
];
const WEEKDAY_SHORT_PT: [&str; 7] = ["Seg", "Ter", "Qua", "Qui", "Sex", "Sab", "Dom"];
const STOP_CHECK_INTERVAL_MILLIS: u64 = 250;
const APP_METADATA_KEYS: [&str; 16] = [
    "category",
    "class",
    "title",
    "last_title",
    "last_seen_at",
    "initialClass",
    "initialTitle",
    "pid",
    "address",
    "icon",
    "iconSource",
    "icon_path",
    "iconPath",
    "astreaIcon",
    "astreaIconName",
    "hideIconFallback",
];
const SERVICE_ENV_KEYS: [&str; 4] = [
    "HYPRLAND_INSTANCE_SIGNATURE",
    "WAYLAND_DISPLAY",
    "XDG_CURRENT_DESKTOP",
    "XDG_SESSION_TYPE",
];

#[derive(Debug, Error)]
pub enum BackendError {
    #[error("io: {0}")]
    Io(#[from] io::Error),
    #[error("json: {0}")]
    Json(#[from] serde_json::Error),
    #[error("collector is already running")]
    AlreadyRunning,
    #[error("hyprland event socket closed")]
    EventSocketClosed,
    #[error("command failed: {0}")]
    Command(String),
    #[error("unsupported service action: {0}")]
    UnsupportedServiceAction(String),
}

pub type Result<T> = std::result::Result<T, BackendError>;

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct WindowInfo {
    #[serde(default)]
    pub class: String,
    #[serde(default)]
    pub title: String,
    #[serde(default, rename = "initialClass")]
    pub initial_class: String,
    #[serde(default, rename = "initialTitle")]
    pub initial_title: String,
    #[serde(default)]
    pub pid: u64,
    #[serde(default)]
    pub address: String,
    #[serde(default)]
    pub workspace: String,
    #[serde(default)]
    pub ok: bool,
    #[serde(default)]
    pub error: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct Sample {
    pub app: String,
    pub category: String,
    #[serde(rename = "category_label")]
    pub category_label: String,
    #[serde(default)]
    pub class: String,
    #[serde(default)]
    pub title: String,
    #[serde(default, rename = "initialClass")]
    pub initial_class: String,
    #[serde(default, rename = "initialTitle")]
    pub initial_title: String,
    #[serde(default)]
    pub pid: u64,
    #[serde(default)]
    pub address: String,
    #[serde(default)]
    pub workspace: String,
    #[serde(default)]
    pub ok: bool,
    #[serde(default)]
    pub error: String,
    #[serde(rename = "sampled_at")]
    pub sampled_at: String,
}

#[derive(Debug, Clone)]
pub struct Paths {
    pub state_path: PathBuf,
    pub events_path: PathBuf,
    pub rules_path: PathBuf,
    pub lock_path: PathBuf,
    pub settings_path: PathBuf,
}

impl Paths {
    pub fn default_for_project() -> Self {
        let state_dir = home_dir()
            .join(".local")
            .join("state")
            .join("Bench")
            .join("ScreenTime");
        Self {
            state_path: state_dir.join("usage.json"),
            events_path: state_dir.join("events.jsonl"),
            lock_path: state_dir.join("monitor.lock"),
            settings_path: state_dir.join("settings.json"),
            rules_path: project_dir().join("app_rules.json"),
        }
    }
}

#[derive(Debug, Clone)]
pub struct MonitorOptions {
    pub interval_seconds: f64,
    pub paths: Paths,
}

impl Default for MonitorOptions {
    fn default() -> Self {
        Self {
            interval_seconds: DEFAULT_INTERVAL_SECONDS,
            paths: Paths::default_for_project(),
        }
    }
}

pub fn project_dir() -> PathBuf {
    if let Ok(path) = env::var("SCREENTIME_PROJECT_DIR") {
        return PathBuf::from(path);
    }
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .map(Path::to_path_buf)
        .unwrap_or_else(|| env::current_dir().unwrap_or_else(|_| PathBuf::from(".")))
}

pub fn home_dir() -> PathBuf {
    env::var("HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|_| PathBuf::from("."))
}

pub fn now_iso_utc() -> String {
    Utc::now().to_rfc3339_opts(SecondsFormat::Secs, false)
}

pub fn normalize(value: &str) -> String {
    value.trim().to_lowercase()
}

pub fn normalize_window_address(value: &str) -> String {
    let text = value.trim().to_lowercase();
    text.strip_prefix("0x").unwrap_or(&text).to_string()
}

pub fn hyprland_event_name(line: &str) -> &str {
    line.split_once(">>").map_or("", |(name, _)| name.trim())
}

pub fn hyprland_event_payload(line: &str) -> &str {
    line.split_once(">>")
        .map_or("", |(_, payload)| payload.trim())
}

pub fn hyprland_event_changes_focus(line: &str) -> bool {
    matches!(
        hyprland_event_name(line),
        "activewindow"
            | "activewindowv2"
            | "closewindow"
            | "focusedmon"
            | "movewindow"
            | "openwindow"
            | "workspace"
            | "workspacev2"
    )
}

pub fn hyprland_event_requires_sample(line: &str, previous_sample: &Sample) -> bool {
    if !hyprland_event_changes_focus(line) {
        return false;
    }

    match hyprland_event_name(line) {
        "activewindowv2" => {
            let next_address = normalize_window_address(
                hyprland_event_payload(line)
                    .split_once(',')
                    .map_or(hyprland_event_payload(line), |(addr, _)| addr),
            );
            let current_address = normalize_window_address(&previous_sample.address);
            !next_address.is_empty() && next_address != current_address
        }
        "activewindow" => {
            let next_class = normalize(
                hyprland_event_payload(line)
                    .split_once(',')
                    .map_or(hyprland_event_payload(line), |(class, _)| class),
            );
            let current_class = normalize(&previous_sample.class);
            !next_class.is_empty() && next_class != current_class
        }
        _ => true,
    }
}

pub fn samples_same_window(previous: &Sample, current: &Sample) -> bool {
    let previous_address = normalize_window_address(&previous.address);
    let current_address = normalize_window_address(&current.address);
    if !previous_address.is_empty() || !current_address.is_empty() {
        return previous_address == current_address;
    }
    previous.app == current.app && normalize(&previous.class) == normalize(&current.class)
}

pub fn sample_changed(previous: &Sample, current: &Sample) -> bool {
    !samples_same_window(previous, current) || previous.app != current.app
}

pub fn resolve_app(raw_class: &str, title: &str, rules: &Value) -> (String, String, String) {
    let class_key = normalize(raw_class);
    let title_key = normalize(title);
    let categories = rules
        .get("categories")
        .and_then(Value::as_object)
        .cloned()
        .unwrap_or_default();

    if looks_like_windows_game(&class_key, &title_key) {
        return (
            fallback_game_app_id(&class_key, &title_key),
            "games".to_string(),
            category_label(&categories, "games", "Games"),
        );
    }

    for (category_id, category) in &categories {
        let Some(apps) = category.get("apps").and_then(Value::as_object) else {
            continue;
        };
        for (app_id, rule) in apps {
            let (class_aliases, title_aliases, exact_titles) = rule_aliases(app_id, rule);
            if exact_titles
                .iter()
                .any(|alias| !alias.is_empty() && title_key == *alias)
            {
                return resolved(app_id, category_id, category);
            }
            if title_aliases.iter().any(|alias| {
                !alias.is_empty() && !title_key.is_empty() && title_key.contains(alias)
            }) {
                return resolved(app_id, category_id, category);
            }
            if class_aliases.iter().any(|alias| {
                !alias.is_empty() && (class_key == *alias || class_key.contains(alias))
            }) {
                return resolved(app_id, category_id, category);
            }
        }
    }

    (
        if class_key.is_empty() {
            "unknown".to_string()
        } else {
            class_key
        },
        "other".to_string(),
        category_label(&categories, "other", "Other"),
    )
}

pub fn resolve_app_with_process(
    raw_class: &str,
    title: &str,
    pid: u64,
    rules: &Value,
) -> (String, String, String) {
    let resolved = resolve_app(raw_class, title, rules);
    if resolved.1 != "other" || !process_looks_like_windows_game(pid) {
        return resolved;
    }

    let categories = rules
        .get("categories")
        .and_then(Value::as_object)
        .cloned()
        .unwrap_or_default();
    let class_key = normalize(raw_class);
    let title_key = normalize(title);
    (
        fallback_game_app_id(&class_key, &title_key),
        "games".to_string(),
        category_label(&categories, "games", "Games"),
    )
}

fn resolved(app_id: &str, category_id: &str, category: &Value) -> (String, String, String) {
    let label = category
        .get("label")
        .and_then(Value::as_str)
        .unwrap_or(category_id)
        .to_string();
    (app_id.to_string(), category_id.to_string(), label)
}

fn category_label(categories: &Map<String, Value>, category_id: &str, fallback: &str) -> String {
    categories
        .get(category_id)
        .and_then(|category| category.get("label"))
        .and_then(Value::as_str)
        .unwrap_or(fallback)
        .to_string()
}

fn fallback_game_app_id(class_key: &str, title_key: &str) -> String {
    if !class_key.is_empty() {
        class_key.to_string()
    } else if !title_key.is_empty() {
        title_key.to_string()
    } else {
        "unknown".to_string()
    }
}

fn looks_like_windows_game(class_key: &str, title_key: &str) -> bool {
    [class_key, title_key].iter().any(|value| {
        let value = value.trim();
        value_looks_like_windows_game(value)
    })
}

fn value_looks_like_windows_game(value: &str) -> bool {
    !value.is_empty()
        && (value.contains(".exe")
            || value.contains("steam_app_")
            || value == "wine"
            || value == "steam_proton"
            || value.contains("pressure-vessel"))
}

fn process_looks_like_windows_game(pid: u64) -> bool {
    if pid == 0 {
        return false;
    }
    let Ok(bytes) = fs::read(format!("/proc/{pid}/cmdline")) else {
        return false;
    };
    let tokens = bytes
        .split(|byte| *byte == b'\0')
        .filter(|token| !token.is_empty())
        .map(|token| String::from_utf8_lossy(token).into_owned())
        .collect::<Vec<_>>();
    process_tokens_look_like_windows_game(&tokens)
}

fn process_tokens_look_like_windows_game<T: AsRef<str>>(tokens: &[T]) -> bool {
    tokens
        .iter()
        .any(|token| value_looks_like_windows_game(&normalize(token.as_ref())))
}

fn rule_aliases(app_id: &str, rule: &Value) -> (Vec<String>, Vec<String>, Vec<String>) {
    let mut class_aliases = vec![normalize(app_id)];
    let mut title_aliases = Vec::new();
    let mut exact_titles = Vec::new();

    match rule {
        Value::Array(items) => {
            class_aliases.extend(items.iter().filter_map(Value::as_str).map(normalize));
        }
        Value::Object(map) => {
            class_aliases.extend(
                array_strings(map.get("aliases"))
                    .into_iter()
                    .map(|value| normalize(&value)),
            );
            title_aliases.extend(
                array_strings(map.get("title_aliases"))
                    .into_iter()
                    .map(|value| normalize(&value)),
            );
            exact_titles.extend(
                array_strings(map.get("exact_titles"))
                    .into_iter()
                    .map(|value| normalize(&value)),
            );
        }
        _ => {}
    }

    (class_aliases, title_aliases, exact_titles)
}

fn array_strings(value: Option<&Value>) -> Vec<String> {
    value
        .and_then(Value::as_array)
        .map(|items| {
            items
                .iter()
                .filter_map(Value::as_str)
                .map(ToString::to_string)
                .collect()
        })
        .unwrap_or_default()
}

pub fn empty_state() -> Value {
    let now = now_iso_utc();
    json!({
        "schema_version": STATE_SCHEMA_VERSION,
        "created_at": now,
        "updated_at": now,
        "total_seconds": 0.0,
        "active_seconds": 0.0,
        "unknown_seconds": 0.0,
        "sample_count": 0,
        "error_count": 0,
        "days": {},
        "weeks": {},
        "apps": {},
        "categories": {},
        "current": {},
        "health": {
            "running": false,
            "last_error": "",
            "last_sample_at": ""
        }
    })
}

pub fn empty_settings() -> Value {
    json!({
        "schema_version": SETTINGS_SCHEMA_VERSION,
        "hidden_apps": [],
    })
}

pub fn migrate_settings(mut settings: Value) -> Value {
    if !settings.is_object() {
        return empty_settings();
    }
    let object = settings.as_object_mut().unwrap_or_else(|| unreachable!());
    object.insert("schema_version".to_string(), json!(SETTINGS_SCHEMA_VERSION));
    if !object.get("hidden_apps").is_some_and(Value::is_array) {
        object.insert("hidden_apps".to_string(), json!([]));
    }
    let hidden = normalized_hidden_apps(&settings);
    if let Some(object) = settings.as_object_mut() {
        object.insert(
            "hidden_apps".to_string(),
            Value::Array(hidden.into_iter().map(Value::String).collect()),
        );
    }
    settings
}

pub fn load_settings(path: &Path) -> Value {
    migrate_settings(load_json(path, empty_settings()))
}

fn normalized_hidden_apps(settings: &Value) -> Vec<String> {
    let mut values = settings
        .get("hidden_apps")
        .and_then(Value::as_array)
        .map(|items| {
            items
                .iter()
                .filter_map(Value::as_str)
                .map(str::trim)
                .filter(|value| !value.is_empty())
                .map(ToString::to_string)
                .collect::<Vec<_>>()
        })
        .unwrap_or_default();
    values.sort();
    values.dedup();
    values
}

fn hidden_apps_set(settings: &Value) -> HashSet<String> {
    normalized_hidden_apps(settings).into_iter().collect()
}

pub fn update_hidden_app(mut settings: Value, app_id: &str, hidden: bool) -> Value {
    settings = migrate_settings(settings);
    let app_id = app_id.trim();
    if app_id.is_empty() {
        return settings;
    }
    let mut hidden_apps = hidden_apps_set(&settings);
    if hidden {
        hidden_apps.insert(app_id.to_string());
    } else {
        hidden_apps.remove(app_id);
    }
    let mut hidden_list = hidden_apps.into_iter().collect::<Vec<_>>();
    hidden_list.sort();
    if let Some(object) = settings.as_object_mut() {
        object.insert(
            "hidden_apps".to_string(),
            Value::Array(hidden_list.into_iter().map(Value::String).collect()),
        );
    }
    settings
}

pub fn settings_json(paths: &Paths) -> Value {
    load_settings(&paths.settings_path)
}

pub fn set_hidden_app(paths: &Paths, app_id: &str, hidden: bool) -> Result<Value> {
    let settings = update_hidden_app(load_settings(&paths.settings_path), app_id, hidden);
    atomic_write_json(&paths.settings_path, &settings)?;
    Ok(settings)
}

pub fn privacy_auth_json() -> Result<Value> {
    let output = Command::new("pkexec").arg("/usr/bin/true").output();
    let Ok(output) = output else {
        return Err(BackendError::Command("pkexec unavailable".to_string()));
    };
    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr).trim().to_string();
        return Err(BackendError::Command(if stderr.is_empty() {
            "authentication cancelled".to_string()
        } else {
            stderr
        }));
    }
    Ok(json!({
        "ok": true,
        "authenticated": true,
    }))
}

pub fn remove_app(paths: &Paths, app_id: &str) -> Result<Value> {
    let app_id = app_id.trim();
    if app_id.is_empty() {
        return Ok(settings_json(paths));
    }

    let state = anonymize_app_usage(load_json(&paths.state_path, empty_state()), app_id);
    atomic_write_json(&paths.state_path, &state)?;
    anonymize_events_file(&paths.events_path, app_id)?;

    let settings = update_hidden_app(load_settings(&paths.settings_path), app_id, false);
    atomic_write_json(&paths.settings_path, &settings)?;
    Ok(settings)
}

pub fn compact_events_file(path: &Path, max_lines: usize) -> Result<Value> {
    let max_lines = max_lines.max(1);
    let before_bytes = path.metadata().map(|metadata| metadata.len()).unwrap_or(0);
    if !path.exists() {
        return Ok(json!({
            "ok": true,
            "changed": false,
            "before_lines": 0,
            "after_lines": 0,
            "before_bytes": 0,
            "after_bytes": 0,
            "retention_lines": max_lines,
        }));
    }

    let file = File::open(path)?;
    let reader = BufReader::new(file);
    let mut kept = VecDeque::with_capacity(max_lines);
    let mut before_lines = 0usize;
    for line in reader.lines() {
        before_lines += 1;
        if kept.len() == max_lines {
            kept.pop_front();
        }
        kept.push_back(line?);
    }

    let changed = before_lines > kept.len();
    if changed {
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)?;
        }
        let tmp = path.with_extension("jsonl.tmp");
        let mut file = File::create(&tmp)?;
        for line in &kept {
            file.write_all(line.as_bytes())?;
            file.write_all(b"\n")?;
        }
        file.sync_all()?;
        fs::rename(tmp, path)?;
    }

    let after_bytes = path.metadata().map(|metadata| metadata.len()).unwrap_or(0);
    Ok(json!({
        "ok": true,
        "changed": changed,
        "before_lines": before_lines,
        "after_lines": kept.len(),
        "before_bytes": before_bytes,
        "after_bytes": after_bytes,
        "retention_lines": max_lines,
    }))
}

pub fn compact_events(paths: &Paths, max_lines: usize) -> Result<Value> {
    compact_events_file(&paths.events_path, max_lines)
}

pub fn doctor_json(paths: &Paths) -> Value {
    let state = json_file_diagnosis(
        "state",
        &paths.state_path,
        Some(STATE_SCHEMA_VERSION),
        "schema_version",
    );
    let settings = json_file_diagnosis(
        "settings",
        &paths.settings_path,
        Some(SETTINGS_SCHEMA_VERSION),
        "schema_version",
    );
    let rules = json_file_diagnosis("rules", &paths.rules_path, None, "");
    let events = events_diagnosis(&paths.events_path);
    let service = service_status_with_peer();
    let event_socket = hyprland_event_socket_path();
    let event_socket_exists = event_socket.as_ref().is_some_and(|path| path.exists());

    let mut checks = vec![
        state.get("check").cloned().unwrap_or_else(|| json!({})),
        settings.get("check").cloned().unwrap_or_else(|| json!({})),
        rules.get("check").cloned().unwrap_or_else(|| json!({})),
        events.get("check").cloned().unwrap_or_else(|| json!({})),
    ];
    checks.push(doctor_check(
        "service",
        "Servico de coleta",
        if service
            .get("collector_active")
            .and_then(Value::as_bool)
            .unwrap_or(false)
        {
            "ok"
        } else {
            "warning"
        },
        service
            .get("collector_active")
            .and_then(Value::as_bool)
            .unwrap_or(false),
        if service
            .get("collector_active")
            .and_then(Value::as_bool)
            .unwrap_or(false)
        {
            "ativo"
        } else {
            "inativo"
        },
    ));
    checks.push(doctor_check(
        "event_socket",
        "Socket de eventos Hyprland",
        if event_socket_exists { "ok" } else { "warning" },
        event_socket_exists,
        if event_socket_exists {
            "encontrado"
        } else {
            "nao encontrado"
        },
    ));

    let ok = !checks.iter().any(|check| {
        check.get("severity").and_then(Value::as_str) == Some("error")
            && !check.get("ok").and_then(Value::as_bool).unwrap_or(false)
    });
    json!({
        "ok": ok,
        "generated_at": now_iso_utc(),
        "backend": "rust",
        "paths": {
            "state": state.get("path").cloned().unwrap_or_else(|| json!({})),
            "settings": settings.get("path").cloned().unwrap_or_else(|| json!({})),
            "rules": rules.get("path").cloned().unwrap_or_else(|| json!({})),
            "events": events.get("path").cloned().unwrap_or_else(|| json!({})),
            "lock": path_diagnosis(&paths.lock_path),
        },
        "events": events.get("metrics").cloned().unwrap_or_else(|| json!({})),
        "service": service,
        "event_socket": {
            "path": event_socket.map(|path| path.display().to_string()).unwrap_or_default(),
            "exists": event_socket_exists,
        },
        "checks": checks,
    })
}

fn json_file_diagnosis(
    id: &str,
    path: &Path,
    expected_schema: Option<u64>,
    schema_key: &str,
) -> Value {
    let path_payload = path_diagnosis(path);
    if !path.exists() {
        return json!({
            "path": path_payload,
            "check": doctor_check(id, id, "warning", false, "arquivo ausente"),
        });
    }
    let text = match fs::read_to_string(path) {
        Ok(text) => text,
        Err(error) => {
            return json!({
                "path": path_payload,
                "check": doctor_check(id, id, "error", false, &error.to_string()),
            });
        }
    };
    let parsed = match serde_json::from_str::<Value>(&text) {
        Ok(value) => value,
        Err(error) => {
            return json!({
                "path": path_payload,
                "check": doctor_check(id, id, "error", false, &format!("json invalido: {error}")),
            });
        }
    };
    if let Some(expected) = expected_schema {
        let actual = parsed.get(schema_key).and_then(Value::as_u64).unwrap_or(0);
        if actual != expected {
            return json!({
                "path": path_payload,
                "check": doctor_check(id, id, "warning", false, &format!("schema {actual}, esperado {expected}")),
            });
        }
    }
    json!({
        "path": path_payload,
        "check": doctor_check(id, id, "ok", true, "ok"),
    })
}

fn events_diagnosis(path: &Path) -> Value {
    let path_payload = path_diagnosis(path);
    if !path.exists() {
        return json!({
            "path": path_payload,
            "metrics": {
                "lines": 0,
                "bytes": 0,
                "retention_lines": DEFAULT_EVENT_RETENTION_LINES,
            },
            "check": doctor_check("events", "events.jsonl", "warning", false, "arquivo ausente"),
        });
    }
    match count_file_lines(path) {
        Ok(lines) => {
            let bytes = path.metadata().map(|metadata| metadata.len()).unwrap_or(0);
            let severity = if lines > DEFAULT_EVENT_RETENTION_LINES {
                "warning"
            } else {
                "ok"
            };
            json!({
                "path": path_payload,
                "metrics": {
                    "lines": lines,
                    "bytes": bytes,
                    "retention_lines": DEFAULT_EVENT_RETENTION_LINES,
                    "needs_compaction": lines > DEFAULT_EVENT_RETENTION_LINES,
                },
                "check": doctor_check(
                    "events",
                    "events.jsonl",
                    severity,
                    lines <= DEFAULT_EVENT_RETENTION_LINES,
                    if lines > DEFAULT_EVENT_RETENTION_LINES { "compactacao recomendada" } else { "ok" },
                ),
            })
        }
        Err(error) => json!({
            "path": path_payload,
            "metrics": {
                "lines": 0,
                "bytes": 0,
                "retention_lines": DEFAULT_EVENT_RETENTION_LINES,
            },
            "check": doctor_check("events", "events.jsonl", "error", false, &error.to_string()),
        }),
    }
}

fn path_diagnosis(path: &Path) -> Value {
    let metadata = path.metadata().ok();
    json!({
        "path": path.display().to_string(),
        "exists": metadata.is_some(),
        "bytes": metadata.as_ref().map_or(0, fs::Metadata::len),
    })
}

fn count_file_lines(path: &Path) -> Result<usize> {
    let file = File::open(path)?;
    let reader = BufReader::new(file);
    let mut lines = 0usize;
    for line in reader.lines() {
        line?;
        lines += 1;
    }
    Ok(lines)
}

fn doctor_check(id: &str, label: &str, severity: &str, ok: bool, message: &str) -> Value {
    json!({
        "id": id,
        "label": label,
        "severity": severity,
        "ok": ok,
        "message": message,
    })
}

pub fn migrate_state(mut state: Value) -> Value {
    if !state.is_object() {
        return empty_state();
    }
    let previous_schema = state
        .get("schema_version")
        .and_then(Value::as_u64)
        .unwrap_or(0);
    let fresh = empty_state();
    let object = state.as_object_mut().unwrap_or_else(|| unreachable!());
    for (key, value) in fresh.as_object().unwrap_or_else(|| unreachable!()) {
        object.entry(key.clone()).or_insert_with(|| value.clone());
    }
    object.insert("schema_version".to_string(), json!(STATE_SCHEMA_VERSION));
    let days_has_data = state
        .get("days")
        .and_then(Value::as_object)
        .is_some_and(|days| !days.is_empty());
    let weeks_invalid = !state.get("weeks").is_some_and(Value::is_object);
    let weeks_empty = state
        .get("weeks")
        .and_then(Value::as_object)
        .is_some_and(Map::is_empty);
    if previous_schema < 4 || weeks_invalid || (weeks_empty && days_has_data) {
        rebuild_weeks_from_days(&mut state);
    }
    backfill_active_totals(&mut state);
    state
}

fn rebuild_weeks_from_days(state: &mut Value) {
    let days = state
        .get("days")
        .and_then(Value::as_object)
        .cloned()
        .unwrap_or_default();
    let mut weeks = Map::new();

    for (day_key, day_bucket) in days {
        let Some(day) = parse_day_key(&day_key) else {
            continue;
        };
        let week_key = week_key_for_date(day);
        let week_value = weeks.entry(week_key).or_insert_with(|| {
            json!({
                "seconds": 0.0,
                "active_seconds": 0.0,
                "unknown_seconds": 0.0,
                "apps": {},
                "categories": {},
            })
        });
        if !week_value.is_object() {
            *week_value = json!({
                "seconds": 0.0,
                "active_seconds": 0.0,
                "unknown_seconds": 0.0,
                "apps": {},
                "categories": {},
            });
        }
        let week_bucket = week_value.as_object_mut().unwrap_or_else(|| unreachable!());
        let current_seconds = week_bucket
            .get("seconds")
            .and_then(Value::as_f64)
            .unwrap_or(0.0);
        week_bucket.insert(
            "seconds".to_string(),
            json!(round3(current_seconds + value_seconds(&day_bucket))),
        );
        let current_active = week_bucket
            .get("active_seconds")
            .and_then(Value::as_f64)
            .unwrap_or(0.0);
        week_bucket.insert(
            "active_seconds".to_string(),
            json!(round3(
                current_active
                    + day_bucket
                        .get("active_seconds")
                        .and_then(Value::as_f64)
                        .unwrap_or(0.0)
            )),
        );
        let current_unknown = week_bucket
            .get("unknown_seconds")
            .and_then(Value::as_f64)
            .unwrap_or(0.0);
        week_bucket.insert(
            "unknown_seconds".to_string(),
            json!(round3(
                current_unknown
                    + day_bucket
                        .get("unknown_seconds")
                        .and_then(Value::as_f64)
                        .unwrap_or(0.0)
            )),
        );
        merge_usage_values(
            child_object_mut(week_bucket, "apps"),
            day_bucket.get("apps").unwrap_or(&Value::Null),
        );
        merge_usage_values(
            child_object_mut(week_bucket, "categories"),
            day_bucket.get("categories").unwrap_or(&Value::Null),
        );
    }

    if let Some(root) = state.as_object_mut() {
        root.insert("weeks".to_string(), Value::Object(weeks));
    }
}

fn backfill_active_totals(state: &mut Value) {
    let total = state
        .get("total_seconds")
        .and_then(Value::as_f64)
        .unwrap_or(0.0);
    let active = state
        .get("active_seconds")
        .and_then(Value::as_f64)
        .unwrap_or(0.0);
    let unknown = state
        .get("unknown_seconds")
        .and_then(Value::as_f64)
        .unwrap_or(0.0);
    if total > 0.0 && active == 0.0 && unknown == 0.0 {
        if let Some(root) = state.as_object_mut() {
            root.insert("active_seconds".to_string(), json!(total));
        }
    }
    if let Some(days) = state.get_mut("days").and_then(Value::as_object_mut) {
        for day_bucket in days.values_mut().filter_map(Value::as_object_mut) {
            let day_total = day_bucket
                .get("seconds")
                .and_then(Value::as_f64)
                .unwrap_or(0.0);
            let day_active = day_bucket
                .get("active_seconds")
                .and_then(Value::as_f64)
                .unwrap_or(0.0);
            let day_unknown = day_bucket
                .get("unknown_seconds")
                .and_then(Value::as_f64)
                .unwrap_or(0.0);
            if day_total > 0.0 && day_active == 0.0 && day_unknown == 0.0 {
                day_bucket.insert("active_seconds".to_string(), json!(day_total));
            }
        }
    }
}

pub fn load_json(path: &Path, fallback: Value) -> Value {
    let Ok(text) = fs::read_to_string(path) else {
        return fallback;
    };
    serde_json::from_str(&text).unwrap_or(fallback)
}

pub fn atomic_write_json(path: &Path, payload: &Value) -> Result<()> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    let tmp = path.with_extension("json.tmp");
    let mut file = File::create(&tmp)?;
    serde_json::to_writer_pretty(&mut file, payload)?;
    file.write_all(b"\n")?;
    file.sync_all()?;
    fs::rename(tmp, path)?;
    Ok(())
}

pub fn load_rules(path: &Path) -> Value {
    load_json(path, json!({"categories": {}}))
}

pub fn rules_mtime(path: &Path) -> SystemTime {
    fs::metadata(path)
        .and_then(|metadata| metadata.modified())
        .unwrap_or(UNIX_EPOCH)
}

pub fn build_sample(rules: &Value) -> Sample {
    let window = focused_window();
    let (app, category, category_label) =
        resolve_app_with_process(&window.class, &window.title, window.pid, rules);
    Sample {
        app,
        category,
        category_label,
        class: window.class,
        title: window.title,
        initial_class: window.initial_class,
        initial_title: window.initial_title,
        pid: window.pid,
        address: window.address,
        workspace: window.workspace,
        ok: window.ok,
        error: window.error,
        sampled_at: now_iso_utc(),
    }
}

pub fn focused_window() -> WindowInfo {
    let output = Command::new("hyprctl")
        .args(["activewindow", "-j"])
        .envs(hyprctl_env())
        .output();
    let Ok(output) = output else {
        return window_error("hyprctl unavailable");
    };
    if !output.status.success() || output.stdout.is_empty() {
        let error = String::from_utf8_lossy(&output.stderr).trim().to_string();
        return window_error(if error.is_empty() {
            "hyprctl activewindow failed"
        } else {
            &error
        });
    }

    let Ok(value) = serde_json::from_slice::<Value>(&output.stdout) else {
        return window_error("invalid hyprctl json");
    };
    let raw_class = value
        .get("class")
        .or_else(|| value.get("initialClass"))
        .and_then(Value::as_str)
        .unwrap_or("");
    let title = value
        .get("title")
        .or_else(|| value.get("initialTitle"))
        .and_then(Value::as_str)
        .unwrap_or("");
    let workspace = value
        .get("workspace")
        .and_then(|workspace| workspace.get("name"))
        .and_then(Value::as_str)
        .unwrap_or("");
    WindowInfo {
        class: raw_class.to_string(),
        title: title.to_string(),
        initial_class: value
            .get("initialClass")
            .and_then(Value::as_str)
            .unwrap_or(raw_class)
            .to_string(),
        initial_title: value
            .get("initialTitle")
            .and_then(Value::as_str)
            .unwrap_or(title)
            .to_string(),
        pid: value.get("pid").and_then(Value::as_u64).unwrap_or(0),
        address: value
            .get("address")
            .and_then(Value::as_str)
            .unwrap_or("")
            .to_string(),
        workspace: workspace.to_string(),
        ok: !raw_class.is_empty() || !title.is_empty(),
        error: String::new(),
    }
}

fn window_error(message: &str) -> WindowInfo {
    WindowInfo {
        class: String::new(),
        title: String::new(),
        initial_class: String::new(),
        initial_title: String::new(),
        pid: 0,
        address: String::new(),
        workspace: String::new(),
        ok: false,
        error: message.to_string(),
    }
}

pub fn hyprctl_env() -> HashMap<String, String> {
    let mut env_map: HashMap<String, String> = env::vars().collect();
    if env_map.contains_key("HYPRLAND_INSTANCE_SIGNATURE") {
        return env_map;
    }

    let runtime_dir = runtime_dir();
    let hypr_dir = runtime_dir.join("hypr");
    let Ok(entries) = fs::read_dir(hypr_dir) else {
        return env_map;
    };
    let newest = entries
        .filter_map(std::result::Result::ok)
        .map(|entry| entry.path())
        .filter(|path| path.join(".socket.sock").exists())
        .filter_map(|path| {
            let modified = fs::metadata(&path)
                .and_then(|metadata| metadata.modified())
                .ok()?;
            Some((modified, path))
        })
        .max_by_key(|(modified, _)| *modified)
        .map(|(_, path)| path);
    if let Some(path) = newest.and_then(|path| {
        path.file_name()
            .map(|name| name.to_string_lossy().to_string())
    }) {
        env_map.insert("HYPRLAND_INSTANCE_SIGNATURE".to_string(), path);
    }
    env_map
}

pub fn runtime_dir() -> PathBuf {
    env::var("XDG_RUNTIME_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|_| PathBuf::from(format!("/run/user/{}", unsafe { libc::geteuid() })))
}

pub fn hyprland_event_socket_path() -> Option<PathBuf> {
    let env_map = hyprctl_env();
    let runtime = env_map
        .get("XDG_RUNTIME_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(runtime_dir);
    if let Some(signature) = env_map.get("HYPRLAND_INSTANCE_SIGNATURE") {
        let socket_path = runtime.join("hypr").join(signature).join(".socket2.sock");
        if socket_path.exists() {
            return Some(socket_path);
        }
    }

    let entries = fs::read_dir(runtime.join("hypr")).ok()?;
    entries
        .filter_map(std::result::Result::ok)
        .map(|entry| entry.path())
        .filter(|path| path.join(".socket2.sock").exists())
        .filter_map(|path| {
            let modified = fs::metadata(&path)
                .and_then(|metadata| metadata.modified())
                .ok()?;
            Some((modified, path.join(".socket2.sock")))
        })
        .max_by_key(|(modified, _)| *modified)
        .map(|(_, path)| path)
}

pub struct HyprlandEventStream {
    socket_path: PathBuf,
    reader: BufReader<UnixStream>,
}

impl HyprlandEventStream {
    pub fn connect(socket_path: PathBuf) -> Result<Self> {
        let stream = UnixStream::connect(&socket_path)?;
        Ok(Self {
            socket_path,
            reader: BufReader::new(stream),
        })
    }

    pub fn socket_path(&self) -> &Path {
        &self.socket_path
    }

    pub fn read_line(&mut self, timeout: Duration) -> Result<Option<String>> {
        self.reader
            .get_ref()
            .set_read_timeout(Some(timeout.max(Duration::from_millis(1))))?;
        let mut line = String::new();
        match self.reader.read_line(&mut line) {
            Ok(0) => Err(BackendError::EventSocketClosed),
            Ok(_) => Ok(Some(line.trim().to_string())),
            Err(error)
                if matches!(
                    error.kind(),
                    io::ErrorKind::WouldBlock
                        | io::ErrorKind::TimedOut
                        | io::ErrorKind::Interrupted
                ) =>
            {
                Ok(None)
            }
            Err(error) => Err(BackendError::Io(error)),
        }
    }
}

pub struct FileLock {
    file: File,
}

impl FileLock {
    pub fn acquire(path: &Path) -> Result<Self> {
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)?;
        }
        let mut file = OpenOptions::new().create(true).write(true).open(path)?;
        let rc = unsafe { libc::flock(file.as_raw_fd(), libc::LOCK_EX | libc::LOCK_NB) };
        if rc != 0 {
            return Err(BackendError::AlreadyRunning);
        }
        file.set_len(0)?;
        writeln!(file, "{}", std::process::id())?;
        Ok(Self { file })
    }
}

impl Drop for FileLock {
    fn drop(&mut self) {
        let _ = unsafe { libc::flock(self.file.as_raw_fd(), libc::LOCK_UN) };
    }
}

pub fn add_elapsed(state: &mut Value, sample: &Sample, start_ts: f64, end_ts: f64) {
    if end_ts <= start_ts {
        return;
    }

    let mut cursor = start_ts;
    while cursor < end_ts {
        let boundary = next_local_midnight(cursor)
            .min(next_local_hour(cursor))
            .min(end_ts);
        add_seconds_to_day(state, sample, boundary - cursor, cursor);
        cursor = boundary;
    }
}

pub fn add_seconds_to_day(state: &mut Value, sample: &Sample, seconds: f64, timestamp: f64) {
    if seconds <= 0.0 {
        return;
    }

    let day = day_key_from_timestamp(timestamp);
    let week = week_key_from_timestamp(timestamp);
    let hour = local_from_timestamp(timestamp).hour().to_string();
    let root = object_mut(state);
    add_f64(root, "total_seconds", seconds);
    if sample.ok {
        add_f64(root, "active_seconds", seconds);
    } else {
        add_f64(root, "unknown_seconds", seconds);
    }

    {
        let days = child_object_mut(root, "days");
        let day_bucket = ensure_bucket(days, &day, None);
        add_f64(day_bucket, "seconds", seconds);
        if sample.ok {
            add_f64(day_bucket, "active_seconds", seconds);
        } else {
            add_f64(day_bucket, "unknown_seconds", seconds);
        }

        let day_apps = child_object_mut(day_bucket, "apps");
        let day_app_bucket = ensure_bucket(day_apps, &sample.app, None);
        add_f64(day_app_bucket, "seconds", seconds);
        update_app_bucket_metadata(day_app_bucket, sample);

        let day_categories = child_object_mut(day_bucket, "categories");
        let day_category_bucket = ensure_bucket(
            day_categories,
            &sample.category,
            Some(&sample.category_label),
        );
        add_f64(day_category_bucket, "seconds", seconds);

        let hourly = child_object_mut(day_bucket, "hourly");
        let hour_bucket = ensure_bucket(hourly, &hour, None);
        add_f64(hour_bucket, "seconds", seconds);
        if sample.ok {
            add_f64(hour_bucket, "active_seconds", seconds);
        } else {
            add_f64(hour_bucket, "unknown_seconds", seconds);
        }
        let hour_apps = child_object_mut(hour_bucket, "apps");
        let hour_app_bucket = ensure_bucket(hour_apps, &sample.app, None);
        add_f64(hour_app_bucket, "seconds", seconds);
        update_app_bucket_metadata(hour_app_bucket, sample);
        let hour_categories = child_object_mut(hour_bucket, "categories");
        let hour_category_bucket = ensure_bucket(
            hour_categories,
            &sample.category,
            Some(&sample.category_label),
        );
        add_f64(hour_category_bucket, "seconds", seconds);
    }

    {
        let apps = child_object_mut(root, "apps");
        let app_bucket = ensure_bucket(apps, &sample.app, None);
        add_f64(app_bucket, "seconds", seconds);
        update_app_bucket_metadata(app_bucket, sample);
        app_bucket.insert("last_seen_at".to_string(), Value::String(now_iso_utc()));
    }

    {
        let categories = child_object_mut(root, "categories");
        let category_bucket =
            ensure_bucket(categories, &sample.category, Some(&sample.category_label));
        add_f64(category_bucket, "seconds", seconds);
    }

    {
        let weeks = child_object_mut(root, "weeks");
        let week_bucket = ensure_bucket(weeks, &week, None);
        add_f64(week_bucket, "seconds", seconds);
        if sample.ok {
            add_f64(week_bucket, "active_seconds", seconds);
        } else {
            add_f64(week_bucket, "unknown_seconds", seconds);
        }
        let week_apps = child_object_mut(week_bucket, "apps");
        let week_app_bucket = ensure_bucket(week_apps, &sample.app, None);
        add_f64(week_app_bucket, "seconds", seconds);
        update_app_bucket_metadata(week_app_bucket, sample);
        let week_categories = child_object_mut(week_bucket, "categories");
        let week_category_bucket = ensure_bucket(
            week_categories,
            &sample.category,
            Some(&sample.category_label),
        );
        add_f64(week_category_bucket, "seconds", seconds);
    }
}

fn object_mut(value: &mut Value) -> &mut Map<String, Value> {
    if !value.is_object() {
        *value = Value::Object(Map::new());
    }
    value.as_object_mut().unwrap_or_else(|| unreachable!())
}

fn child_object_mut<'a>(map: &'a mut Map<String, Value>, key: &str) -> &'a mut Map<String, Value> {
    let value = map
        .entry(key.to_string())
        .or_insert_with(|| Value::Object(Map::new()));
    if !value.is_object() {
        *value = Value::Object(Map::new());
    }
    value.as_object_mut().unwrap_or_else(|| unreachable!())
}

fn ensure_bucket<'a>(
    root: &'a mut Map<String, Value>,
    key: &str,
    label: Option<&str>,
) -> &'a mut Map<String, Value> {
    let value = root
        .entry(key.to_string())
        .or_insert_with(|| json!({"seconds": 0.0}));
    if !value.is_object() {
        *value = json!({"seconds": 0.0});
    }
    let bucket = value.as_object_mut().unwrap_or_else(|| unreachable!());
    bucket
        .entry("seconds".to_string())
        .or_insert_with(|| json!(0.0));
    if let Some(label) = label {
        bucket.insert("label".to_string(), Value::String(label.to_string()));
    }
    bucket
}

fn add_f64(map: &mut Map<String, Value>, key: &str, amount: f64) {
    let current = map.get(key).and_then(Value::as_f64).unwrap_or(0.0);
    map.insert(key.to_string(), json!(round3(current + amount)));
}

fn add_u64(map: &mut Map<String, Value>, key: &str, amount: u64) {
    let current = map.get(key).and_then(Value::as_u64).unwrap_or(0);
    map.insert(key.to_string(), json!(current + amount));
}

pub fn anonymize_app_usage(mut state: Value, app_id: &str) -> Value {
    let app_id = app_id.trim();
    state = migrate_state(state);
    if app_id.is_empty() || app_id == "unknown" {
        return state;
    }

    if let Some(root) = state.as_object_mut() {
        anonymize_usage_scope(root, app_id);

        if let Some(days) = root.get_mut("days").and_then(Value::as_object_mut) {
            for day in days.values_mut().filter_map(Value::as_object_mut) {
                anonymize_usage_scope(day, app_id);
                if let Some(hourly) = day.get_mut("hourly").and_then(Value::as_object_mut) {
                    for hour in hourly.values_mut().filter_map(Value::as_object_mut) {
                        anonymize_usage_scope(hour, app_id);
                    }
                }
            }
        }

        if let Some(weeks) = root.get_mut("weeks").and_then(Value::as_object_mut) {
            for week in weeks.values_mut().filter_map(Value::as_object_mut) {
                anonymize_usage_scope(week, app_id);
            }
        }

        if root
            .get("current")
            .and_then(|current| current.get("app"))
            .and_then(Value::as_str)
            == Some(app_id)
        {
            root.insert("current".to_string(), unknown_current_sample());
        }
        root.insert("updated_at".to_string(), Value::String(now_iso_utc()));
    }

    state
}

fn anonymize_usage_scope(scope: &mut Map<String, Value>, app_id: &str) {
    let removed = scope
        .get_mut("apps")
        .and_then(Value::as_object_mut)
        .and_then(|apps| apps.remove(app_id));
    let Some(removed) = removed else {
        return;
    };
    let seconds = round3(value_seconds(&removed));
    if seconds <= 0.0 {
        return;
    }
    let source_category = removed
        .get("category")
        .and_then(Value::as_str)
        .unwrap_or("other")
        .to_string();

    {
        let apps = child_object_mut(scope, "apps");
        let unknown = ensure_bucket(apps, "unknown", None);
        add_f64(unknown, "seconds", seconds);
        apply_unknown_app_metadata(unknown);
    }

    {
        let categories = child_object_mut(scope, "categories");
        move_category_usage_to_other(categories, &source_category, seconds);
    }
}

fn apply_unknown_app_metadata(bucket: &mut Map<String, Value>) {
    bucket.insert("label".to_string(), Value::String("Unknown".to_string()));
    bucket.insert("category".to_string(), Value::String("other".to_string()));
    bucket.insert("class".to_string(), Value::String("unknown".to_string()));
    bucket.insert("title".to_string(), Value::String("Unknown".to_string()));
    bucket.insert(
        "last_title".to_string(),
        Value::String("Unknown".to_string()),
    );
    bucket.insert(
        "initialClass".to_string(),
        Value::String("unknown".to_string()),
    );
    bucket.insert(
        "initialTitle".to_string(),
        Value::String("Unknown".to_string()),
    );
    bucket.insert("pid".to_string(), json!(0));
    bucket.insert("address".to_string(), Value::String(String::new()));
}

fn move_category_usage_to_other(
    categories: &mut Map<String, Value>,
    source_category: &str,
    seconds: f64,
) {
    if source_category != "other" {
        subtract_category_seconds(categories, source_category, seconds);
        let other = ensure_bucket(categories, "other", Some("Other"));
        add_f64(other, "seconds", seconds);
    } else {
        let other = ensure_bucket(categories, "other", Some("Other"));
        other.insert("label".to_string(), Value::String("Other".to_string()));
    }
}

fn subtract_category_seconds(categories: &mut Map<String, Value>, category_id: &str, seconds: f64) {
    let should_remove = if let Some(bucket) = categories
        .get_mut(category_id)
        .and_then(Value::as_object_mut)
    {
        let current = bucket.get("seconds").and_then(Value::as_f64).unwrap_or(0.0);
        let remaining = round3((current - seconds).max(0.0));
        if remaining > 0.0 {
            bucket.insert("seconds".to_string(), json!(remaining));
            false
        } else {
            true
        }
    } else {
        false
    };
    if should_remove {
        categories.remove(category_id);
    }
}

fn unknown_current_sample() -> Value {
    json!({
        "app": "unknown",
        "category": "other",
        "category_label": "Other",
        "class": "unknown",
        "title": "Unknown",
        "initialClass": "unknown",
        "initialTitle": "Unknown",
        "pid": 0,
        "address": "",
        "workspace": "",
        "ok": false,
        "error": "",
        "sampled_at": now_iso_utc(),
    })
}

fn anonymize_events_file(path: &Path, app_id: &str) -> Result<()> {
    if !path.exists() {
        return Ok(());
    }
    let text = fs::read_to_string(path)?;
    let mut changed = false;
    let mut lines = Vec::new();
    for line in text.lines() {
        match serde_json::from_str::<Value>(line) {
            Ok(mut event) => {
                if anonymize_event_value(&mut event, app_id) {
                    changed = true;
                }
                lines.push(serde_json::to_string(&event)?);
            }
            Err(_) => lines.push(line.to_string()),
        }
    }
    if changed {
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)?;
        }
        let tmp = path.with_extension("jsonl.tmp");
        let mut file = File::create(&tmp)?;
        for line in lines {
            file.write_all(line.as_bytes())?;
            file.write_all(b"\n")?;
        }
        file.sync_all()?;
        fs::rename(tmp, path)?;
    }
    Ok(())
}

fn anonymize_event_value(value: &mut Value, app_id: &str) -> bool {
    let mut changed = false;
    match value {
        Value::Object(map) => {
            if map.get("app").and_then(Value::as_str) == Some(app_id) {
                map.insert("app".to_string(), Value::String("unknown".to_string()));
                map.insert("category".to_string(), Value::String("other".to_string()));
                map.insert(
                    "category_label".to_string(),
                    Value::String("Other".to_string()),
                );
                map.insert("class".to_string(), Value::String("unknown".to_string()));
                map.insert("title".to_string(), Value::String("Unknown".to_string()));
                map.insert(
                    "initialClass".to_string(),
                    Value::String("unknown".to_string()),
                );
                map.insert(
                    "initialTitle".to_string(),
                    Value::String("Unknown".to_string()),
                );
                changed = true;
            }
            for child in map.values_mut() {
                changed |= anonymize_event_value(child, app_id);
            }
        }
        Value::Array(items) => {
            for child in items {
                changed |= anonymize_event_value(child, app_id);
            }
        }
        _ => {}
    }
    changed
}

fn update_app_bucket_metadata(bucket: &mut Map<String, Value>, sample: &Sample) {
    bucket.insert(
        "category".to_string(),
        Value::String(sample.category.clone()),
    );
    bucket.insert("class".to_string(), Value::String(sample.class.clone()));
    bucket.insert("title".to_string(), Value::String(sample.title.clone()));
    bucket.insert(
        "last_title".to_string(),
        Value::String(sample.title.clone()),
    );
    bucket.insert(
        "initialClass".to_string(),
        Value::String(sample.initial_class.clone()),
    );
    bucket.insert(
        "initialTitle".to_string(),
        Value::String(sample.initial_title.clone()),
    );
    bucket.insert("pid".to_string(), json!(sample.pid));
    bucket.insert("address".to_string(), Value::String(sample.address.clone()));
}

fn round3(value: f64) -> f64 {
    (value * 1000.0).round() / 1000.0
}

pub fn local_from_timestamp(timestamp: f64) -> DateTime<Local> {
    let secs = timestamp.floor() as i64;
    let nanos = ((timestamp - secs as f64) * 1_000_000_000.0)
        .round()
        .max(0.0) as u32;
    match Local.timestamp_opt(secs, nanos) {
        LocalResult::Single(value) | LocalResult::Ambiguous(value, _) => value,
        LocalResult::None => Local::now(),
    }
}

pub fn day_key_from_timestamp(timestamp: f64) -> String {
    local_from_timestamp(timestamp)
        .format("%Y-%m-%d")
        .to_string()
}

pub fn week_key_from_timestamp(timestamp: f64) -> String {
    let date = local_from_timestamp(timestamp).date_naive();
    let start = date - ChronoDuration::days(date.weekday().num_days_from_monday() as i64);
    start.format("%Y-%m-%d").to_string()
}

pub fn next_local_hour(timestamp: f64) -> f64 {
    let value = local_from_timestamp(timestamp) + ChronoDuration::hours(1);
    local_timestamp(value.year(), value.month(), value.day(), value.hour(), 0, 0)
}

pub fn next_local_midnight(timestamp: f64) -> f64 {
    let next_day = local_from_timestamp(timestamp).date_naive() + ChronoDuration::days(1);
    local_timestamp(next_day.year(), next_day.month(), next_day.day(), 0, 0, 0)
}

fn local_timestamp(year: i32, month: u32, day: u32, hour: u32, minute: u32, second: u32) -> f64 {
    match Local.with_ymd_and_hms(year, month, day, hour, minute, second) {
        LocalResult::Single(value) | LocalResult::Ambiguous(value, _) => value.timestamp() as f64,
        LocalResult::None => Local::now().timestamp() as f64,
    }
}

pub fn append_event(paths: &Paths, previous: Option<&Sample>, current: &Sample) -> Result<()> {
    if let Some(parent) = paths.events_path.parent() {
        fs::create_dir_all(parent)?;
    }
    let mut file = OpenOptions::new()
        .create(true)
        .append(true)
        .open(&paths.events_path)?;
    let event = json!({
        "at": now_iso_utc(),
        "from": previous,
        "to": current,
    });
    writeln!(file, "{}", serde_json::to_string(&event)?)?;
    Ok(())
}

pub fn monitor_health_payload(
    sample: &Sample,
    interval_seconds: f64,
    rules_loaded_at: &str,
    collector_mode: &str,
    last_sample_at: &str,
    event_socket: Option<&Path>,
    event_socket_error: &str,
) -> Value {
    let mut health = Map::new();
    health.insert("running".to_string(), json!(true));
    health.insert(
        "last_error".to_string(),
        Value::String(sample.error.clone()),
    );
    health.insert(
        "last_sample_at".to_string(),
        Value::String(last_sample_at.to_string()),
    );
    health.insert("interval_seconds".to_string(), json!(interval_seconds));
    health.insert("pid".to_string(), json!(std::process::id()));
    health.insert(
        "rules_loaded_at".to_string(),
        Value::String(rules_loaded_at.to_string()),
    );
    health.insert(
        "collector_mode".to_string(),
        Value::String(collector_mode.to_string()),
    );
    health.insert("backend".to_string(), Value::String("rust".to_string()));
    if let Some(path) = event_socket {
        health.insert(
            "event_socket".to_string(),
            Value::String(path.display().to_string()),
        );
    }
    if !event_socket_error.is_empty() {
        health.insert(
            "event_socket_error".to_string(),
            Value::String(event_socket_error.to_string()),
        );
    }
    Value::Object(health)
}

pub fn fmt_seconds(seconds: f64) -> String {
    let seconds = seconds.round() as i64;
    let hours = seconds / 3600;
    let minutes = (seconds % 3600) / 60;
    let secs = seconds % 60;
    if hours > 0 {
        format!("{hours}h {minutes:02}m")
    } else if minutes > 0 {
        format!("{minutes}m {secs:02}s")
    } else {
        format!("{secs}s")
    }
}

pub fn today_key() -> String {
    Local::now().format("%Y-%m-%d").to_string()
}

fn date_key(value: NaiveDate) -> String {
    value.format("%Y-%m-%d").to_string()
}

fn parse_day_key(value: &str) -> Option<NaiveDate> {
    NaiveDate::parse_from_str(value, "%Y-%m-%d").ok()
}

fn week_start_for(value: NaiveDate) -> NaiveDate {
    value - ChronoDuration::days(value.weekday().num_days_from_monday() as i64)
}

fn week_end_for(start: NaiveDate) -> NaiveDate {
    start + ChronoDuration::days(6)
}

fn week_key_for_date(value: NaiveDate) -> String {
    date_key(week_start_for(value))
}

fn formatted_day(value: NaiveDate) -> String {
    let label = format!(
        "{} de {}",
        value.day(),
        MONTH_NAMES_PT[value.month0() as usize]
    );
    if date_key(value) == today_key() {
        format!("Hoje, {label}")
    } else {
        label
    }
}

fn formatted_week_range(start: NaiveDate) -> String {
    let end = week_end_for(start);
    let label = if start.month() == end.month() && start.year() == end.year() {
        format!(
            "{}-{} de {}",
            start.day(),
            end.day(),
            MONTH_NAMES_PT[start.month0() as usize]
        )
    } else if start.year() == end.year() {
        format!(
            "{} de {} - {} de {}",
            start.day(),
            MONTH_NAMES_PT[start.month0() as usize],
            end.day(),
            MONTH_NAMES_PT[end.month0() as usize]
        )
    } else {
        format!(
            "{} de {} de {} - {} de {} de {}",
            start.day(),
            MONTH_NAMES_PT[start.month0() as usize],
            start.year(),
            end.day(),
            MONTH_NAMES_PT[end.month0() as usize],
            end.year()
        )
    };
    if week_key_for_date(Local::now().date_naive()) == date_key(start) {
        format!("Esta semana, {label}")
    } else {
        label
    }
}

fn formatted_generated_at() -> String {
    format!("Atualizado hoje as {}", Local::now().format("%H:%M"))
}

fn value_seconds(value: &Value) -> f64 {
    value.get("seconds").and_then(Value::as_f64).unwrap_or(0.0)
}

fn sorted_usage_items(items: &Value) -> Vec<(String, Value)> {
    let mut rows: Vec<(String, Value)> = items
        .as_object()
        .map(|map| {
            map.iter()
                .filter(|(_, value)| value.is_object())
                .map(|(key, value)| (key.clone(), value.clone()))
                .collect()
        })
        .unwrap_or_default();
    rows.sort_by(|(_, left), (_, right)| {
        value_seconds(right)
            .partial_cmp(&value_seconds(left))
            .unwrap_or(std::cmp::Ordering::Equal)
    });
    rows
}

fn usage_rows(
    items: &Value,
    limit: usize,
    total_seconds: Option<f64>,
    metadata_items: Option<&Value>,
    include_app_icons: bool,
) -> Vec<Value> {
    usage_rows_with_visibility(
        items,
        limit,
        total_seconds,
        metadata_items,
        include_app_icons,
        None,
        true,
        false,
    )
}

fn usage_rows_with_visibility(
    items: &Value,
    limit: usize,
    total_seconds: Option<f64>,
    metadata_items: Option<&Value>,
    include_app_icons: bool,
    hidden_app_ids: Option<&HashSet<String>>,
    include_hidden: bool,
    annotate_hidden: bool,
) -> Vec<Value> {
    sorted_usage_items(items)
        .into_iter()
        .filter(|(key, _)| include_hidden || !hidden_app_ids.is_some_and(|ids| ids.contains(key)))
        .take(limit)
        .map(|(key, bucket)| {
            let mut row = bucket.as_object().cloned().unwrap_or_default();
            let hidden = hidden_app_ids.is_some_and(|ids| ids.contains(&key));
            if let Some(metadata_bucket) = metadata_items
                .and_then(|metadata| metadata.get(&key))
                .and_then(Value::as_object)
            {
                for metadata_key in APP_METADATA_KEYS {
                    let missing = row.get(metadata_key).is_none_or(|value| {
                        value.is_null()
                            || value.as_str().is_some_and(str::is_empty)
                            || value.as_bool() == Some(false)
                    });
                    if missing {
                        if let Some(value) = metadata_bucket.get(metadata_key) {
                            if !value.is_null() {
                                row.insert(metadata_key.to_string(), value.clone());
                            }
                        }
                    }
                }
            }
            let seconds = round3(value_seconds(&bucket));
            row.insert("id".to_string(), Value::String(key));
            row.insert("seconds".to_string(), json!(seconds));
            row.insert("duration".to_string(), Value::String(fmt_seconds(seconds)));
            if let Some(total) = total_seconds {
                row.insert(
                    "percent".to_string(),
                    json!(round6(seconds / total.max(1.0))),
                );
            }
            if include_app_icons {
                annotate_app_label(&mut row);
                annotate_app_icon(&mut row);
            }
            if annotate_hidden {
                row.insert("hidden".to_string(), json!(hidden));
            }
            Value::Object(row)
        })
        .collect()
}

fn round6(value: f64) -> f64 {
    (value * 1_000_000.0).round() / 1_000_000.0
}

fn annotate_app_label(row: &mut Map<String, Value>) {
    let has_label = row
        .get("label")
        .and_then(Value::as_str)
        .is_some_and(|value| !value.trim().is_empty());
    let app_id = row
        .get("id")
        .and_then(Value::as_str)
        .unwrap_or("")
        .to_string();
    let label = if has_label {
        row.get("label")
            .and_then(Value::as_str)
            .unwrap_or_default()
            .to_string()
    } else {
        app_display_label(row, &app_id)
    };
    if !has_label {
        row.insert("label".to_string(), Value::String(label.clone()));
    }
    if row
        .get("name")
        .and_then(Value::as_str)
        .is_none_or(|value| value.trim().is_empty())
    {
        row.insert("name".to_string(), Value::String(label));
    }
}

fn app_display_label(row: &Map<String, Value>, app_id: &str) -> String {
    let app_key = normalize(app_id);
    if let Some(label) = known_app_label(&app_key) {
        return label.to_string();
    }
    let class_key = normalize(row.get("class").and_then(Value::as_str).unwrap_or(&app_key));
    let title_key = normalize(
        row.get("last_title")
            .or_else(|| row.get("title"))
            .and_then(Value::as_str)
            .unwrap_or(""),
    );
    if looks_like_windows_game(&class_key, &title_key) {
        if let Some(label) = best_game_label(row) {
            return label;
        }
    }
    if let Some(label) = best_metadata_label(row) {
        return label;
    }
    prettify_app_id(app_id)
}

fn known_app_label(app_key: &str) -> Option<&'static str> {
    match app_key {
        "brave" => Some("Brave"),
        "chrome" => Some("Chrome"),
        "code" => Some("Visual Studio Code"),
        "discord" => Some("Discord"),
        "files" => Some("Finder"),
        "firefox" => Some("Firefox"),
        "heroic" => Some("Heroic"),
        "lutris" => Some("Lutris"),
        "prismlauncher" => Some("Prism Launcher"),
        "screentime" => Some("ScreenTime"),
        "settings" => Some("Astrea Settings"),
        "spotify" => Some("Spotify"),
        "steam" => Some("Steam"),
        "terminal" => Some("Terminal"),
        "telegram" => Some("Telegram"),
        "unknown" => Some("Unknown"),
        "weather" => Some("Weather"),
        "whatsapp" => Some("WhatsApp"),
        "zen" => Some("Zen Browser"),
        _ => None,
    }
}

fn best_game_label(row: &Map<String, Value>) -> Option<String> {
    ["last_title", "title", "initialTitle"]
        .into_iter()
        .filter_map(|key| row.get(key).and_then(Value::as_str))
        .filter_map(clean_game_title)
        .next()
}

fn clean_game_title(value: &str) -> Option<String> {
    let mut text = value.trim();
    for marker in [" - Build ", " – Build ", " — Build "] {
        if let Some((before, _)) = text.split_once(marker) {
            text = before.trim();
        }
    }
    if text.is_empty() {
        return None;
    }
    let lower = normalize(text);
    if matches!(
        lower.as_str(),
        "steam" | "iniciando..." | "launching..." | "starting..." | "unknown"
    ) {
        return None;
    }
    Some(text.to_string())
}

fn best_metadata_label(row: &Map<String, Value>) -> Option<String> {
    ["initialTitle", "title", "last_title"]
        .into_iter()
        .filter_map(|key| row.get(key).and_then(Value::as_str))
        .map(str::trim)
        .find(|value| !value.is_empty())
        .map(ToString::to_string)
}

fn prettify_app_id(app_id: &str) -> String {
    let trimmed = app_id.trim();
    if trimmed.is_empty() {
        return "Unknown".to_string();
    }
    let without_extension = trimmed
        .strip_suffix(".exe")
        .or_else(|| trimmed.strip_suffix(".EXE"))
        .unwrap_or(trimmed);
    if let Some(steam_id) = without_extension.strip_prefix("steam_app_") {
        return format!("Steam App {steam_id}");
    }
    let last_segment = without_extension
        .rsplit(['.', '/', '\\'])
        .find(|value| !value.is_empty())
        .unwrap_or(without_extension);
    title_case_app_id(last_segment)
}

fn title_case_app_id(value: &str) -> String {
    value
        .split(['-', '_', ' '])
        .filter(|part| !part.is_empty())
        .map(|part| {
            let mut chars = part.chars();
            let Some(first) = chars.next() else {
                return String::new();
            };
            format!("{}{}", first.to_uppercase(), chars.as_str().to_lowercase())
        })
        .collect::<Vec<_>>()
        .join(" ")
}

fn annotate_app_icon(row: &mut Map<String, Value>) {
    if ["icon", "iconSource", "astreaIcon", "astreaIconName"]
        .iter()
        .any(|key| {
            row.get(*key)
                .and_then(Value::as_str)
                .is_some_and(|value| !value.is_empty())
        })
    {
        return;
    }
    let needs_deep_icon = client_needs_deep_icon(row);
    let mut icon = if needs_deep_icon {
        resolve_deep_icon(row)
    } else {
        String::new()
    };
    if icon.is_empty() {
        let class_name = row
            .get("class")
            .or_else(|| row.get("initialClass"))
            .or_else(|| row.get("id"))
            .and_then(Value::as_str)
            .unwrap_or("");
        let title = row
            .get("last_title")
            .or_else(|| row.get("title"))
            .or_else(|| row.get("name"))
            .or_else(|| row.get("id"))
            .and_then(Value::as_str)
            .unwrap_or("");
        icon = alt_tab_icon_name_for_client(class_name, title);
    }
    if !icon.is_empty() {
        row.insert("icon".to_string(), Value::String(icon));
    } else if needs_deep_icon {
        row.insert("hideIconFallback".to_string(), json!(true));
    }
}

fn client_needs_deep_icon(row: &Map<String, Value>) -> bool {
    [
        "class",
        "initialClass",
        "title",
        "last_title",
        "initialTitle",
    ]
    .iter()
    .filter_map(|key| row.get(*key).and_then(Value::as_str))
    .collect::<Vec<_>>()
    .join(" ")
    .to_lowercase()
    .split_whitespace()
    .any(|part| {
        part.contains(".exe")
            || part.contains("wine")
            || part.contains("proton")
            || part.contains("pressure-vessel")
            || part.contains("steam_app_")
    })
}

fn resolve_deep_icon(row: &Map<String, Value>) -> String {
    let script = astrea_root()
        .join("Core")
        .join("bridge")
        .join("system")
        .join("app_icons.py");
    if !script.exists() {
        return String::new();
    }
    let class_name = row
        .get("class")
        .or_else(|| row.get("initialClass"))
        .and_then(Value::as_str)
        .unwrap_or("");
    let title = row
        .get("last_title")
        .or_else(|| row.get("title"))
        .or_else(|| row.get("id"))
        .and_then(Value::as_str)
        .unwrap_or("App");
    let payload = json!({
        "name": title,
        "title": title,
        "pid": row.get("pid").and_then(Value::as_u64).unwrap_or(0),
        "application.process.id": row.get("pid").and_then(Value::as_u64).unwrap_or(0),
        "application.name": title,
        "application.process.binary": class_name,
        "application.id": class_name,
        "node.name": title,
        "window.class": class_name,
        "window.initial_class": row.get("initialClass").and_then(Value::as_str).unwrap_or(""),
        "window.title": title,
        "window.initial_title": row.get("initialTitle").and_then(Value::as_str).unwrap_or(""),
    });
    let Ok(output) = Command::new("python3")
        .arg(script)
        .arg("resolve")
        .arg(payload.to_string())
        .output()
    else {
        return String::new();
    };
    if !output.status.success() || output.stdout.is_empty() {
        return String::new();
    }
    let Ok(data) = serde_json::from_slice::<Value>(&output.stdout) else {
        return String::new();
    };
    let icon_name = data.get("icon_name").and_then(Value::as_str).unwrap_or("");
    let icon_path = data.get("icon").and_then(Value::as_str).unwrap_or("");
    if !icon_path.is_empty() && icon_name != "audio-x-generic" {
        return icon_path.to_string();
    }
    if !icon_name.is_empty() && icon_name != "audio-x-generic" {
        return icon_name.to_string();
    }
    String::new()
}

fn astrea_root() -> PathBuf {
    env::var("ASTREA_ROOT")
        .map(PathBuf::from)
        .unwrap_or_else(|_| home_dir().join(".local").join("share").join("Astrea"))
}

fn astrea_launcher_icon_path(name: &str) -> String {
    home_dir()
        .join(".local")
        .join("share")
        .join("applications")
        .join("astrea-icons")
        .join(format!("{name}.png"))
        .to_string_lossy()
        .into_owned()
}

fn alt_tab_icon_name_for_client(class_name: &str, title: &str) -> String {
    let raw_class = class_name.trim();
    let cls = normalize(raw_class);
    let text = normalize(title);
    if cls == "org.vinegarhq.sober" {
        return "org.vinegarhq.Sober".to_string();
    }
    if cls.contains("zen") {
        return "zen-browser".to_string();
    }
    if cls.contains("kitty") {
        return "kitty".to_string();
    }
    if cls.contains("code") || cls.contains("cursor") {
        return "visual-studio-code".to_string();
    }
    if cls.contains("spotify") {
        return "spotify".to_string();
    }
    if cls.contains("discord") {
        return "discord".to_string();
    }
    if let Some(steam_id) = cls.strip_prefix("steam_app_") {
        if steam_id.chars().all(|value| value.is_ascii_digit()) {
            return format!("steam_icon_{steam_id}");
        }
    }
    if cls == "steam_app_default" {
        return String::new();
    }
    if cls.contains("steam") {
        return "steam".to_string();
    }
    if text.contains("settings") || text.contains("configura") {
        return astrea_launcher_icon_path("astrea-settings");
    }
    if text.contains("screen") && text.contains("time") {
        return "clock".to_string();
    }
    let desktop_icon = desktop_icon_for_client(&cls, &text);
    if !desktop_icon.is_empty() {
        return desktop_icon;
    }
    if text.contains("finder") {
        return "folder".to_string();
    }
    if text.contains("weather") || text.contains("clima") {
        return "weather-clear".to_string();
    }
    if cls.contains("org.quickshell") {
        return "application-x-executable".to_string();
    }
    if raw_class.contains('.') || raw_class.split_whitespace().count() > 1 {
        return "application-x-executable".to_string();
    }
    if is_safe_icon_name(&cls) {
        cls
    } else {
        "application-x-executable".to_string()
    }
}

fn is_safe_icon_name(value: &str) -> bool {
    !value.is_empty()
        && value
            .chars()
            .all(|ch| ch.is_ascii_alphanumeric() || matches!(ch, '-' | '_' | '.'))
}

#[derive(Clone, Debug)]
struct DesktopEntry {
    id: String,
    name: String,
    exec: String,
    icon: String,
}

static DESKTOP_ENTRIES: OnceLock<Vec<DesktopEntry>> = OnceLock::new();

fn desktop_entries() -> &'static Vec<DesktopEntry> {
    DESKTOP_ENTRIES.get_or_init(load_desktop_entries)
}

fn load_desktop_entries() -> Vec<DesktopEntry> {
    application_dirs()
        .into_iter()
        .filter_map(|dir| fs::read_dir(dir).ok())
        .flat_map(|entries| entries.filter_map(std::result::Result::ok))
        .map(|entry| entry.path())
        .filter(|path| {
            path.extension()
                .and_then(|extension| extension.to_str())
                .is_some_and(|extension| extension == "desktop")
        })
        .filter_map(|path| parse_desktop_entry(&path))
        .collect()
}

fn application_dirs() -> Vec<PathBuf> {
    let mut dirs = Vec::new();
    dirs.push(
        env::var("XDG_DATA_HOME")
            .map(PathBuf::from)
            .unwrap_or_else(|_| home_dir().join(".local").join("share"))
            .join("applications"),
    );
    for entry in env::var("XDG_DATA_DIRS")
        .unwrap_or_else(|_| "/usr/local/share:/usr/share".to_string())
        .split(':')
        .filter(|entry| !entry.is_empty())
    {
        let path = PathBuf::from(entry).join("applications");
        if !dirs.iter().any(|existing| existing == &path) {
            dirs.push(path);
        }
    }
    dirs
}

fn parse_desktop_entry(path: &Path) -> Option<DesktopEntry> {
    let text = fs::read_to_string(path).ok()?;
    let mut in_desktop_entry = false;
    let mut name = String::new();
    let mut exec = String::new();
    let mut icon = String::new();
    let mut no_display = String::new();
    for line in text.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        if line.starts_with('[') && line.ends_with(']') {
            in_desktop_entry = line == "[Desktop Entry]";
            continue;
        }
        if !in_desktop_entry {
            continue;
        }
        let Some((key, value)) = line.split_once('=') else {
            continue;
        };
        match key {
            "Name" => name = value.trim().to_string(),
            "Exec" => exec = value.trim().to_string(),
            "Icon" => icon = value.trim().to_string(),
            "NoDisplay" => no_display = value.trim().to_string(),
            _ => {}
        }
    }
    if no_display.eq_ignore_ascii_case("true") || icon.is_empty() {
        return None;
    }
    Some(DesktopEntry {
        id: path
            .file_name()
            .and_then(|name| name.to_str())
            .unwrap_or_default()
            .to_string(),
        name,
        exec,
        icon,
    })
}

fn desktop_icon_for_client(class_name: &str, title: &str) -> String {
    let cls = class_name.to_lowercase();
    let class_without_bin = cls.strip_suffix("-bin").unwrap_or(&cls);
    let text = title.to_lowercase();
    let mut best_icon = String::new();
    let mut best_score = 0;
    for entry in desktop_entries() {
        let entry_name = entry.name.to_lowercase();
        let entry_id = entry.id.to_lowercase();
        let entry_exec = entry.exec.to_lowercase();
        let haystack = format!("{entry_name} {entry_id} {entry_exec}");
        let mut score = 0;
        if !cls.is_empty() && (haystack.contains(&cls) || haystack.contains(class_without_bin)) {
            score = 3;
        }
        if !text.is_empty()
            && !entry_name.is_empty()
            && (entry_name.contains(&text) || text.contains(&entry_name))
        {
            score = score.max(if cls == "org.quickshell" { 4 } else { 2 });
        }
        if !text.is_empty() && entry_name == text {
            score = score.max(6);
        }
        if score > 0 && entry_id.contains("astrea-") {
            score += 1;
        }
        if score > best_score {
            best_score = score;
            best_icon = entry.icon.clone();
        }
    }
    best_icon
}

fn top_category_from(categories: &Value) -> (String, String) {
    let Some((category_id, bucket)) = sorted_usage_items(categories).into_iter().next() else {
        return (String::new(), String::new());
    };
    let label = bucket
        .get("label")
        .and_then(Value::as_str)
        .unwrap_or(&category_id)
        .to_string();
    (category_id, label)
}

fn hourly_rows(day_root: &Value) -> Vec<Value> {
    let hourly = day_root.get("hourly").unwrap_or(&Value::Null);
    (0..24)
        .map(|hour| {
            let bucket = hourly.get(hour.to_string()).unwrap_or(&Value::Null);
            let seconds = round3(value_seconds(bucket));
            let categories_root = bucket.get("categories").unwrap_or(&Value::Null);
            let categories = usage_rows(categories_root, 8, Some(seconds), None, false);
            let (top_category, top_category_label) = top_category_from(categories_root);
            json!({
                "hour": hour,
                "label": format!("{hour:02}:00"),
                "seconds": seconds,
                "duration": fmt_seconds(seconds),
                "active_seconds": round3(bucket.get("active_seconds").and_then(Value::as_f64).unwrap_or(0.0)),
                "unknown_seconds": round3(bucket.get("unknown_seconds").and_then(Value::as_f64).unwrap_or(0.0)),
                "categories": categories,
                "top_category": top_category,
                "top_category_label": top_category_label,
            })
        })
        .collect()
}

fn merge_usage_values(target: &mut Map<String, Value>, source: &Value) {
    let Some(source_map) = source.as_object() else {
        return;
    };
    for (item_id, raw_bucket) in source_map {
        let Some(raw_map) = raw_bucket.as_object() else {
            continue;
        };
        let bucket_value = target
            .entry(item_id.clone())
            .or_insert_with(|| json!({"seconds": 0.0}));
        if !bucket_value.is_object() {
            *bucket_value = json!({"seconds": 0.0});
        }
        let bucket = bucket_value
            .as_object_mut()
            .unwrap_or_else(|| unreachable!());
        let current = bucket.get("seconds").and_then(Value::as_f64).unwrap_or(0.0);
        bucket.insert(
            "seconds".to_string(),
            json!(round3(
                current
                    + raw_bucket
                        .get("seconds")
                        .and_then(Value::as_f64)
                        .unwrap_or(0.0)
            )),
        );
        if let Some(label) = raw_map.get("label") {
            bucket.insert("label".to_string(), label.clone());
        }
        for metadata_key in APP_METADATA_KEYS {
            if let Some(value) = raw_map.get(metadata_key) {
                if !value.is_null() {
                    bucket.insert(metadata_key.to_string(), value.clone());
                }
            }
        }
    }
}

fn week_history(state: &Value, selected_week_start: NaiveDate) -> Vec<Value> {
    let mut starts = vec![
        selected_week_start,
        week_start_for(Local::now().date_naive()),
    ];
    if let Some(weeks) = state.get("weeks").and_then(Value::as_object) {
        for key in weeks.keys() {
            if let Some(day) = parse_day_key(key) {
                starts.push(week_start_for(day));
            }
        }
    }
    if let Some(days) = state.get("days").and_then(Value::as_object) {
        for key in days.keys() {
            if let Some(day) = parse_day_key(key) {
                starts.push(week_start_for(day));
            }
        }
    }
    starts.sort();
    starts.dedup();
    let first = starts.first().copied().unwrap_or(selected_week_start);
    let last = starts.last().copied().unwrap_or(selected_week_start);
    let mut rows = Vec::new();
    let mut cursor = first;
    while cursor <= last {
        let key = date_key(cursor);
        let bucket = state
            .get("weeks")
            .and_then(|weeks| weeks.get(&key))
            .unwrap_or(&Value::Null);
        let seconds = round3(value_seconds(bucket));
        rows.push(json!({
            "start": key,
            "end": date_key(week_end_for(cursor)),
            "label": formatted_week_range(cursor),
            "seconds": seconds,
            "duration": fmt_seconds(seconds),
            "selected": cursor == selected_week_start,
            "has_data": seconds > 0.0,
        }));
        cursor += ChronoDuration::days(7);
    }
    rows
}

fn week_snapshot(
    state: &Value,
    selected_week_start: NaiveDate,
    limit: usize,
    history: &[Value],
    hidden_app_ids: &HashSet<String>,
) -> Value {
    let start = week_start_for(selected_week_start);
    let end = week_end_for(start);
    let start_key = date_key(start);
    let week_root = state
        .get("weeks")
        .and_then(|weeks| weeks.get(&start_key))
        .unwrap_or(&Value::Null);
    let use_week_root = value_seconds(week_root) > 0.0
        || week_root.get("apps").is_some()
        || week_root.get("categories").is_some();
    let mut week_apps = if use_week_root {
        week_root
            .get("apps")
            .and_then(Value::as_object)
            .cloned()
            .unwrap_or_default()
    } else {
        Map::new()
    };
    let mut week_categories = if use_week_root {
        week_root
            .get("categories")
            .and_then(Value::as_object)
            .cloned()
            .unwrap_or_default()
    } else {
        Map::new()
    };
    let mut week_days = Vec::new();
    for offset in 0..7 {
        let current_day = start + ChronoDuration::days(offset);
        let key = date_key(current_day);
        let day_root = state
            .get("days")
            .and_then(|days| days.get(&key))
            .unwrap_or(&Value::Null);
        let seconds = round3(value_seconds(day_root));
        if !use_week_root {
            merge_usage_values(&mut week_apps, day_root.get("apps").unwrap_or(&Value::Null));
            merge_usage_values(
                &mut week_categories,
                day_root.get("categories").unwrap_or(&Value::Null),
            );
        }
        let weekday_index = current_day.weekday().num_days_from_monday() as usize;
        week_days.push(json!({
            "date": key,
            "label": WEEKDAY_SHORT_PT[weekday_index],
            "short_label": WEEKDAY_SHORT_PT[weekday_index],
            "seconds": seconds,
            "duration": fmt_seconds(seconds),
            "categories": usage_rows(day_root.get("categories").unwrap_or(&Value::Null), limit, Some(seconds), None, false),
        }));
    }
    let days_total = week_days.iter().map(value_seconds).sum::<f64>();
    let week_total = round3(if value_seconds(week_root) > 0.0 {
        value_seconds(week_root)
    } else {
        days_total
    });
    let selected_index = history
        .iter()
        .position(|row| row.get("start").and_then(Value::as_str) == Some(start_key.as_str()))
        .map(|index| index as i64)
        .unwrap_or(-1);
    let previous_start = if selected_index > 0 {
        history
            .get((selected_index - 1) as usize)
            .and_then(|row| row.get("start"))
            .and_then(Value::as_str)
            .unwrap_or("")
    } else {
        ""
    };
    let next_start =
        if selected_index >= 0 && (selected_index as usize) < history.len().saturating_sub(1) {
            history
                .get(selected_index as usize + 1)
                .and_then(|row| row.get("start"))
                .and_then(Value::as_str)
                .unwrap_or("")
        } else {
            ""
        };
    json!({
        "start": start_key,
        "end": date_key(end),
        "label": formatted_week_range(start),
        "seconds": week_total,
        "duration": fmt_seconds(week_total),
        "days": week_days,
        "apps": usage_rows_with_visibility(&Value::Object(week_apps), limit, Some(week_total), state.get("apps"), true, Some(hidden_app_ids), false, false),
        "categories": usage_rows(&Value::Object(week_categories), limit, Some(week_total), None, false),
        "history_index": selected_index,
        "history_count": history.len(),
        "previous_start": previous_start,
        "next_start": next_start,
    })
}

fn pid_alive(pid: u64) -> bool {
    if pid == 0 {
        return false;
    }
    let result = unsafe { libc::kill(pid as libc::pid_t, 0) };
    if result == 0 {
        return true;
    }
    let errno = unsafe { *libc::__errno_location() };
    errno == libc::EPERM
}

fn effective_health(raw_health: &Value) -> Value {
    let mut health = raw_health.as_object().cloned().unwrap_or_default();
    let running = health
        .get("running")
        .and_then(Value::as_bool)
        .unwrap_or(false);
    let interval = health
        .get("interval_seconds")
        .and_then(Value::as_f64)
        .unwrap_or(DEFAULT_INTERVAL_SECONDS)
        .max(1.0);
    let stale_after = HEALTH_STALE_FLOOR_SECONDS.max(interval * 4.0);
    let sample_at_text = health
        .get("last_sample_at")
        .and_then(Value::as_str)
        .unwrap_or("");
    let sample_age = DateTime::parse_from_rfc3339(sample_at_text)
        .ok()
        .map(|sample_at| {
            (Utc::now() - sample_at.with_timezone(&Utc)).num_milliseconds() as f64 / 1000.0
        })
        .map(|age| age.max(0.0));
    let stale = running && sample_age.is_none_or(|age| age > stale_after);
    let pid = health.get("pid").and_then(Value::as_u64).unwrap_or(0);
    let alive = pid_alive(pid);
    health.insert("running".to_string(), json!(running && !stale));
    health.insert("stale".to_string(), json!(stale));
    health.insert("pid_alive".to_string(), json!(alive));
    health.insert(
        "sample_age_seconds".to_string(),
        sample_age.map_or(Value::Null, |age| json!(round3(age))),
    );
    if stale
        && health
            .get("last_error")
            .and_then(Value::as_str)
            .unwrap_or("")
            .trim()
            .is_empty()
    {
        health.insert(
            "last_error".to_string(),
            Value::String("collector stale".to_string()),
        );
    }
    Value::Object(health)
}

fn health_label(health: &Value) -> &'static str {
    if health
        .get("stale")
        .and_then(Value::as_bool)
        .unwrap_or(false)
    {
        "Coletor sem atualizacao"
    } else if !health
        .get("last_error")
        .and_then(Value::as_str)
        .unwrap_or("")
        .trim()
        .is_empty()
    {
        "Coletor degradado"
    } else if health
        .get("running")
        .and_then(Value::as_bool)
        .unwrap_or(false)
    {
        "Coletor ativo"
    } else {
        "Coletor parado"
    }
}

#[derive(Debug)]
struct CommandResult {
    success: bool,
    stdout: String,
    stderr: String,
    code: Option<i32>,
}

fn run_systemctl_user(args: &[&str], tolerate_failure: bool) -> Result<CommandResult> {
    let output = Command::new("systemctl").arg("--user").args(args).output();
    match output {
        Ok(output) => {
            let result = CommandResult {
                success: output.status.success(),
                stdout: String::from_utf8_lossy(&output.stdout).to_string(),
                stderr: String::from_utf8_lossy(&output.stderr).to_string(),
                code: output.status.code(),
            };
            if !result.success && !tolerate_failure {
                return Err(BackendError::Command(command_error_message(&result)));
            }
            Ok(result)
        }
        Err(error) if tolerate_failure => Ok(CommandResult {
            success: false,
            stdout: String::new(),
            stderr: error.to_string(),
            code: None,
        }),
        Err(error) => Err(BackendError::Command(error.to_string())),
    }
}

fn command_error_message(result: &CommandResult) -> String {
    let message = result
        .stderr
        .trim()
        .split('\n')
        .next()
        .filter(|line| !line.is_empty())
        .or_else(|| {
            result
                .stdout
                .trim()
                .split('\n')
                .next()
                .filter(|line| !line.is_empty())
        })
        .unwrap_or("systemctl failed");
    match result.code {
        Some(code) => format!("{message} (exit {code})"),
        None => message.to_string(),
    }
}

fn parse_systemctl_show(output: &str) -> HashMap<String, String> {
    output
        .lines()
        .filter_map(|line| line.split_once('='))
        .map(|(key, value)| (key.to_string(), value.to_string()))
        .collect()
}

fn service_display_state(active: bool, enabled: bool, installed: bool) -> &'static str {
    if active && enabled {
        "Ativo"
    } else if active {
        "Rodando"
    } else if enabled {
        "Habilitado"
    } else if installed {
        "Desativado"
    } else {
        "Nao instalado"
    }
}

pub fn service_status(unit_name: &str) -> Value {
    let result =
        run_systemctl_user(&["show", unit_name], true).unwrap_or_else(|error| CommandResult {
            success: false,
            stdout: String::new(),
            stderr: error.to_string(),
            code: None,
        });
    let props = parse_systemctl_show(&result.stdout);
    let load_state = props
        .get("LoadState")
        .map(String::as_str)
        .unwrap_or("not-found");
    let active_state = props
        .get("ActiveState")
        .map(String::as_str)
        .unwrap_or("inactive");
    let unit_file_state = props
        .get("UnitFileState")
        .map(String::as_str)
        .unwrap_or("disabled");
    let fragment_path = props.get("FragmentPath").map(String::as_str).unwrap_or("");
    let installed = !matches!(load_state, "not-found" | "bad") || !fragment_path.is_empty();
    let enabled = matches!(unit_file_state, "enabled" | "enabled-runtime");
    let active = active_state == "active";
    let last_error = if result.success {
        String::new()
    } else {
        command_error_message(&result)
    };
    json!({
        "unit": unit_name,
        "installed": installed,
        "enabled": enabled,
        "active": active,
        "load_state": load_state,
        "active_state": active_state,
        "sub_state": props.get("SubState").cloned().unwrap_or_default(),
        "unit_file_state": unit_file_state,
        "fragment_path": fragment_path,
        "last_error": last_error,
        "display": {
            "state": service_display_state(active, enabled, installed),
        },
    })
}

fn service_status_with_peer() -> Value {
    let peer = service_status(PYTHON_SERVICE_NAME);
    let peer_active = peer.get("active").and_then(Value::as_bool).unwrap_or(false);
    let mut status = service_status(RUST_SERVICE_NAME);
    if let Some(map) = status.as_object_mut() {
        let rust_active = map.get("active").and_then(Value::as_bool).unwrap_or(false);
        let rust_enabled = map.get("enabled").and_then(Value::as_bool).unwrap_or(false);
        if peer_active && !rust_active && !rust_enabled {
            let display = child_object_mut(map, "display");
            display.insert(
                "state".to_string(),
                Value::String("Legacy-Backend ativo".to_string()),
            );
            display.insert(
                "unit".to_string(),
                Value::String(PYTHON_SERVICE_NAME.to_string()),
            );
        }
        map.insert("peer".to_string(), peer);
        map.insert("peer_active".to_string(), json!(peer_active));
        map.insert(
            "collector_active".to_string(),
            json!(rust_active || peer_active),
        );
    }
    status
}

fn xdg_config_home() -> PathBuf {
    env::var("XDG_CONFIG_HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|_| home_dir().join(".config"))
}

fn user_unit_dir() -> PathBuf {
    xdg_config_home().join("systemd").join("user")
}

fn service_unit_source(unit_name: &str) -> PathBuf {
    project_dir().join(unit_name)
}

fn service_unit_link(unit_name: &str) -> PathBuf {
    user_unit_dir().join(unit_name)
}

fn install_service_unit(unit_name: &str) -> Result<()> {
    let source = service_unit_source(unit_name);
    if !source.exists() {
        return Err(BackendError::Command(format!(
            "service unit source is missing: {}",
            source.display()
        )));
    }
    let link = service_unit_link(unit_name);
    if let Some(parent) = link.parent() {
        fs::create_dir_all(parent)?;
    }
    if let Ok(existing) = fs::read_link(&link) {
        if fs::canonicalize(existing).ok() == fs::canonicalize(&source).ok() {
            return Ok(());
        }
    }
    if fs::symlink_metadata(&link).is_ok() {
        fs::remove_file(&link)?;
    }
    unix_fs::symlink(source, link)?;
    Ok(())
}

fn install_service_unit_if_present(unit_name: &str) -> Result<()> {
    if service_unit_source(unit_name).exists() {
        install_service_unit(unit_name)?;
    }
    Ok(())
}

fn remove_legacy_service_units() {
    for unit in LEGACY_SERVICE_NAMES {
        let _ = run_systemctl_user(&["disable", "--now", unit], true);
        let link = service_unit_link(unit);
        if fs::symlink_metadata(&link).is_ok() {
            let _ = fs::remove_file(link);
        }
    }
}

fn import_service_environment() {
    let mut args = Vec::with_capacity(1 + SERVICE_ENV_KEYS.len());
    args.push("import-environment");
    args.extend(SERVICE_ENV_KEYS);
    let _ = run_systemctl_user(&args, true);
}

fn prepare_rust_service() -> Result<()> {
    install_service_unit(RUST_SERVICE_NAME)?;
    install_service_unit_if_present(PYTHON_SERVICE_NAME)?;
    remove_legacy_service_units();
    import_service_environment();
    run_systemctl_user(&["daemon-reload"], false)?;
    Ok(())
}

fn mark_service_result(mut result: Value, action: &str, ok: bool, error: Option<String>) -> Value {
    if let Some(map) = result.as_object_mut() {
        map.insert("ok".to_string(), json!(ok));
        map.insert("action".to_string(), Value::String(action.to_string()));
        if let Some(error) = error {
            map.insert("last_error".to_string(), Value::String(error));
        }
    }
    result
}

pub fn service_action(action: &str) -> Result<Value> {
    if action == "status" {
        return Ok(mark_service_result(
            service_status_with_peer(),
            action,
            true,
            None,
        ));
    }

    match action {
        "enable" | "start" | "restart" => {
            prepare_rust_service()?;
            let _ = run_systemctl_user(&["disable", "--now", PYTHON_SERVICE_NAME], true);
        }
        "disable" | "stop" => {
            remove_legacy_service_units();
        }
        _ => return Err(BackendError::UnsupportedServiceAction(action.to_string())),
    }

    match action {
        "enable" => {
            run_systemctl_user(&["enable", "--now", RUST_SERVICE_NAME], false)?;
        }
        "disable" => {
            let _ = run_systemctl_user(&["disable", "--now", RUST_SERVICE_NAME], true);
            let _ = run_systemctl_user(&["disable", "--now", PYTHON_SERVICE_NAME], true);
            let _ = run_systemctl_user(&["daemon-reload"], true);
        }
        "start" => {
            run_systemctl_user(&["start", RUST_SERVICE_NAME], false)?;
        }
        "stop" => {
            let _ = run_systemctl_user(&["stop", RUST_SERVICE_NAME], true);
            let _ = run_systemctl_user(&["stop", PYTHON_SERVICE_NAME], true);
        }
        "restart" => {
            run_systemctl_user(&["restart", RUST_SERVICE_NAME], false)?;
        }
        _ => unreachable!(),
    }

    Ok(mark_service_result(
        service_status_with_peer(),
        action,
        true,
        None,
    ))
}

pub fn snapshot_from_state(
    state: Value,
    paths: &Paths,
    day: Option<&str>,
    week: Option<&str>,
    limit: usize,
    service: Value,
    settings: Value,
) -> Value {
    let state = migrate_state(state);
    let settings = migrate_settings(settings);
    let hidden_app_ids = hidden_apps_set(&settings);
    let selected_date = day
        .and_then(parse_day_key)
        .unwrap_or_else(|| Local::now().date_naive());
    let selected_day = day
        .map(ToString::to_string)
        .unwrap_or_else(|| date_key(selected_date));
    let selected_week_start = week
        .and_then(parse_day_key)
        .map(week_start_for)
        .unwrap_or_else(|| week_start_for(selected_date));
    let week_rows = week_history(&state, selected_week_start);
    let day_root = state
        .get("days")
        .and_then(|days| days.get(&selected_day))
        .unwrap_or(&Value::Null);
    let day_seconds = round3(value_seconds(day_root));
    let day_categories = usage_rows(
        day_root.get("categories").unwrap_or(&Value::Null),
        limit,
        Some(day_seconds),
        None,
        false,
    );
    let day_apps = usage_rows_with_visibility(
        day_root.get("apps").unwrap_or(&Value::Null),
        limit,
        Some(day_seconds),
        state.get("apps"),
        true,
        Some(&hidden_app_ids),
        false,
        false,
    );
    let health = effective_health(state.get("health").unwrap_or(&Value::Null));
    let health_display = health_label(&health);
    let all_apps = usage_rows_with_visibility(
        state.get("apps").unwrap_or(&Value::Null),
        limit,
        None,
        None,
        true,
        Some(&hidden_app_ids),
        false,
        false,
    );
    let settings_apps = usage_rows_with_visibility(
        state.get("apps").unwrap_or(&Value::Null),
        usize::MAX,
        None,
        None,
        true,
        Some(&hidden_app_ids),
        true,
        true,
    );
    let mut settings_payload = settings.as_object().cloned().unwrap_or_default();
    settings_payload.insert("apps".to_string(), Value::Array(settings_apps));
    settings_payload.insert("hidden_app_count".to_string(), json!(hidden_app_ids.len()));
    json!({
        "schema_version": state.get("schema_version").and_then(Value::as_u64).unwrap_or(STATE_SCHEMA_VERSION),
        "generated_at": now_iso_utc(),
        "state_path": paths.state_path.display().to_string(),
        "events_path": paths.events_path.display().to_string(),
        "rules_path": paths.rules_path.display().to_string(),
        "settings_path": paths.settings_path.display().to_string(),
        "selected_day": selected_day,
        "selected_week": date_key(selected_week_start),
        "current": state.get("current").cloned().unwrap_or_else(|| json!({})),
        "health": health,
        "service": service,
        "settings": Value::Object(settings_payload),
        "display": {
            "selected_day": formatted_day(selected_date),
            "selected_week": formatted_week_range(selected_week_start),
            "generated_at": formatted_generated_at(),
            "health": health_display,
        },
        "totals": {
            "seconds": round3(state.get("total_seconds").and_then(Value::as_f64).unwrap_or(0.0)),
            "active_seconds": round3(state.get("active_seconds").and_then(Value::as_f64).unwrap_or(0.0)),
            "unknown_seconds": round3(state.get("unknown_seconds").and_then(Value::as_f64).unwrap_or(0.0)),
            "duration": fmt_seconds(state.get("total_seconds").and_then(Value::as_f64).unwrap_or(0.0)),
            "active_duration": fmt_seconds(state.get("active_seconds").and_then(Value::as_f64).unwrap_or(0.0)),
            "unknown_duration": fmt_seconds(state.get("unknown_seconds").and_then(Value::as_f64).unwrap_or(0.0)),
        },
        "day": {
            "seconds": day_seconds,
            "active_seconds": round3(day_root.get("active_seconds").and_then(Value::as_f64).unwrap_or(0.0)),
            "unknown_seconds": round3(day_root.get("unknown_seconds").and_then(Value::as_f64).unwrap_or(0.0)),
            "duration": fmt_seconds(day_seconds),
            "active_duration": fmt_seconds(day_root.get("active_seconds").and_then(Value::as_f64).unwrap_or(0.0)),
            "unknown_duration": fmt_seconds(day_root.get("unknown_seconds").and_then(Value::as_f64).unwrap_or(0.0)),
            "categories": day_categories,
            "apps": day_apps,
            "top_categories": day_categories,
            "top_apps": day_apps,
            "hourly": hourly_rows(day_root),
        },
        "week": week_snapshot(&state, selected_week_start, limit, &week_rows, &hidden_app_ids),
        "weeks": week_rows,
        "all_time": {
            "categories": usage_rows(state.get("categories").unwrap_or(&Value::Null), limit, None, None, false),
            "apps": all_apps,
        },
        "sample_count": state.get("sample_count").and_then(Value::as_u64).unwrap_or(0),
        "error_count": state.get("error_count").and_then(Value::as_u64).unwrap_or(0),
    })
}

pub fn snapshot_json(paths: &Paths, day: Option<&str>, week: Option<&str>, limit: usize) -> Value {
    let state = load_json(&paths.state_path, empty_state());
    snapshot_from_state(
        state,
        paths,
        day,
        week,
        limit,
        service_status_with_peer(),
        load_settings(&paths.settings_path),
    )
}

pub fn run_monitor(options: MonitorOptions) -> Result<()> {
    let interval_seconds = options.interval_seconds.max(1.0);
    let interval = Duration::from_secs_f64(interval_seconds);
    let _lock = FileLock::acquire(&options.paths.lock_path)?;
    let mut state = migrate_state(load_json(&options.paths.state_path, empty_state()));
    let mut rules = load_rules(&options.paths.rules_path);
    let mut loaded_rules_mtime = rules_mtime(&options.paths.rules_path);
    let rules_loaded_at = now_iso_utc();
    let mut event_stream = open_event_stream();
    let mut event_socket_error = String::new();
    let mut next_event_socket_retry =
        Instant::now() + Duration::from_secs_f64(EVENT_SOCKET_RETRY_SECONDS);
    let mut previous_sample = build_sample(&rules);
    let mut previous_instant = Instant::now();
    let mut previous_wall = current_unix_seconds();
    let mut next_flush = previous_instant + interval;

    {
        let root = object_mut(&mut state);
        root.insert(
            "current".to_string(),
            serde_json::to_value(&previous_sample)?,
        );
        root.insert(
            "health".to_string(),
            monitor_health_payload(
                &previous_sample,
                interval_seconds,
                &rules_loaded_at,
                if event_stream.is_some() {
                    "rust-event"
                } else {
                    "rust-polling"
                },
                &previous_sample.sampled_at,
                event_stream.as_ref().map(HyprlandEventStream::socket_path),
                &event_socket_error,
            ),
        );
    }
    atomic_write_json(&options.paths.state_path, &state)?;
    append_event(&options.paths, None, &previous_sample)?;

    while !should_stop() {
        let mut event_line = None;
        let mut waited_with_polling = false;
        if let Some(stream) = event_stream.as_mut() {
            let timeout = next_flush
                .saturating_duration_since(Instant::now())
                .min(stop_check_interval());
            match stream.read_line(timeout) {
                Ok(line) => event_line = line,
                Err(error) => {
                    event_socket_error = error.to_string();
                    event_stream = None;
                    next_event_socket_retry =
                        Instant::now() + Duration::from_secs_f64(EVENT_SOCKET_RETRY_SECONDS);
                }
            }
        } else {
            waited_with_polling = true;
            sleep_until_or_stop(interval);
            if Instant::now() >= next_event_socket_retry {
                event_stream = open_event_stream();
                if event_stream.is_some() {
                    event_socket_error.clear();
                }
                next_event_socket_retry =
                    Instant::now() + Duration::from_secs_f64(EVENT_SOCKET_RETRY_SECONDS);
            }
        }

        let current_rules_mtime = rules_mtime(&options.paths.rules_path);
        let rules_changed = current_rules_mtime != loaded_rules_mtime;
        if rules_changed {
            rules = load_rules(&options.paths.rules_path);
            loaded_rules_mtime = current_rules_mtime;
        }

        let current_instant = Instant::now();
        let current_wall = current_unix_seconds();
        let focus_event = event_line.as_deref().is_some_and(|line| {
            event_stream.is_some() && hyprland_event_requires_sample(line, &previous_sample)
        });
        let due_flush = current_instant >= next_flush;
        let polling_tick = waited_with_polling || event_stream.is_none();

        if event_line.is_some() && !focus_event && !due_flush && !rules_changed {
            continue;
        }

        let should_resample = polling_tick || focus_event || rules_changed;
        let current_sample = if should_resample {
            build_sample(&rules)
        } else {
            previous_sample.clone()
        };
        if focus_event
            && !due_flush
            && !rules_changed
            && samples_same_window(&previous_sample, &current_sample)
        {
            continue;
        }

        commit_elapsed_sample(
            &mut state,
            &previous_sample,
            previous_wall,
            previous_instant,
            current_instant,
        );
        if current_sample.error.is_empty() {
            // No-op: this shape mirrors Python and keeps error_count untouched on healthy samples.
        } else {
            let root = object_mut(&mut state);
            add_u64(root, "error_count", 1);
        }
        if should_resample && sample_changed(&previous_sample, &current_sample) {
            append_event(&options.paths, Some(&previous_sample), &current_sample)?;
        }

        let last_sample_at = if should_resample {
            current_sample.sampled_at.clone()
        } else {
            now_iso_utc()
        };
        let mode = if event_stream.is_some() {
            "rust-event"
        } else {
            "rust-polling"
        };
        {
            let root = object_mut(&mut state);
            root.insert("updated_at".to_string(), Value::String(now_iso_utc()));
            root.insert(
                "current".to_string(),
                serde_json::to_value(&current_sample)?,
            );
            root.insert(
                "health".to_string(),
                monitor_health_payload(
                    &current_sample,
                    interval_seconds,
                    &rules_loaded_at,
                    mode,
                    &last_sample_at,
                    event_stream.as_ref().map(HyprlandEventStream::socket_path),
                    &event_socket_error,
                ),
            );
        }
        atomic_write_json(&options.paths.state_path, &state)?;
        previous_sample = current_sample;
        previous_instant = current_instant;
        previous_wall = current_wall;
        next_flush = current_instant + interval;
    }

    if let Some(health) = state.get_mut("health").and_then(Value::as_object_mut) {
        health.insert("running".to_string(), json!(false));
    }
    if let Some(root) = state.as_object_mut() {
        root.insert("updated_at".to_string(), Value::String(now_iso_utc()));
    }
    atomic_write_json(&options.paths.state_path, &state)?;
    Ok(())
}

fn open_event_stream() -> Option<HyprlandEventStream> {
    hyprland_event_socket_path().and_then(|path| HyprlandEventStream::connect(path).ok())
}

fn commit_elapsed_sample(
    state: &mut Value,
    sample: &Sample,
    previous_wall: f64,
    previous_instant: Instant,
    current_instant: Instant,
) {
    let elapsed = current_instant
        .saturating_duration_since(previous_instant)
        .as_secs_f64()
        .clamp(0.0, MAX_SAMPLE_SECONDS);
    if elapsed <= 0.0 {
        return;
    }
    add_elapsed(state, sample, previous_wall, previous_wall + elapsed);
    let root = object_mut(state);
    add_u64(root, "sample_count", 1);
}

fn current_unix_seconds() -> f64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_secs_f64())
        .unwrap_or(0.0)
}

fn stop_check_interval() -> Duration {
    Duration::from_millis(STOP_CHECK_INTERVAL_MILLIS)
}

fn sleep_until_or_stop(duration: Duration) {
    let started = Instant::now();
    while !should_stop() {
        let remaining = duration.saturating_sub(started.elapsed());
        if remaining.is_zero() {
            break;
        }
        std::thread::sleep(remaining.min(stop_check_interval()));
    }
}

fn should_stop() -> bool {
    STOP_REQUESTED.load(std::sync::atomic::Ordering::Relaxed)
}

static STOP_REQUESTED: std::sync::atomic::AtomicBool = std::sync::atomic::AtomicBool::new(false);

extern "C" fn handle_stop_signal(_signal: libc::c_int) {
    STOP_REQUESTED.store(true, std::sync::atomic::Ordering::Relaxed);
}

pub fn install_signal_handlers() {
    unsafe {
        libc::signal(
            libc::SIGINT,
            handle_stop_signal as *const () as libc::sighandler_t,
        );
        libc::signal(
            libc::SIGTERM,
            handle_stop_signal as *const () as libc::sighandler_t,
        );
    }
}

pub fn status_json(paths: &Paths) -> Value {
    let state = migrate_state(load_json(&paths.state_path, empty_state()));
    let health = effective_health(state.get("health").unwrap_or(&Value::Null));
    let health_display = health_label(&health);
    let service = service_status_with_peer();
    let service_display = service
        .get("display")
        .and_then(|display| display.get("state"))
        .and_then(Value::as_str)
        .unwrap_or("")
        .to_string();
    json!({
        "backend": "rust",
        "schema_version": state.get("schema_version").and_then(Value::as_u64).unwrap_or(STATE_SCHEMA_VERSION),
        "state_path": paths.state_path.display().to_string(),
        "events_path": paths.events_path.display().to_string(),
        "rules_path": paths.rules_path.display().to_string(),
        "lock_path": paths.lock_path.display().to_string(),
        "settings_path": paths.settings_path.display().to_string(),
        "sample_count": state.get("sample_count").and_then(Value::as_u64).unwrap_or(0),
        "error_count": state.get("error_count").and_then(Value::as_u64).unwrap_or(0),
        "current": state.get("current").cloned().unwrap_or_else(|| json!({})),
        "health": health,
        "service": service,
        "display": {
            "health": health_display,
            "service": service_display,
            "generated_at": formatted_generated_at(),
        },
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    struct StopRequestReset;

    impl Drop for StopRequestReset {
        fn drop(&mut self) {
            STOP_REQUESTED.store(false, std::sync::atomic::Ordering::Relaxed);
        }
    }

    #[test]
    fn polling_wait_should_return_promptly_when_stop_is_requested() {
        let _reset = StopRequestReset;
        STOP_REQUESTED.store(false, std::sync::atomic::Ordering::Relaxed);

        let stopper = std::thread::spawn(|| {
            std::thread::sleep(Duration::from_millis(20));
            STOP_REQUESTED.store(true, std::sync::atomic::Ordering::Relaxed);
        });
        let started = Instant::now();

        sleep_until_or_stop(Duration::from_secs(5));

        stopper.join().unwrap();
        assert!(started.elapsed() < Duration::from_millis(500));
    }

    #[test]
    fn focus_filter_should_track_focus_related_events() {
        assert!(hyprland_event_changes_focus("activewindow>>kitty,agony"));
        assert!(hyprland_event_changes_focus("activewindowv2>>0x123"));
        assert!(hyprland_event_changes_focus("workspace>>2"));
        assert!(!hyprland_event_changes_focus("windowtitle>>0x123"));
        assert!(!hyprland_event_changes_focus(""));
    }

    #[test]
    fn event_requires_sample_should_ignore_same_window_noise() {
        let sample = Sample {
            address: "0xabc".to_string(),
            class: "kitty".to_string(),
            app: "terminal".to_string(),
            ..Sample::default()
        };

        assert!(!hyprland_event_requires_sample(
            "activewindowv2>>abc",
            &sample
        ));
        assert!(!hyprland_event_requires_sample(
            "activewindow>>kitty,spinner",
            &sample
        ));
        assert!(!hyprland_event_requires_sample("windowtitle>>abc", &sample));
        assert!(hyprland_event_requires_sample(
            "activewindowv2>>def",
            &sample
        ));
        assert!(hyprland_event_requires_sample(
            "activewindow>>zen,ChatGPT",
            &sample
        ));
        assert!(hyprland_event_requires_sample("closewindow>>abc", &sample));
    }

    #[test]
    fn sample_changed_should_ignore_title_only_changes() {
        let previous = Sample {
            address: "0xabc".to_string(),
            app: "terminal".to_string(),
            class: "kitty".to_string(),
            title: "old".to_string(),
            ..Sample::default()
        };
        let current = Sample {
            address: "abc".to_string(),
            app: "terminal".to_string(),
            class: "kitty".to_string(),
            title: "new".to_string(),
            ..Sample::default()
        };
        let next_app = Sample {
            address: "def".to_string(),
            app: "zen".to_string(),
            class: "zen".to_string(),
            title: "ChatGPT".to_string(),
            ..Sample::default()
        };

        assert!(samples_same_window(&previous, &current));
        assert!(!sample_changed(&previous, &current));
        assert!(!samples_same_window(&previous, &next_app));
        assert!(sample_changed(&previous, &next_app));
    }

    #[test]
    fn resolve_app_should_match_class_and_title_aliases() {
        let rules = json!({
            "categories": {
                "development": {
                    "label": "Development",
                    "apps": {
                        "terminal": ["kitty"]
                    }
                },
                "utilities": {
                    "label": "Utilities",
                    "apps": {
                        "screentime": {"title_aliases": ["ScreenTime"]}
                    }
                },
                "other": {"label": "Other", "apps": {}}
            }
        });

        assert_eq!(
            resolve_app("kitty", "agony", &rules),
            (
                "terminal".to_string(),
                "development".to_string(),
                "Development".to_string()
            )
        );
        assert_eq!(
            resolve_app("org.quickshell", "ScreenTime", &rules),
            (
                "screentime".to_string(),
                "utilities".to_string(),
                "Utilities".to_string()
            )
        );
        assert_eq!(
            resolve_app("", "", &rules),
            (
                "unknown".to_string(),
                "other".to_string(),
                "Other".to_string()
            )
        );
    }

    #[test]
    fn resolve_app_should_classify_windows_executables_as_games() {
        let rules = json!({
            "categories": {
                "games": {"label": "Games", "apps": {}},
                "other": {"label": "Other", "apps": {}}
            }
        });

        assert_eq!(
            resolve_app("R.E.P.O.exe", "R.E.P.O.", &rules),
            (
                "r.e.p.o.exe".to_string(),
                "games".to_string(),
                "Games".to_string()
            )
        );
        assert_eq!(
            resolve_app("steam_app_3241660", "R.E.P.O.", &rules),
            (
                "steam_app_3241660".to_string(),
                "games".to_string(),
                "Games".to_string()
            )
        );
    }

    #[test]
    fn resolve_app_should_not_collapse_steam_app_windows_into_steam() {
        let rules = json!({
            "categories": {
                "games": {
                    "label": "Games",
                    "apps": {
                        "steam": ["steam", "steamwebhelper"]
                    }
                },
                "other": {"label": "Other", "apps": {}}
            }
        });

        assert_eq!(
            resolve_app("steam_app_1962700", "Subnautica 2 - Build 115506", &rules),
            (
                "steam_app_1962700".to_string(),
                "games".to_string(),
                "Games".to_string()
            )
        );
    }

    #[test]
    fn process_tokens_should_classify_windows_game_runtimes() {
        assert!(process_tokens_look_like_windows_game(&[
            "pressure-vessel",
            "/home/agony/.steam/steamapps/common/Repo/R.E.P.O.exe",
        ]));
        assert!(process_tokens_look_like_windows_game(&[
            "steam_app_3241660",
            "-some-flag",
        ]));
        assert!(!process_tokens_look_like_windows_game(&[
            "/usr/bin/java",
            "-jar",
            "notes.jar",
        ]));
    }

    #[test]
    fn alt_tab_icon_names_should_avoid_noisy_theme_and_raw_class_icons() {
        assert_eq!(
            alt_tab_icon_name_for_client("org.quickshell", "ScreenTime"),
            "clock"
        );
        let settings_icon = alt_tab_icon_name_for_client("org.quickshell", "Astrea Settings");
        assert_ne!(settings_icon, "preferences-system");
        assert!(
            settings_icon.ends_with("/.local/share/applications/astrea-icons/astrea-settings.png")
        );
        assert_eq!(
            alt_tab_icon_name_for_client("Tales of Androgyny", "Tales of Androgyny"),
            "application-x-executable"
        );
    }

    #[test]
    fn add_elapsed_should_update_day_week_hour_and_app_buckets() {
        let mut state = empty_state();
        let sample = Sample {
            app: "terminal".to_string(),
            category: "development".to_string(),
            category_label: "Development".to_string(),
            class: "kitty".to_string(),
            title: "agony".to_string(),
            ok: true,
            ..Sample::default()
        };
        let start = match Local.with_ymd_and_hms(2026, 6, 4, 9, 30, 0) {
            LocalResult::Single(value) | LocalResult::Ambiguous(value, _) => {
                value.timestamp() as f64
            }
            LocalResult::None => panic!("invalid local test timestamp"),
        };

        add_elapsed(&mut state, &sample, start, start + 90.0);

        assert_eq!(state["total_seconds"].as_f64().unwrap_or_default(), 90.0);
        assert_eq!(
            state["days"]["2026-06-04"]["seconds"]
                .as_f64()
                .unwrap_or_default(),
            90.0
        );
        assert_eq!(
            state["weeks"]["2026-06-01"]["seconds"]
                .as_f64()
                .unwrap_or_default(),
            90.0
        );
        assert_eq!(
            state["days"]["2026-06-04"]["hourly"]["9"]["seconds"]
                .as_f64()
                .unwrap_or_default(),
            90.0
        );
        assert_eq!(
            state["apps"]["terminal"]["seconds"]
                .as_f64()
                .unwrap_or_default(),
            90.0
        );
    }

    #[test]
    fn file_lock_should_not_truncate_existing_lock_when_busy() {
        let path = env::temp_dir().join(format!(
            "astrea-screentime-lock-{}-{}.lock",
            std::process::id(),
            Utc::now().timestamp_nanos_opt().unwrap_or_default()
        ));
        let first = FileLock::acquire(&path).unwrap_or_else(|error| panic!("{error}"));
        let before = fs::read_to_string(&path).unwrap_or_default();

        assert!(matches!(
            FileLock::acquire(&path),
            Err(BackendError::AlreadyRunning)
        ));
        let after = fs::read_to_string(&path).unwrap_or_default();

        drop(first);
        let _ = fs::remove_file(&path);
        assert_eq!(before, after);
    }

    #[test]
    fn snapshot_should_expose_ui_contract() {
        let mut state = empty_state();
        let sample = Sample {
            app: "terminal".to_string(),
            category: "development".to_string(),
            category_label: "Development".to_string(),
            class: "kitty".to_string(),
            title: "agony".to_string(),
            ok: true,
            ..Sample::default()
        };
        let start = match Local.with_ymd_and_hms(2026, 6, 4, 9, 0, 0) {
            LocalResult::Single(value) | LocalResult::Ambiguous(value, _) => {
                value.timestamp() as f64
            }
            LocalResult::None => panic!("invalid local test timestamp"),
        };
        add_elapsed(&mut state, &sample, start, start + 3600.0);
        state["health"] = json!({
            "running": true,
            "last_error": "",
            "last_sample_at": now_iso_utc(),
            "interval_seconds": 15.0,
            "pid": std::process::id(),
        });
        state["current"] = serde_json::to_value(&sample).unwrap_or_else(|error| panic!("{error}"));
        let paths = Paths {
            state_path: PathBuf::from("/tmp/usage.json"),
            events_path: PathBuf::from("/tmp/events.jsonl"),
            rules_path: PathBuf::from("/tmp/app_rules.json"),
            lock_path: PathBuf::from("/tmp/monitor.lock"),
            settings_path: PathBuf::from("/tmp/settings.json"),
        };

        let snapshot = snapshot_from_state(
            state,
            &paths,
            Some("2026-06-04"),
            Some("2026-06-01"),
            12,
            json!({"unit": "astrea-screentimed.service", "active": true, "enabled": true, "display": {"state": "Ativo"}}),
            empty_settings(),
        );

        assert_eq!(
            snapshot["schema_version"].as_u64().unwrap_or_default(),
            STATE_SCHEMA_VERSION
        );
        assert_eq!(
            snapshot["selected_day"].as_str().unwrap_or_default(),
            "2026-06-04"
        );
        assert_eq!(
            snapshot["selected_week"].as_str().unwrap_or_default(),
            "2026-06-01"
        );
        assert_eq!(snapshot["day"]["hourly"].as_array().map(Vec::len), Some(24));
        assert_eq!(
            snapshot["day"]["top_apps"][0]["id"]
                .as_str()
                .unwrap_or_default(),
            "terminal"
        );
        assert_eq!(
            snapshot["day"]["top_apps"][0]["icon"]
                .as_str()
                .unwrap_or_default(),
            "kitty"
        );
        assert_eq!(snapshot["week"]["days"].as_array().map(Vec::len), Some(7));
        assert_eq!(
            snapshot["week"]["apps"][0]["id"]
                .as_str()
                .unwrap_or_default(),
            "terminal"
        );
        assert!(
            snapshot["display"]["health"]
                .as_str()
                .unwrap_or_default()
                .contains("Coletor")
        );
        assert_eq!(
            snapshot["service"]["unit"].as_str().unwrap_or_default(),
            "astrea-screentimed.service"
        );
    }

    #[test]
    fn usage_rows_should_expose_human_app_labels() {
        let rows = usage_rows(
            &json!({
                "repo.exe": {
                    "seconds": 42.0,
                    "class": "repo.exe",
                    "title": "R.E.P.O.",
                    "last_title": "R.E.P.O."
                },
                "steam_app_1962700": {
                    "seconds": 24.0,
                    "class": "steam_app_1962700",
                    "title": "Subnautica 2 - Build 115506",
                    "last_title": "Subnautica 2 - Build 115506"
                },
                "zen": {
                    "seconds": 12.0,
                    "class": "zen",
                    "title": "Zen Browser",
                    "last_title": "(4) YouTube - Zen Browser"
                },
                "some_tool": {
                    "seconds": 6.0,
                    "class": "some_tool",
                    "title": "",
                    "last_title": ""
                }
            }),
            8,
            None,
            None,
            true,
        );

        assert_eq!(rows[0]["label"].as_str().unwrap_or_default(), "R.E.P.O.");
        assert_eq!(
            rows[1]["label"].as_str().unwrap_or_default(),
            "Subnautica 2"
        );
        assert_eq!(rows[2]["label"].as_str().unwrap_or_default(), "Zen Browser");
        assert_eq!(rows[3]["label"].as_str().unwrap_or_default(), "Some Tool");
    }

    #[test]
    fn snapshot_should_hide_app_rows_without_hiding_category_totals() {
        let mut state = empty_state();
        let sample = Sample {
            app: "terminal".to_string(),
            category: "development".to_string(),
            category_label: "Development".to_string(),
            class: "kitty".to_string(),
            title: "agony".to_string(),
            ok: true,
            ..Sample::default()
        };
        let start = match Local.with_ymd_and_hms(2026, 6, 4, 9, 0, 0) {
            LocalResult::Single(value) | LocalResult::Ambiguous(value, _) => {
                value.timestamp() as f64
            }
            LocalResult::None => panic!("invalid local test timestamp"),
        };
        add_elapsed(&mut state, &sample, start, start + 1800.0);
        let paths = Paths {
            state_path: PathBuf::from("/tmp/usage.json"),
            events_path: PathBuf::from("/tmp/events.jsonl"),
            rules_path: PathBuf::from("/tmp/app_rules.json"),
            lock_path: PathBuf::from("/tmp/monitor.lock"),
            settings_path: PathBuf::from("/tmp/settings.json"),
        };

        let snapshot = snapshot_from_state(
            state,
            &paths,
            Some("2026-06-04"),
            Some("2026-06-01"),
            12,
            json!({"unit": "astrea-screentimed.service", "active": true, "enabled": true, "display": {"state": "Ativo"}}),
            json!({"hidden_apps": ["terminal"]}),
        );

        assert_eq!(
            snapshot["day"]["top_apps"].as_array().map(Vec::len),
            Some(0)
        );
        assert_eq!(
            snapshot["day"]["top_categories"][0]["id"]
                .as_str()
                .unwrap_or_default(),
            "development"
        );
        assert_eq!(
            snapshot["day"]["top_categories"][0]["seconds"]
                .as_f64()
                .unwrap_or_default(),
            1800.0
        );
        assert_eq!(
            snapshot["settings"]["apps"][0]["id"]
                .as_str()
                .unwrap_or_default(),
            "terminal"
        );
        assert!(
            snapshot["settings"]["apps"][0]["hidden"]
                .as_bool()
                .unwrap_or(false)
        );
    }

    #[test]
    fn anonymize_app_usage_should_move_app_history_to_unknown() {
        let mut state = empty_state();
        let terminal = Sample {
            app: "terminal".to_string(),
            category: "development".to_string(),
            category_label: "Development".to_string(),
            class: "kitty".to_string(),
            title: "agony".to_string(),
            ok: true,
            ..Sample::default()
        };
        let browser = Sample {
            app: "zen".to_string(),
            category: "browser".to_string(),
            category_label: "Browser".to_string(),
            class: "zen".to_string(),
            title: "ChatGPT".to_string(),
            ok: true,
            ..Sample::default()
        };
        let start = match Local.with_ymd_and_hms(2026, 6, 4, 9, 0, 0) {
            LocalResult::Single(value) | LocalResult::Ambiguous(value, _) => {
                value.timestamp() as f64
            }
            LocalResult::None => panic!("invalid local test timestamp"),
        };
        add_elapsed(&mut state, &terminal, start, start + 1800.0);
        add_elapsed(&mut state, &browser, start + 1800.0, start + 2400.0);

        let state = anonymize_app_usage(state, "terminal");

        assert!(state["apps"].get("terminal").is_none());
        assert_eq!(
            state["apps"]["unknown"]["seconds"]
                .as_f64()
                .unwrap_or_default(),
            1800.0
        );
        assert_eq!(
            state["apps"]["unknown"]["category"]
                .as_str()
                .unwrap_or_default(),
            "other"
        );
        assert!(state["categories"].get("development").is_none());
        assert_eq!(
            state["categories"]["other"]["seconds"]
                .as_f64()
                .unwrap_or_default(),
            1800.0
        );
        assert_eq!(
            state["days"]["2026-06-04"]["apps"]["unknown"]["seconds"]
                .as_f64()
                .unwrap_or_default(),
            1800.0
        );
        assert_eq!(
            state["days"]["2026-06-04"]["hourly"]["9"]["apps"]["unknown"]["seconds"]
                .as_f64()
                .unwrap_or_default(),
            1800.0
        );
        assert_eq!(
            state["weeks"]["2026-06-01"]["apps"]["unknown"]["seconds"]
                .as_f64()
                .unwrap_or_default(),
            1800.0
        );
    }

    #[test]
    fn compact_events_file_should_keep_latest_events() {
        let path = env::temp_dir().join(format!(
            "astrea-screentime-events-{}-{}.jsonl",
            std::process::id(),
            Utc::now().timestamp_nanos_opt().unwrap_or_default()
        ));
        fs::write(
            &path,
            "{\"at\":\"1\"}\n{\"at\":\"2\"}\nnot-json-but-preserved\n{\"at\":\"4\"}\n",
        )
        .unwrap_or_else(|error| panic!("{error}"));

        let result = compact_events_file(&path, 2).unwrap_or_else(|error| panic!("{error}"));
        let compacted = fs::read_to_string(&path).unwrap_or_else(|error| panic!("{error}"));
        let _ = fs::remove_file(&path);

        assert_eq!(result["before_lines"].as_u64().unwrap_or_default(), 4);
        assert_eq!(result["after_lines"].as_u64().unwrap_or_default(), 2);
        assert_eq!(compacted, "not-json-but-preserved\n{\"at\":\"4\"}\n");
    }

    #[test]
    fn doctor_json_should_expose_paths_and_event_metrics() {
        let root = env::temp_dir().join(format!(
            "astrea-screentime-doctor-{}-{}",
            std::process::id(),
            Utc::now().timestamp_nanos_opt().unwrap_or_default()
        ));
        fs::create_dir_all(&root).unwrap_or_else(|error| panic!("{error}"));
        let paths = Paths {
            state_path: root.join("usage.json"),
            events_path: root.join("events.jsonl"),
            rules_path: root.join("app_rules.json"),
            lock_path: root.join("monitor.lock"),
            settings_path: root.join("settings.json"),
        };
        atomic_write_json(&paths.state_path, &empty_state())
            .unwrap_or_else(|error| panic!("{error}"));
        atomic_write_json(&paths.settings_path, &empty_settings())
            .unwrap_or_else(|error| panic!("{error}"));
        atomic_write_json(
            &paths.rules_path,
            &json!({"categories": {"other": {"label": "Other", "apps": {}}}}),
        )
        .unwrap_or_else(|error| panic!("{error}"));
        fs::write(&paths.events_path, "{\"at\":\"1\"}\n{\"at\":\"2\"}\n")
            .unwrap_or_else(|error| panic!("{error}"));

        let result = doctor_json(&paths);
        let _ = fs::remove_dir_all(&root);

        assert!(result["ok"].as_bool().unwrap_or(false));
        assert_eq!(result["events"]["lines"].as_u64().unwrap_or_default(), 2);
        assert_eq!(
            result["paths"]["state"]["exists"]
                .as_bool()
                .unwrap_or(false),
            true
        );
    }
}
