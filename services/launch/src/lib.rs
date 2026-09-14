use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::env;
use std::fs::{self, OpenOptions};
use std::io::{Read, Write};
use std::net::Shutdown;
use std::os::unix::fs::PermissionsExt;
use std::os::unix::net::{UnixListener, UnixStream};
use std::os::unix::process::CommandExt;
use std::path::{Path, PathBuf};
use std::process::{self, Command, Stdio};
use std::thread;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

const DEFAULT_BOOST_MS: u64 = 3000;
pub const DEFAULT_UMU_ID: &str = "umu-default";

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "kebab-case")]
pub enum LaunchTarget {
    File { path: String },
    Url { url: String },
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct DesktopLaunchContext {
    pub targets: Vec<LaunchTarget>,
    pub files: Vec<String>,
    pub urls: Vec<String>,
    pub name: Option<String>,
    pub icon: Option<String>,
    pub desktop_file: PathBuf,
}

impl DesktopLaunchContext {
    fn from_targets(targets: &[LaunchTarget], desktop_file: &Path) -> Self {
        let mut context = Self {
            targets: targets.to_vec(),
            desktop_file: desktop_file.to_path_buf(),
            ..Self::default()
        };
        for target in targets {
            match target {
                LaunchTarget::File { path } => context.files.push(expand_home(path)),
                LaunchTarget::Url { url } => context.urls.push(url.clone()),
            }
        }
        context
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "kebab-case")]
pub enum LaunchRequest {
    Desktop {
        id: String,
        #[serde(default)]
        targets: Vec<LaunchTarget>,
    },
    Command {
        command: String,
    },
    Argv {
        argv: Vec<String>,
        working_dir: Option<String>,
    },
    Windows {
        path: String,
    },
    File {
        path: String,
    },
    Url {
        url: String,
    },
    Steam {
        uri: String,
    },
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct CommandSpec {
    pub argv: Vec<String>,
    pub working_dir: Option<PathBuf>,
    pub desktop_file: Option<PathBuf>,
    pub environment: BTreeMap<String, String>,
    pub windows_metadata: Option<WindowsLaunchMetadata>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct WindowsLaunchMetadata {
    pub runner: String,
    pub machine: String,
    pub warnings: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct WindowsLaunchPlan {
    pub command: CommandSpec,
    pub metadata: WindowsLaunchMetadata,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WindowsRunnerPolicy {
    Proton,
    Wine,
    Auto,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct WindowsCompatibilityConfig {
    pub runner: WindowsRunnerPolicy,
    pub use_proton_profile: bool,
    pub gamemode: bool,
    pub mangohud: bool,
    pub gamescope: bool,
    pub extra_env: String,
    pub extra_prefix: String,
}

impl Default for WindowsCompatibilityConfig {
    fn default() -> Self {
        Self {
            runner: WindowsRunnerPolicy::Proton,
            use_proton_profile: true,
            gamemode: true,
            mangohud: false,
            gamescope: false,
            extra_env: String::new(),
            extra_prefix: String::new(),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct WindowsProtonConfig {
    pub gamemode: bool,
    pub mangohud: bool,
    pub gamescope: bool,
    pub use_gamescope_profile: bool,
    pub gamescope_width: u32,
    pub gamescope_height: u32,
    pub gamescope_refresh: u32,
    pub gamescope_fullscreen: bool,
    pub gamescope_immediate_flips: bool,
    pub gamescope_hide_cursor: bool,
    pub gamescope_force_grab_cursor: bool,
    pub gamescope_adaptive_sync: bool,
    pub gamescope_extra_args: String,
    pub enable_nvapi: bool,
    pub hide_nvidia_gpu: bool,
    pub sync_mode: String,
    pub use_wined3d: bool,
    pub dxvk_async: bool,
    pub dxvk_hdr: bool,
    pub vkd3d_dxr: bool,
    pub fsr: bool,
    pub fsr_strength: u32,
    pub custom_env: String,
    pub custom_prefix: String,
}

impl Default for WindowsProtonConfig {
    fn default() -> Self {
        Self {
            gamemode: true,
            mangohud: false,
            gamescope: false,
            use_gamescope_profile: true,
            gamescope_width: 1920,
            gamescope_height: 1080,
            gamescope_refresh: 60,
            gamescope_fullscreen: true,
            gamescope_immediate_flips: false,
            gamescope_hide_cursor: false,
            gamescope_force_grab_cursor: false,
            gamescope_adaptive_sync: false,
            gamescope_extra_args: String::new(),
            enable_nvapi: false,
            hide_nvidia_gpu: false,
            sync_mode: "default".into(),
            use_wined3d: false,
            dxvk_async: false,
            dxvk_hdr: false,
            vkd3d_dxr: false,
            fsr: false,
            fsr_strength: 2,
            custom_env: String::new(),
            custom_prefix: String::new(),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct WindowsGamescopeConfig {
    pub width: u32,
    pub height: u32,
    pub refresh: u32,
    pub fullscreen: bool,
    pub immediate_flips: bool,
    pub hide_cursor: bool,
    pub extra_args: String,
}

impl Default for WindowsGamescopeConfig {
    fn default() -> Self {
        Self {
            width: 1920,
            height: 1080,
            refresh: 60,
            fullscreen: true,
            immediate_flips: false,
            hide_cursor: false,
            extra_args: String::new(),
        }
    }
}

#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(default)]
pub struct LaunchConfig {
    pub default_external_boost_ms: u64,
    pub allow_nvidia_env: bool,
    pub isolate_launches: bool,
    pub history_limit: usize,
    pub latency: LatencyConfig,
    pub rules: Vec<Rule>,
}

impl Default for LaunchConfig {
    fn default() -> Self {
        Self {
            default_external_boost_ms: DEFAULT_BOOST_MS,
            allow_nvidia_env: false,
            isolate_launches: true,
            history_limit: 200,
            latency: LatencyConfig::default(),
            rules: Vec::new(),
        }
    }
}

#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(default)]
pub struct LatencyConfig {
    pub enabled: bool,
    pub socket_paths: Vec<String>,
    pub command: Option<Vec<String>>,
}

impl Default for LatencyConfig {
    fn default() -> Self {
        Self {
            enabled: true,
            socket_paths: default_latency_socket_paths(),
            command: None,
        }
    }
}

#[derive(Debug, Clone, Default, Deserialize, Serialize, PartialEq, Eq)]
#[serde(default)]
pub struct Rule {
    pub desktop_id: Option<String>,
    pub executable: Option<String>,
    pub command_substring: Option<String>,
    pub steam_appid: Option<String>,
    pub env: Vec<(String, String)>,
    pub working_dir: Option<String>,
    pub launch_boost_ms: Option<u64>,
    pub allow_external_pid_boost: Option<bool>,
    pub game_mode_preset: Option<String>,
    pub steam_compat_preset: Option<String>,
    pub nvidia_env: Option<bool>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LaunchRecord {
    pub timestamp_ms: u128,
    pub kind: String,
    pub target: String,
    pub argv: Vec<String>,
    pub pid: Option<u32>,
    pub status: String,
    pub detail: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub windows: Option<WindowsLaunchMetadata>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct LaunchDaemonResponse {
    ok: bool,
    record: Option<LaunchRecord>,
    error: Option<String>,
}

#[derive(Debug)]
enum LaunchdError {
    Connect,
    Request(String),
}

pub fn compatibility_config_path() -> PathBuf {
    xdg_config_home().join("AstreaOS/gaming/compatibility.json")
}

pub fn proton_config_path() -> PathBuf {
    xdg_config_home().join("AstreaOS/gaming/proton.json")
}

pub fn gamescope_config_path() -> PathBuf {
    xdg_config_home().join("AstreaOS/gaming/gamescope.json")
}

pub fn shared_windows_prefix_root() -> PathBuf {
    xdg_data_home().join("AstreaOS/windows-prefixes/shared/proton")
}

pub fn shared_windows_wine_prefix() -> PathBuf {
    shared_windows_prefix_root().join("pfx")
}

pub fn windows_log_dir() -> PathBuf {
    xdg_state_home().join("AstreaOS/windows-prefixes/logs")
}

pub fn normalize_compatibility_config(raw: &serde_json::Value) -> WindowsCompatibilityConfig {
    let defaults = WindowsCompatibilityConfig::default();
    let object = raw.as_object();
    let runner = match object
        .and_then(|values| values.get("runner"))
        .and_then(serde_json::Value::as_str)
    {
        Some("wine") => WindowsRunnerPolicy::Wine,
        Some("auto") => WindowsRunnerPolicy::Auto,
        _ => defaults.runner,
    };
    WindowsCompatibilityConfig {
        runner,
        use_proton_profile: json_bool(object, "use_proton_profile", defaults.use_proton_profile),
        gamemode: json_bool(object, "gamemode", defaults.gamemode),
        mangohud: json_bool(object, "mangohud", defaults.mangohud),
        gamescope: json_bool(object, "gamescope", defaults.gamescope),
        extra_env: json_string(object, "extra_env"),
        extra_prefix: json_string(object, "extra_prefix"),
    }
}

pub fn normalize_proton_config(raw: &serde_json::Value) -> WindowsProtonConfig {
    let defaults = WindowsProtonConfig::default();
    let object = raw.as_object();
    let sync_mode = match json_string_value(object, "sync_mode").as_str() {
        "disable-esync" | "disable-fsync" | "disable-both" => {
            json_string_value(object, "sync_mode")
        }
        _ => defaults.sync_mode.clone(),
    };
    WindowsProtonConfig {
        gamemode: json_bool(object, "gamemode", defaults.gamemode),
        mangohud: json_bool(object, "mangohud", defaults.mangohud),
        gamescope: json_bool(object, "gamescope", defaults.gamescope),
        use_gamescope_profile: json_bool(
            object,
            "use_gamescope_profile",
            defaults.use_gamescope_profile,
        ),
        gamescope_width: json_u32(
            object,
            "gamescope_width",
            defaults.gamescope_width,
            640,
            10000,
        ),
        gamescope_height: json_u32(
            object,
            "gamescope_height",
            defaults.gamescope_height,
            360,
            10000,
        ),
        gamescope_refresh: json_u32(
            object,
            "gamescope_refresh",
            defaults.gamescope_refresh,
            30,
            1000,
        ),
        gamescope_fullscreen: json_bool(
            object,
            "gamescope_fullscreen",
            defaults.gamescope_fullscreen,
        ),
        gamescope_immediate_flips: json_bool(
            object,
            "gamescope_immediate_flips",
            defaults.gamescope_immediate_flips,
        ),
        gamescope_hide_cursor: json_bool(
            object,
            "gamescope_hide_cursor",
            defaults.gamescope_hide_cursor,
        ),
        gamescope_force_grab_cursor: json_bool(
            object,
            "gamescope_force_grab_cursor",
            defaults.gamescope_force_grab_cursor,
        ),
        gamescope_adaptive_sync: json_bool(
            object,
            "gamescope_adaptive_sync",
            defaults.gamescope_adaptive_sync,
        ),
        gamescope_extra_args: json_string(object, "gamescope_extra_args"),
        enable_nvapi: json_bool(object, "enable_nvapi", defaults.enable_nvapi),
        hide_nvidia_gpu: json_bool(object, "hide_nvidia_gpu", defaults.hide_nvidia_gpu),
        sync_mode,
        use_wined3d: json_bool(object, "use_wined3d", defaults.use_wined3d),
        dxvk_async: json_bool(object, "dxvk_async", defaults.dxvk_async),
        dxvk_hdr: json_bool(object, "dxvk_hdr", defaults.dxvk_hdr),
        vkd3d_dxr: json_bool(object, "vkd3d_dxr", defaults.vkd3d_dxr),
        fsr: json_bool(object, "fsr", defaults.fsr),
        fsr_strength: json_u32(object, "fsr_strength", defaults.fsr_strength, 0, 5),
        custom_env: json_string(object, "custom_env"),
        custom_prefix: json_string(object, "custom_prefix"),
    }
}

pub fn normalize_gamescope_config(raw: &serde_json::Value) -> WindowsGamescopeConfig {
    let defaults = WindowsGamescopeConfig::default();
    let object = raw.as_object();
    WindowsGamescopeConfig {
        width: json_u32(object, "width", defaults.width, 640, 10000),
        height: json_u32(object, "height", defaults.height, 360, 10000),
        refresh: json_u32(object, "refresh", defaults.refresh, 30, 1000),
        fullscreen: json_bool(object, "fullscreen", defaults.fullscreen),
        immediate_flips: json_bool(object, "immediate_flips", defaults.immediate_flips),
        hide_cursor: json_bool(object, "hide_cursor", defaults.hide_cursor),
        extra_args: json_string(object, "extra_args"),
    }
}

fn json_bool(
    object: Option<&serde_json::Map<String, serde_json::Value>>,
    key: &str,
    fallback: bool,
) -> bool {
    object
        .and_then(|values| values.get(key))
        .and_then(serde_json::Value::as_bool)
        .unwrap_or(fallback)
}

fn json_string(object: Option<&serde_json::Map<String, serde_json::Value>>, key: &str) -> String {
    json_string_value(object, key).trim().to_string()
}

fn json_string_value(
    object: Option<&serde_json::Map<String, serde_json::Value>>,
    key: &str,
) -> String {
    object
        .and_then(|values| values.get(key))
        .and_then(serde_json::Value::as_str)
        .unwrap_or_default()
        .to_string()
}

fn json_u32(
    object: Option<&serde_json::Map<String, serde_json::Value>>,
    key: &str,
    fallback: u32,
    minimum: u32,
    maximum: u32,
) -> u32 {
    let value = object
        .and_then(|values| values.get(key))
        .and_then(|value| value.as_u64().or_else(|| value.as_str()?.parse().ok()))
        .map(|value: u64| value as u32)
        .unwrap_or(fallback);
    value.clamp(minimum, maximum)
}

fn read_json_object(path: &Path) -> serde_json::Value {
    fs::read_to_string(path)
        .ok()
        .and_then(|text| serde_json::from_str(&text).ok())
        .unwrap_or_else(|| serde_json::Value::Object(serde_json::Map::new()))
}

pub fn config_path() -> PathBuf {
    xdg_config_home().join("AstreaOS/system/launch.json")
}

pub fn history_path() -> PathBuf {
    xdg_state_home().join("Astrea/launch/history.jsonl")
}

pub fn launchd_socket_path() -> PathBuf {
    xdg_runtime_dir()
        .unwrap_or_else(|| xdg_state_home().join("Astrea/runtime"))
        .join("Astrea/astrea-launchd.sock")
}

pub fn load_config() -> (LaunchConfig, Option<String>) {
    let path = config_path();
    match fs::read_to_string(&path) {
        Ok(text) => match serde_json::from_str::<LaunchConfig>(&text) {
            Ok(config) => (config, None),
            Err(err) => (
                LaunchConfig::default(),
                Some(format!("config parse failed: {err}")),
            ),
        },
        Err(err) if err.kind() == std::io::ErrorKind::NotFound => (LaunchConfig::default(), None),
        Err(err) => (
            LaunchConfig::default(),
            Some(format!("config read failed: {err}")),
        ),
    }
}

pub fn default_config_text() -> String {
    serde_json::to_string_pretty(&LaunchConfig::default()).unwrap_or_else(|_| "{}".into()) + "\n"
}

pub fn ensure_default_config() -> Result<(), String> {
    let path = config_path();
    if path.exists() {
        return Ok(());
    }
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|err| format!("create config dir: {err}"))?;
    }
    fs::write(path, default_config_text()).map_err(|err| format!("write config: {err}"))
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct WindowsTargetMetadata {
    pub path: PathBuf,
    pub machine: String,
    pub is_msi: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SelectedWindowsRunner {
    Umu,
    Wine,
}

pub fn parse_argv_tokens(text: &str) -> Result<Vec<String>, String> {
    let mut tokens = Vec::new();
    let mut current = String::new();
    let mut chars = text.chars().peekable();
    let mut quote = None;
    let mut started = false;

    while let Some(ch) = chars.next() {
        match ch {
            '\'' | '"' if quote.is_none() => {
                quote = Some(ch);
                started = true;
            }
            '\'' | '"' if quote == Some(ch) => quote = None,
            '\\' if quote != Some('\'') => {
                let Some(next) = chars.next() else {
                    return Err("unterminated escape in argument prefix".into());
                };
                current.push(next);
                started = true;
            }
            ch if ch.is_whitespace() && quote.is_none() => {
                if started {
                    tokens.push(std::mem::take(&mut current));
                    started = false;
                }
            }
            _ => {
                current.push(ch);
                started = true;
            }
        }
    }

    if quote.is_some() {
        return Err("unterminated quote in argument prefix".into());
    }
    if started {
        tokens.push(current);
    }
    Ok(tokens)
}

pub fn parse_pe_machine(bytes: &[u8]) -> Result<String, String> {
    if bytes.len() < 64 || &bytes[..2] != b"MZ" {
        return Err("Windows executable is not a valid DOS/PE image".into());
    }
    let pe_offset = u32::from_le_bytes(
        bytes[0x3c..0x40]
            .try_into()
            .map_err(|_| "Windows executable has an invalid PE offset")?,
    ) as usize;
    let header_end = pe_offset
        .checked_add(6)
        .ok_or_else(|| "Windows executable has an invalid PE offset".to_string())?;
    if header_end > bytes.len() || &bytes[pe_offset..pe_offset + 4] != b"PE\0\0" {
        return Err("Windows executable is missing a valid PE signature".into());
    }
    let machine = u16::from_le_bytes(
        bytes[pe_offset + 4..pe_offset + 6]
            .try_into()
            .map_err(|_| "Windows executable has an invalid PE machine")?,
    );
    Ok(match machine {
        0x014c => "i386".into(),
        0x8664 => "x86_64".into(),
        0xaa64 => "arm64".into(),
        value => format!("unknown-0x{value:04x}"),
    })
}

pub fn validate_windows_target(path: &Path) -> Result<WindowsTargetMetadata, String> {
    let expanded = PathBuf::from(expand_home(&path.to_string_lossy()));
    let target = expanded
        .canonicalize()
        .map_err(|err| format!("file not found: {} ({err})", expanded.display()))?;
    if !target.is_file() {
        return Err(format!("file not found: {}", target.display()));
    }
    let suffix = target
        .extension()
        .and_then(|value| value.to_str())
        .unwrap_or_default()
        .to_ascii_lowercase();
    match suffix.as_str() {
        "msi" => Ok(WindowsTargetMetadata {
            path: target,
            machine: "unknown".into(),
            is_msi: true,
        }),
        "exe" => {
            let bytes =
                fs::read(&target).map_err(|err| format!("read Windows executable: {err}"))?;
            let machine = parse_pe_machine(&bytes)?;
            Ok(WindowsTargetMetadata {
                path: target,
                machine,
                is_msi: false,
            })
        }
        _ => Err(format!("unsupported Windows file: {}", target.display())),
    }
}

pub fn select_windows_runner(
    policy: WindowsRunnerPolicy,
    umu_available: bool,
    wine_available: bool,
) -> Result<SelectedWindowsRunner, String> {
    match policy {
        WindowsRunnerPolicy::Proton if umu_available => Ok(SelectedWindowsRunner::Umu),
        WindowsRunnerPolicy::Proton => Err("UMU is required for the Proton Windows runner".into()),
        WindowsRunnerPolicy::Wine if wine_available => Ok(SelectedWindowsRunner::Wine),
        WindowsRunnerPolicy::Wine => Err("Wine not found; install wine or choose Auto".into()),
        WindowsRunnerPolicy::Auto if umu_available => Ok(SelectedWindowsRunner::Umu),
        WindowsRunnerPolicy::Auto if wine_available => Ok(SelectedWindowsRunner::Wine),
        WindowsRunnerPolicy::Auto => Err("neither UMU nor Wine is available".into()),
    }
}

pub fn plan_windows_launch(path: &Path) -> Result<WindowsLaunchPlan, String> {
    let target = validate_windows_target(path)?;
    let compatibility =
        normalize_compatibility_config(&read_json_object(&compatibility_config_path()));
    let profile = if compatibility.use_proton_profile {
        normalize_proton_config(&read_json_object(&proton_config_path()))
    } else {
        WindowsProtonConfig {
            gamemode: compatibility.gamemode,
            mangohud: compatibility.mangohud,
            gamescope: compatibility.gamescope,
            custom_env: compatibility.extra_env.clone(),
            custom_prefix: compatibility.extra_prefix.clone(),
            ..WindowsProtonConfig::default()
        }
    };
    let gamescope = if profile.use_gamescope_profile {
        normalize_gamescope_config(&read_json_object(&gamescope_config_path()))
    } else {
        let mut extra_args = Vec::new();
        if profile.gamescope_force_grab_cursor {
            extra_args.push("--force-grab-cursor".into());
        }
        if profile.gamescope_adaptive_sync {
            extra_args.push("--adaptive-sync".into());
        }
        extra_args.extend(parse_argv_tokens(&profile.gamescope_extra_args)?);
        WindowsGamescopeConfig {
            width: profile.gamescope_width,
            height: profile.gamescope_height,
            refresh: profile.gamescope_refresh,
            fullscreen: profile.gamescope_fullscreen,
            immediate_flips: profile.gamescope_immediate_flips,
            hide_cursor: profile.gamescope_hide_cursor,
            extra_args: extra_args
                .iter()
                .map(|arg| {
                    if arg.contains(char::is_whitespace) {
                        format!("'{arg}'")
                    } else {
                        arg.clone()
                    }
                })
                .collect::<Vec<_>>()
                .join(" "),
        }
    };
    let runtime = discover_windows_runtime();
    let selected = select_windows_runner(
        compatibility.runner,
        runtime.umu_run.is_some(),
        runtime.wine.is_some(),
    )?;
    let mut environment = windows_base_environment(&runtime);
    let mut warnings = Vec::new();
    apply_windows_profile_environment(&mut environment, &profile)?;

    let runner_command = match selected {
        SelectedWindowsRunner::Umu => {
            let umu = runtime.umu_run.as_ref().expect("UMU selection has a path");
            environment.insert(
                "WINEPREFIX".into(),
                shared_windows_wine_prefix().to_string_lossy().to_string(),
            );
            environment.insert(
                "PROTONPATH".into(),
                runtime
                    .proton
                    .as_ref()
                    .map(|path| path.parent().unwrap_or(path).to_string_lossy().to_string())
                    .unwrap_or_else(|| "GE-Proton".into()),
            );
            let identity = env::var("UMU_ID")
                .ok()
                .filter(|value| !value.is_empty())
                .or_else(|| env::var("GAMEID").ok().filter(|value| !value.is_empty()))
                .unwrap_or_else(|| DEFAULT_UMU_ID.into());
            environment.insert("UMU_ID".into(), identity.clone());
            environment.insert("GAMEID".into(), identity);
            environment.insert("STORE".into(), "none".into());
            let mut command = vec![umu.to_string_lossy().to_string()];
            append_windows_target_args(&mut command, &target);
            command
        }
        SelectedWindowsRunner::Wine => {
            environment.insert(
                "WINEPREFIX".into(),
                shared_windows_wine_prefix().to_string_lossy().to_string(),
            );
            let wine = runtime.wine.as_ref().expect("Wine selection has a path");
            let mut command = vec![wine.to_string_lossy().to_string()];
            append_windows_target_args(&mut command, &target);
            command
        }
    };

    let mut argv = compose_windows_wrappers(
        runner_command,
        &profile,
        &gamescope,
        &target.machine,
        &runtime,
        &mut warnings,
    )?;
    let custom_prefix = parse_argv_tokens(&profile.custom_prefix)?;
    if !custom_prefix.is_empty() {
        let mut prefixed = custom_prefix;
        prefixed.append(&mut argv);
        argv = prefixed;
    }

    let metadata = WindowsLaunchMetadata {
        runner: match selected {
            SelectedWindowsRunner::Umu => "umu-proton".into(),
            SelectedWindowsRunner::Wine => "wine".into(),
        },
        machine: target.machine,
        warnings,
    };
    let command = CommandSpec {
        argv,
        working_dir: target.path.parent().map(Path::to_path_buf),
        desktop_file: None,
        environment,
        windows_metadata: Some(metadata.clone()),
    };
    Ok(WindowsLaunchPlan { command, metadata })
}

fn append_windows_target_args(command: &mut Vec<String>, target: &WindowsTargetMetadata) {
    if target.is_msi {
        command.extend([
            "msiexec".into(),
            "/i".into(),
            target.path.to_string_lossy().to_string(),
        ]);
    } else {
        command.push(target.path.to_string_lossy().to_string());
    }
}

fn windows_base_environment(runtime: &WindowsRuntime) -> BTreeMap<String, String> {
    let mut environment = BTreeMap::new();
    environment.insert(
        "PROTON_LOG_DIR".into(),
        windows_log_dir().to_string_lossy().to_string(),
    );
    environment.insert(
        "STEAM_COMPAT_CLIENT_INSTALL_PATH".into(),
        steam_root().to_string_lossy().to_string(),
    );
    environment.insert("STEAM_COMPAT_APP_ID".into(), "0".into());
    if let Some(interface) = &runtime.runtime_interface {
        let mut path = interface.to_string_lossy().to_string();
        if let Some(existing) = env::var_os("PATH") {
            let existing = env::split_paths(&existing)
                .map(|value| value.to_string_lossy().to_string())
                .collect::<Vec<_>>();
            if !existing.is_empty() {
                path.push(':');
                path.push_str(&existing.join(":"));
            }
        }
        environment.insert("PATH".into(), path);
    }
    environment
}

fn apply_windows_profile_environment(
    environment: &mut BTreeMap<String, String>,
    profile: &WindowsProtonConfig,
) -> Result<(), String> {
    if profile.enable_nvapi {
        environment.insert("PROTON_FORCE_NVAPI".into(), "1".into());
        environment.insert("PROTON_HIDE_NVIDIA_GPU".into(), "0".into());
    } else if profile.hide_nvidia_gpu {
        environment.insert("PROTON_HIDE_NVIDIA_GPU".into(), "1".into());
    }
    if matches!(profile.sync_mode.as_str(), "disable-esync" | "disable-both") {
        environment.insert("PROTON_NO_ESYNC".into(), "1".into());
    }
    if matches!(profile.sync_mode.as_str(), "disable-fsync" | "disable-both") {
        environment.insert("PROTON_NO_FSYNC".into(), "1".into());
    }
    if profile.use_wined3d {
        environment.insert("PROTON_USE_WINED3D".into(), "1".into());
    }
    if profile.dxvk_async {
        environment.insert("DXVK_ASYNC".into(), "1".into());
    }
    if profile.dxvk_hdr {
        environment.insert("DXVK_HDR".into(), "1".into());
    }
    if profile.vkd3d_dxr {
        environment.insert("VKD3D_CONFIG".into(), "dxr".into());
    }
    if profile.fsr {
        environment.insert("WINE_FULLSCREEN_FSR".into(), "1".into());
        environment.insert(
            "WINE_FULLSCREEN_FSR_STRENGTH".into(),
            profile.fsr_strength.to_string(),
        );
    }
    for token in parse_argv_tokens(&profile.custom_env)? {
        let Some((key, value)) = token.split_once('=') else {
            continue;
        };
        if !key.is_empty() && !key.contains('=') {
            environment.insert(key.to_string(), value.to_string());
        }
    }
    Ok(())
}

struct WindowsRuntime {
    umu_run: Option<PathBuf>,
    wine: Option<PathBuf>,
    proton: Option<PathBuf>,
    gamemode: Option<PathBuf>,
    mangohud: Option<PathBuf>,
    gamescope: Option<PathBuf>,
    runtime_interface: Option<PathBuf>,
}

fn discover_windows_runtime() -> WindowsRuntime {
    WindowsRuntime {
        umu_run: find_umu_run(),
        wine: find_command_path("wine"),
        proton: find_proton(),
        gamemode: find_command_path("gamemoderun"),
        mangohud: find_command_path("mangohud"),
        gamescope: find_command_path("gamescope"),
        runtime_interface: find_runtime_interface(),
    }
}

fn compose_windows_wrappers(
    mut command: Vec<String>,
    profile: &WindowsProtonConfig,
    gamescope: &WindowsGamescopeConfig,
    machine: &str,
    runtime: &WindowsRuntime,
    warnings: &mut Vec<String>,
) -> Result<Vec<String>, String> {
    let mut mangohud_consumed = false;
    if profile.gamescope {
        if let Some(gamescope_program) = &runtime.gamescope {
            let mut wrapper = vec![gamescope_program.to_string_lossy().to_string()];
            if gamescope.fullscreen {
                wrapper.push("-f".into());
            }
            wrapper.extend([
                "-W".into(),
                gamescope.width.to_string(),
                "-H".into(),
                gamescope.height.to_string(),
                "-r".into(),
                gamescope.refresh.to_string(),
            ]);
            if gamescope.immediate_flips {
                wrapper.push("--immediate-flips".into());
            }
            if gamescope.hide_cursor {
                wrapper.extend(["--hide-cursor-delay".into(), "-1".into()]);
            }
            if profile.mangohud && runtime.mangohud.is_some() {
                wrapper.push("--mangoapp".into());
                mangohud_consumed = true;
            }
            wrapper.extend(parse_argv_tokens(&gamescope.extra_args)?);
            wrapper.push("--".into());
            wrapper.append(&mut command);
            command = wrapper;
        } else {
            warnings.push("Gamescope requested but unavailable; continuing without it.".into());
        }
    }
    if profile.mangohud && !mangohud_consumed {
        if let Some(mangohud) = &runtime.mangohud {
            command.insert(0, mangohud.to_string_lossy().to_string());
        } else {
            warnings.push("MangoHud requested but unavailable; continuing without it.".into());
        }
    }
    if profile.gamemode {
        if machine == "i386" && !has_32bit_gamemode_auto() {
            warnings.push(
                "GameMode skipped for 32-bit Windows app because lib32-gamemode is missing.".into(),
            );
        } else if let Some(gamemode) = &runtime.gamemode {
            command.insert(0, gamemode.to_string_lossy().to_string());
        } else {
            warnings.push("GameMode requested but unavailable; continuing without it.".into());
        }
    }
    Ok(command)
}

pub fn run_launch(request: LaunchRequest) -> Result<LaunchRecord, String> {
    let (config, config_warning) = load_config();
    let mut detail_parts = Vec::new();
    if let Some(warning) = config_warning {
        detail_parts.push(warning);
    }

    let mut command = resolve_request(&request)?;
    let raw_command = command.argv.join(" ");
    let executable = command.argv.first().map(String::as_str);
    let rule = matching_rule(&config, &request, executable, &raw_command).cloned();
    if let Some(rule) = &rule {
        apply_rule(&config, &mut command, rule, &mut detail_parts);
    } else if matches!(request, LaunchRequest::Desktop { .. })
        && !(config.allow_nvidia_env && nvidia_available())
    {
        let targets = match &request {
            LaunchRequest::Desktop { targets, .. } => targets,
            _ => &[][..],
        };
        prefer_desktop_launcher(&mut command, targets);
    } else if config.allow_nvidia_env && nvidia_available() {
        prepend_env(&mut command, nvidia_env_vars());
    }

    let kind = request_kind(&request).to_string();
    let target = request_target(&request).to_string();
    let argv_for_log = command.argv.clone();
    let boost_ms = rule
        .as_ref()
        .and_then(|rule| rule.launch_boost_ms)
        .unwrap_or(config.default_external_boost_ms);
    let allow_pid_boost = rule
        .as_ref()
        .and_then(|rule| rule.allow_external_pid_boost)
        .unwrap_or(true);

    request_boost(&config, "app-launch", None, boost_ms);

    let spawn = spawn_command(&command, config.isolate_launches);
    let (pid, status, detail) = match spawn {
        Ok(pid_raw) => {
            let launch_pid = normalize_launch_pid(pid_raw);
            if allow_pid_boost {
                request_boost(&config, "app-launch-pid", launch_pid, boost_ms);
            }
            (launch_pid, "ok".to_string(), "spawned".to_string())
        }
        Err(err) => (None, "error".to_string(), err),
    };

    detail_parts.push(detail);
    let record = LaunchRecord {
        timestamp_ms: now_ms(),
        kind,
        target,
        argv: argv_for_log,
        pid,
        status,
        detail: detail_parts.join("; "),
        windows: command.windows_metadata.clone(),
    };
    let _ = append_history(&record, config.history_limit);
    if record.status == "ok" {
        Ok(record)
    } else {
        Err(record.detail)
    }
}

pub fn run_launch_via_daemon(request: LaunchRequest) -> Result<LaunchRecord, String> {
    match send_launch_request(&request) {
        Ok(record) => Ok(record),
        Err(LaunchdError::Connect) => run_launch(request),
        Err(LaunchdError::Request(err)) => Err(err),
    }
}

pub fn parse_cli_request(args: &[String]) -> Result<LaunchRequest, String> {
    let Some(command) = args.first().map(String::as_str) else {
        return Err("missing command".into());
    };
    match command {
        "--desktop" => {
            let id = args
                .get(1)
                .filter(|value| !value.is_empty())
                .cloned()
                .ok_or_else(|| "--desktop requires an id".to_string())?;
            let mut targets = Vec::new();
            let mut index = 2;
            while index < args.len() {
                let flag = &args[index];
                let value = args
                    .get(index + 1)
                    .filter(|value| !value.is_empty())
                    .cloned()
                    .ok_or_else(|| format!("{flag} requires a value"))?;
                match flag.as_str() {
                    "--file" => targets.push(LaunchTarget::File { path: value }),
                    "--url" => {
                        validate_url(&value)?;
                        targets.push(LaunchTarget::Url { url: value });
                    }
                    _ => return Err(format!("unknown desktop target option: {flag}")),
                }
                index += 2;
            }
            Ok(LaunchRequest::Desktop { id, targets })
        }
        "--command" => {
            exact_single_arg(args, "--command").map(|command| LaunchRequest::Command { command })
        }
        "--argv-json" => {
            let json = exact_single_arg(args, "--argv-json")?;
            let argv = serde_json::from_str::<Vec<String>>(&json)
                .map_err(|err| format!("--argv-json expects a JSON string array: {err}"))?;
            Ok(LaunchRequest::Argv {
                argv,
                working_dir: None,
            })
        }
        "--windows" => {
            exact_single_arg(args, "--windows").map(|path| LaunchRequest::Windows { path })
        }
        "--file" => exact_single_arg(args, "--file").map(|path| LaunchRequest::File { path }),
        "--url" => {
            let url = exact_single_arg(args, "--url")?;
            validate_url(&url)?;
            Ok(LaunchRequest::Url { url })
        }
        "--steam" => exact_single_arg(args, "--steam").map(|uri| LaunchRequest::Steam { uri }),
        _ => Err("invalid command".into()),
    }
}

fn exact_single_arg(args: &[String], flag: &str) -> Result<String, String> {
    match args {
        [command, value] if command == flag && !value.is_empty() => Ok(value.clone()),
        [command] if command == flag => Err(format!("{flag} requires a value")),
        _ => Err(format!("{flag} accepts exactly one value")),
    }
}

pub fn serve_launchd() -> Result<(), String> {
    let path = launchd_socket_path();
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|err| format!("create launchd socket dir: {err}"))?;
    }
    prepare_socket_path(&path)?;
    let listener =
        UnixListener::bind(&path).map_err(|err| format!("bind launchd socket: {err}"))?;
    let _ = fs::set_permissions(&path, fs::Permissions::from_mode(0o600));

    for stream in listener.incoming() {
        match stream {
            Ok(mut stream) => {
                thread::spawn(move || {
                    let response = handle_launchd_stream(&mut stream);
                    let text = serde_json::to_string(&response).unwrap_or_else(|_| {
                        "{\"ok\":false,\"error\":\"serialization failed\"}".into()
                    });
                    let _ = writeln!(stream, "{text}");
                });
            }
            Err(err) => eprintln!("[astrea-launchd] accept failed: {err}"),
        }
    }
    Ok(())
}

fn prepare_socket_path(path: &Path) -> Result<(), String> {
    if !path.exists() {
        return Ok(());
    }
    if UnixStream::connect(path).is_ok() {
        return Err(format!("launchd socket already active: {}", path.display()));
    }
    fs::remove_file(path).map_err(|err| format!("remove stale launchd socket: {err}"))
}

pub fn resolve_request(request: &LaunchRequest) -> Result<CommandSpec, String> {
    match request {
        LaunchRequest::Desktop { id, targets } => {
            let path = resolve_desktop_entry(id)
                .ok_or_else(|| format!("desktop entry not found: {id}"))?;
            let context = DesktopLaunchContext::from_targets(targets, &path);
            command_from_desktop_file_with_context(&path, &context)
        }
        LaunchRequest::Command { command } => Ok(CommandSpec {
            argv: vec!["sh".into(), "-lc".into(), command.clone()],
            working_dir: None,
            desktop_file: None,
            environment: BTreeMap::new(),
            windows_metadata: None,
        }),
        LaunchRequest::Argv { argv, working_dir } => {
            if argv.is_empty() || argv.first().is_some_and(|arg| arg.is_empty()) {
                return Err("argv launch request requires a program".into());
            }
            Ok(CommandSpec {
                argv: argv.clone(),
                working_dir: working_dir
                    .as_deref()
                    .filter(|value| !value.is_empty())
                    .map(expand_home)
                    .map(PathBuf::from),
                desktop_file: None,
                environment: BTreeMap::new(),
                windows_metadata: None,
            })
        }
        LaunchRequest::Windows { path } => Ok(plan_windows_launch(Path::new(path))?.command),
        LaunchRequest::File { path } => Ok(CommandSpec {
            argv: command_for_file_path(&expand_home(path))?,
            working_dir: working_dir_for_file(path),
            desktop_file: None,
            environment: BTreeMap::new(),
            windows_metadata: None,
        }),
        LaunchRequest::Url { url } => {
            validate_url(url)?;
            Ok(CommandSpec {
                argv: vec!["xdg-open".into(), url.clone()],
                working_dir: None,
                desktop_file: None,
                environment: BTreeMap::new(),
                windows_metadata: None,
            })
        }
        LaunchRequest::Steam { uri } => {
            validate_steam_uri(uri)?;
            let launcher = if command_available("steam") {
                "steam"
            } else {
                "xdg-open"
            };
            Ok(CommandSpec {
                argv: vec![launcher.into(), uri.clone()],
                working_dir: None,
                desktop_file: None,
                environment: BTreeMap::new(),
                windows_metadata: None,
            })
        }
    }
}

pub fn command_from_desktop_file(path: &Path) -> Result<CommandSpec, String> {
    command_from_desktop_file_with_context(
        path,
        &DesktopLaunchContext {
            desktop_file: path.to_path_buf(),
            ..DesktopLaunchContext::default()
        },
    )
}

pub fn command_from_desktop_file_with_context(
    path: &Path,
    context: &DesktopLaunchContext,
) -> Result<CommandSpec, String> {
    let entry = parse_desktop_entry(path)?;
    if entry
        .get("Type")
        .map(String::as_str)
        .unwrap_or("Application")
        != "Application"
    {
        return Err("desktop entry is not an application".into());
    }
    if entry.get("Hidden").is_some_and(|v| truthy(v))
        || entry.get("NoDisplay").is_some_and(|v| truthy(v))
    {
        return Err("desktop entry is hidden".into());
    }
    let exec = entry
        .get("Exec")
        .ok_or_else(|| "desktop entry has no Exec".to_string())?;
    let mut effective_context = context.clone();
    if effective_context.desktop_file.as_os_str().is_empty() {
        effective_context.desktop_file = path.to_path_buf();
    }
    effective_context.name = entry.get("Name").cloned();
    effective_context.icon = entry.get("Icon").cloned();
    let argv = parse_exec_line_with_context(exec, &effective_context)?;
    if argv.is_empty() {
        return Err("desktop Exec is empty".into());
    }
    Ok(CommandSpec {
        argv,
        working_dir: entry
            .get("Path")
            .filter(|v| !v.is_empty())
            .map(PathBuf::from),
        desktop_file: Some(path.to_path_buf()),
        environment: BTreeMap::new(),
        windows_metadata: None,
    })
}

pub fn parse_exec_line(line: &str) -> Result<Vec<String>, String> {
    parse_exec_line_with_context(line, &DesktopLaunchContext::default())
}

pub fn parse_exec_line_with_context(
    line: &str,
    context: &DesktopLaunchContext,
) -> Result<Vec<String>, String> {
    let mut args = Vec::new();
    let mut current = String::new();
    let mut chars = line.chars().peekable();
    let mut quote: Option<char> = None;
    let mut arg_started = false;
    let mut field_code: Option<char> = None;

    while let Some(ch) = chars.next() {
        match ch {
            '\'' | '"' if quote.is_none() => {
                quote = Some(ch);
                arg_started = true;
            }
            '\'' | '"' if quote == Some(ch) => {
                quote = None;
                arg_started = true;
            }
            '\\' => {
                if field_code.is_some() {
                    return Err("desktop field code must be the only field in its argument".into());
                }
                if let Some(next) = chars.next() {
                    current.push(next);
                    arg_started = true;
                }
            }
            '%' => match chars.next() {
                Some('%') => {
                    if field_code.is_some() {
                        return Err(
                            "desktop field code must be the only field in its argument".into()
                        );
                    }
                    current.push('%');
                    arg_started = true;
                }
                Some(code @ ('f' | 'F' | 'u' | 'U' | 'i' | 'c' | 'k')) => {
                    if field_code.is_some() || !current.is_empty() {
                        return Err(format!(
                            "desktop field code %{code} must be the only field in its argument"
                        ));
                    }
                    field_code = Some(code);
                    arg_started = true;
                }
                Some(code) => return Err(format!("unknown desktop field code %{code}")),
                None => return Err("desktop field code is missing a code".into()),
            },
            ch if ch.is_whitespace() && quote.is_none() => {
                finish_exec_arg_with_context(
                    &mut args,
                    &mut current,
                    &mut arg_started,
                    &mut field_code,
                    context,
                )?;
            }
            _ => {
                if field_code.is_some() {
                    return Err("desktop field code must be the only field in its argument".into());
                }
                current.push(ch);
                arg_started = true;
            }
        }
    }

    if quote.is_some() {
        return Err("unterminated quote in Exec".into());
    }
    finish_exec_arg_with_context(
        &mut args,
        &mut current,
        &mut arg_started,
        &mut field_code,
        context,
    )?;
    Ok(args)
}

fn finish_exec_arg_with_context(
    args: &mut Vec<String>,
    current: &mut String,
    arg_started: &mut bool,
    field_code: &mut Option<char>,
    context: &DesktopLaunchContext,
) -> Result<(), String> {
    if let Some(code) = field_code.take() {
        args.extend(expand_field_code(code, context));
    } else if *arg_started {
        args.push(std::mem::take(current));
    }
    current.clear();
    *arg_started = false;
    Ok(())
}

fn expand_field_code(code: char, context: &DesktopLaunchContext) -> Vec<String> {
    match code {
        'f' => context.files.first().cloned().into_iter().collect(),
        'F' => context.files.clone(),
        'u' => uri_targets(context).first().cloned().into_iter().collect(),
        'U' => uri_targets(context),
        'i' => context
            .icon
            .as_deref()
            .filter(|value| !value.is_empty())
            .map(|value| vec!["--icon".into(), value.into()])
            .unwrap_or_default(),
        'c' => context
            .name
            .as_deref()
            .filter(|value| !value.is_empty())
            .map(|value| vec![value.into()])
            .unwrap_or_default(),
        'k' => context
            .desktop_file
            .to_str()
            .filter(|value| !value.is_empty())
            .map(|value| vec![value.into()])
            .unwrap_or_default(),
        _ => Vec::new(),
    }
}

fn uri_targets(context: &DesktopLaunchContext) -> Vec<String> {
    if !context.targets.is_empty() {
        return context
            .targets
            .iter()
            .map(|target| match target {
                LaunchTarget::File { path } => format!("file://{}", path.replace(' ', "%20")),
                LaunchTarget::Url { url } => url.clone(),
            })
            .collect();
    }
    let mut targets = context.urls.clone();
    targets.extend(
        context
            .files
            .iter()
            .map(|path| format!("file://{}", path.replace(' ', "%20"))),
    );
    targets
}

pub fn matching_rule<'a>(
    config: &'a LaunchConfig,
    request: &LaunchRequest,
    executable: Option<&str>,
    command_text: &str,
) -> Option<&'a Rule> {
    let desktop_id = match request {
        LaunchRequest::Desktop { id, .. } => Some(
            Path::new(id)
                .file_name()
                .and_then(|v| v.to_str())
                .unwrap_or(id.as_str()),
        ),
        _ => None,
    };
    let steam_appid = match request {
        LaunchRequest::Steam { uri } => extract_steam_appid(uri),
        LaunchRequest::Url { url } => extract_steam_appid(url),
        LaunchRequest::Argv { .. } => None,
        _ => None,
    };

    config.rules.iter().find(|rule| {
        rule.desktop_id
            .as_deref()
            .zip(desktop_id)
            .is_some_and(|(rule_id, id)| rule_id == id || rule_id == request_target(request))
            || rule
                .executable
                .as_deref()
                .zip(executable)
                .is_some_and(|(needle, exe)| executable_matches(needle, exe))
            || rule
                .command_substring
                .as_deref()
                .is_some_and(|needle| command_text.contains(needle))
            || rule
                .steam_appid
                .as_deref()
                .zip(steam_appid.as_deref())
                .is_some_and(|(needle, appid)| needle == appid)
    })
}

pub fn extract_steam_appid(uri: &str) -> Option<String> {
    let text = uri.trim();
    if let Some(rest) = text.strip_prefix("steam://rungameid/") {
        return rest
            .split(['/', '?', '&'])
            .next()
            .filter(|v| v.chars().all(char::is_numeric))
            .map(str::to_string);
    }
    if let Some(index) = text.find("/app/") {
        let rest = &text[index + 5..];
        return rest
            .split(['/', '?', '&'])
            .next()
            .filter(|v| v.chars().all(char::is_numeric))
            .map(str::to_string);
    }
    None
}

pub fn append_history(record: &LaunchRecord, limit: usize) -> Result<(), String> {
    let path = history_path();
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|err| format!("create history dir: {err}"))?;
    }
    let line = serde_json::to_string(record).map_err(|err| format!("history json: {err}"))?;
    let mut file = OpenOptions::new()
        .create(true)
        .append(true)
        .open(&path)
        .map_err(|err| format!("open history: {err}"))?;
    writeln!(file, "{line}").map_err(|err| format!("write history: {err}"))?;
    trim_history(&path, limit);
    Ok(())
}

pub fn read_history(limit: usize) -> Vec<String> {
    let text = fs::read_to_string(history_path()).unwrap_or_default();
    let mut lines: Vec<String> = text.lines().map(str::to_string).collect();
    if lines.len() > limit {
        lines = lines.split_off(lines.len() - limit);
    }
    lines
}

fn send_launch_request(request: &LaunchRequest) -> Result<LaunchRecord, LaunchdError> {
    let path = launchd_socket_path();
    let mut stream = UnixStream::connect(&path).map_err(|_err| LaunchdError::Connect)?;
    stream
        .set_read_timeout(Some(Duration::from_secs(3)))
        .map_err(|err| LaunchdError::Request(format!("set launchd read timeout: {err}")))?;
    stream
        .set_write_timeout(Some(Duration::from_secs(1)))
        .map_err(|err| LaunchdError::Request(format!("set launchd write timeout: {err}")))?;
    let text = serde_json::to_string(request)
        .map_err(|err| LaunchdError::Request(format!("request json: {err}")))?;
    writeln!(stream, "{text}")
        .map_err(|err| LaunchdError::Request(format!("write launchd request: {err}")))?;
    stream
        .shutdown(Shutdown::Write)
        .map_err(|err| LaunchdError::Request(format!("finish launchd request: {err}")))?;
    let mut response = String::new();
    stream
        .read_to_string(&mut response)
        .map_err(|err| LaunchdError::Request(format!("read launchd response: {err}")))?;
    let response: LaunchDaemonResponse = serde_json::from_str(&response)
        .map_err(|err| LaunchdError::Request(format!("parse launchd response: {err}")))?;
    if response.ok {
        response
            .record
            .ok_or_else(|| LaunchdError::Request("launchd returned empty success".into()))
    } else {
        Err(LaunchdError::Request(
            response.error.unwrap_or_else(|| "launchd failed".into()),
        ))
    }
}

fn handle_launchd_stream(stream: &mut UnixStream) -> LaunchDaemonResponse {
    let mut text = String::new();
    if let Err(err) = stream.read_to_string(&mut text) {
        return LaunchDaemonResponse {
            ok: false,
            record: None,
            error: Some(format!("read request: {err}")),
        };
    }
    let request = match serde_json::from_str::<LaunchRequest>(text.trim()) {
        Ok(request) => request,
        Err(err) => {
            return LaunchDaemonResponse {
                ok: false,
                record: None,
                error: Some(format!("parse request: {err}")),
            };
        }
    };
    match run_launch(request) {
        Ok(record) => LaunchDaemonResponse {
            ok: true,
            record: Some(record),
            error: None,
        },
        Err(err) => LaunchDaemonResponse {
            ok: false,
            record: None,
            error: Some(err),
        },
    }
}

fn apply_rule(
    config: &LaunchConfig,
    command: &mut CommandSpec,
    rule: &Rule,
    details: &mut Vec<String>,
) {
    if let Some(dir) = &rule.working_dir {
        command.working_dir = Some(PathBuf::from(expand_home(dir)));
    }

    prepend_env(command, rule.env.clone());

    let wants_nvidia =
        rule.nvidia_env.unwrap_or(false) || (config.allow_nvidia_env && nvidia_available());
    if wants_nvidia {
        prepend_env(command, nvidia_env_vars());
    }

    if let Some(preset) = &rule.steam_compat_preset {
        apply_steam_compat(command, preset);
        details.push(format!("steam compat preset: {preset}"));
    }

    if let Some(preset) = &rule.game_mode_preset {
        apply_game_mode(command, preset);
        details.push(format!("game mode preset: {preset}"));
    }
}

fn prefer_desktop_launcher(command: &mut CommandSpec, targets: &[LaunchTarget]) {
    let Some(path) = command.desktop_file.as_ref() else {
        return;
    };
    if command_available("gio") {
        let mut argv = vec![
            "gio".into(),
            "launch".into(),
            path.to_string_lossy().to_string(),
        ];
        for target in targets {
            match target {
                LaunchTarget::File { path } => argv.push(expand_home(path)),
                LaunchTarget::Url { url } => argv.push(url.clone()),
            }
        }
        command.argv = argv;
        command.working_dir = None;
    }
}

fn apply_game_mode(command: &mut CommandSpec, preset: &str) {
    match preset {
        "gamemode" if command_available("gamemoderun") => {
            command.argv.insert(0, "gamemoderun".into())
        }
        "gamescope" if command_available("gamescope") => {
            let mut next = vec!["gamescope".into(), "--".into()];
            next.extend(command.argv.clone());
            command.argv = next;
        }
        "gamescope-gamemode" => {
            let mut next = Vec::new();
            if command_available("gamemoderun") {
                next.push("gamemoderun".into());
            }
            if command_available("gamescope") {
                next.extend(["gamescope".into(), "--".into()]);
            }
            next.extend(command.argv.clone());
            command.argv = next;
        }
        _ => {}
    }
}

fn apply_steam_compat(command: &mut CommandSpec, preset: &str) {
    if preset.is_empty() || preset == "default" {
        return;
    }
    prepend_env(command, vec![("STEAM_COMPAT_CONFIG".into(), preset.into())]);
}

fn prepend_env(command: &mut CommandSpec, vars: Vec<(String, String)>) {
    for (key, value) in vars {
        if !key.is_empty() && !key.contains('=') {
            command.environment.insert(key, value);
        }
    }
}

fn spawn_command(command: &CommandSpec, isolate_launches: bool) -> Result<u32, String> {
    spawn_command_with(
        command,
        isolate_launches,
        command_available("systemd-run"),
        spawn_command_systemd,
        spawn_command_direct,
    )
}

fn spawn_command_with<FSystemd, FDirect>(
    command: &CommandSpec,
    isolate_launches: bool,
    systemd_available: bool,
    systemd_spawn: FSystemd,
    direct_spawn: FDirect,
) -> Result<u32, String>
where
    FSystemd: Fn(&CommandSpec) -> Result<u32, String>,
    FDirect: Fn(&CommandSpec) -> Result<u32, String>,
{
    if isolate_launches && systemd_available {
        match systemd_spawn(command) {
            Ok(pid) => return Ok(pid),
            Err(_) => return direct_spawn(command),
        }
    }
    direct_spawn(command)
}

fn spawn_command_direct(command: &CommandSpec) -> Result<u32, String> {
    let Some(program) = command.argv.first() else {
        return Err("empty command".into());
    };
    let mut process = Command::new(program);
    process.args(&command.argv[1..]);
    process.process_group(0);
    process.envs(&command.environment);
    if let Some(dir) = &command.working_dir {
        process.current_dir(dir);
    }
    process
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null());
    process
        .spawn()
        .map(|mut child| {
            let pid = child.id();
            thread::spawn(move || {
                let _ = child.wait();
            });
            pid
        })
        .map_err(|err| format!("spawn failed: {err}"))
}

fn spawn_command_systemd(command: &CommandSpec) -> Result<u32, String> {
    let Some(program) = command.argv.first() else {
        return Err("empty command".into());
    };
    let unit = transient_launch_unit_name();
    let args = systemd_run_args(command, &unit);
    let status = Command::new("systemd-run")
        .args(&args)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status()
        .map_err(|err| format!("systemd-run failed: {err}"))?;
    if !status.success() {
        return Err(format!("systemd-run exited with {status}"));
    }

    let pid = wait_for_unit_main_pid(&unit).unwrap_or(0);
    if pid == 0 {
        // systemd-run --no-block can succeed before MainPID is visible;
        // treat launch as accepted and return a sentinel pid.
        return Ok(0);
    }
    if program.is_empty() {
        return Err("empty command".into());
    }
    Ok(pid)
}

fn systemd_run_args(command: &CommandSpec, unit: &str) -> Vec<String> {
    let mut args = vec![
        "--user".into(),
        "--collect".into(),
        "--quiet".into(),
        "--no-block".into(),
        format!("--unit={unit}"),
        "--property=ExitType=cgroup".into(),
        "--property=Slice=app.slice".into(),
        "--property=StartupCPUWeight=10000".into(),
        "--property=StartupIOWeight=10000".into(),
    ];
    if let Some(dir) = &command.working_dir {
        args.push(format!("--working-directory={}", dir.to_string_lossy()));
    }
    for (key, value) in &command.environment {
        args.push(format!("--setenv={key}={value}"));
    }
    args.push("--".into());
    args.extend(command.argv.clone());
    args
}

fn transient_launch_unit_name() -> String {
    format!("astrea-launch-{}-{}.service", process::id(), now_ms())
}

fn wait_for_unit_main_pid(unit: &str) -> Option<u32> {
    for _ in 0..10 {
        if let Some(pid) = unit_main_pid(unit) {
            if pid > 0 {
                return Some(pid);
            }
        }
        thread::sleep(Duration::from_millis(20));
    }
    None
}

fn unit_main_pid(unit: &str) -> Option<u32> {
    let output = Command::new("systemctl")
        .args(["--user", "show", unit, "--property=MainPID", "--value"])
        .stdin(Stdio::null())
        .stderr(Stdio::null())
        .output()
        .ok()?;
    if !output.status.success() {
        return None;
    }
    String::from_utf8_lossy(&output.stdout)
        .trim()
        .parse::<u32>()
        .ok()
}

fn normalize_launch_pid(pid: u32) -> Option<u32> {
    if pid == 0 { None } else { Some(pid) }
}

fn request_boost(config: &LaunchConfig, reason: &str, pid: Option<u32>, duration_ms: u64) {
    if !config.latency.enabled {
        return;
    }
    let payload = serde_json::json!({
        "op": "boost",
        "reason": reason,
        "pid": pid,
        "duration_ms": duration_ms,
        "source": "astrea-launch"
    });
    let text = payload.to_string();

    for socket in &config.latency.socket_paths {
        let path = PathBuf::from(expand_home(socket));
        if !path.exists() {
            continue;
        }
        if let Ok(mut stream) = UnixStream::connect(&path) {
            let _ = writeln!(stream, "{text}");
            return;
        }
    }

    if let Some(command) = &config.latency.command {
        if let Some(program) = command.first() {
            let mut child = Command::new(program);
            for arg in &command[1..] {
                child.arg(
                    arg.replace("{reason}", reason)
                        .replace("{duration_ms}", &duration_ms.to_string())
                        .replace("{pid}", &pid.map(|p| p.to_string()).unwrap_or_default()),
                );
            }
            let _ = child
                .stdin(Stdio::null())
                .stdout(Stdio::null())
                .stderr(Stdio::null())
                .spawn();
        }
    }
}

fn parse_desktop_entry(path: &Path) -> Result<std::collections::BTreeMap<String, String>, String> {
    let text = fs::read_to_string(path).map_err(|err| format!("read desktop file: {err}"))?;
    let mut in_entry = false;
    let mut values = std::collections::BTreeMap::new();
    for raw in text.lines() {
        let line = raw.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        if line.starts_with('[') && line.ends_with(']') {
            in_entry = line == "[Desktop Entry]";
            continue;
        }
        if !in_entry {
            continue;
        }
        if let Some((key, value)) = line.split_once('=') {
            values.insert(key.trim().to_string(), value.trim().to_string());
        }
    }
    Ok(values)
}

fn resolve_desktop_entry(id: &str) -> Option<PathBuf> {
    let expanded = PathBuf::from(expand_home(id));
    if expanded.is_file() {
        return Some(expanded);
    }

    let file_name = if id.ends_with(".desktop") {
        id.to_string()
    } else {
        format!("{id}.desktop")
    };
    for dir in application_dirs().into_iter().chain([xdg_desktop_dir()]) {
        let direct = dir.join(&file_name);
        if direct.is_file() {
            return Some(direct);
        }
        let root = dir;
        let mut pending = vec![root.clone()];
        while let Some(current) = pending.pop() {
            let Ok(entries) = fs::read_dir(&current) else {
                continue;
            };
            for entry in entries.flatten() {
                let path = entry.path();
                if path.is_dir() {
                    pending.push(path);
                    continue;
                }
                if path.extension().and_then(|value| value.to_str()) != Some("desktop") {
                    continue;
                }
                let relative = path.strip_prefix(&root).ok()?;
                let canonical = relative
                    .to_string_lossy()
                    .replace(std::path::MAIN_SEPARATOR, "-");
                if canonical == file_name {
                    return Some(path);
                }
            }
        }
    }
    None
}

fn application_dirs() -> Vec<PathBuf> {
    let mut dirs = vec![xdg_data_home().join("applications")];
    for entry in env::var("XDG_DATA_DIRS")
        .unwrap_or_else(|_| "/usr/local/share:/usr/share".into())
        .split(':')
    {
        if !entry.is_empty() {
            dirs.push(PathBuf::from(entry).join("applications"));
        }
    }
    dirs
}

fn xdg_config_home() -> PathBuf {
    env::var_os("XDG_CONFIG_HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|| home_dir().join(".config"))
}

fn xdg_data_home() -> PathBuf {
    env::var_os("XDG_DATA_HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|| home_dir().join(".local/share"))
}

fn xdg_state_home() -> PathBuf {
    env::var_os("XDG_STATE_HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|| home_dir().join(".local/state"))
}

fn xdg_runtime_dir() -> Option<PathBuf> {
    env::var_os("XDG_RUNTIME_DIR").map(PathBuf::from)
}

fn xdg_desktop_dir() -> PathBuf {
    let config = xdg_config_home().join("user-dirs.dirs");
    if let Ok(text) = fs::read_to_string(config) {
        for line in text.lines() {
            let line = line.trim();
            if let Some(value) = line.strip_prefix("XDG_DESKTOP_DIR=") {
                return PathBuf::from(expand_home(value.trim().trim_matches('"')));
            }
        }
    }
    home_dir().join("Desktop")
}

fn home_dir() -> PathBuf {
    env::var_os("HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("/"))
}

fn default_latency_socket_paths() -> Vec<String> {
    let mut paths = Vec::new();
    if let Some(runtime) = xdg_runtime_dir() {
        paths.push(
            runtime
                .join("Astrea/astrea-latencyd.sock")
                .to_string_lossy()
                .to_string(),
        );
        paths.push(
            runtime
                .join("astrea-latencyd.sock")
                .to_string_lossy()
                .to_string(),
        );
    } else {
        paths.push(
            xdg_state_home()
                .join("Astrea/runtime/astrea-latencyd.sock")
                .to_string_lossy()
                .to_string(),
        );
    }
    paths
}

fn trim_history(path: &Path, limit: usize) {
    if limit == 0 {
        return;
    }
    let Ok(text) = fs::read_to_string(path) else {
        return;
    };
    let lines: Vec<&str> = text.lines().collect();
    if lines.len() <= limit {
        return;
    }
    let keep = lines[lines.len() - limit..].join("\n") + "\n";
    let _ = fs::write(path, keep);
}

fn validate_url(url: &str) -> Result<(), String> {
    let Some((scheme, _)) = url.split_once(':') else {
        return Err("unsupported url scheme".into());
    };
    if !scheme.is_empty()
        && scheme
            .chars()
            .all(|ch| ch.is_ascii_alphanumeric() || matches!(ch, '+' | '-' | '.'))
        && scheme
            .chars()
            .next()
            .is_some_and(|ch| ch.is_ascii_alphabetic())
    {
        Ok(())
    } else {
        Err("unsupported url scheme".into())
    }
}

fn validate_steam_uri(uri: &str) -> Result<(), String> {
    if uri.starts_with("steam://")
        || uri.starts_with("https://store.steampowered.com/")
        || uri.starts_with("https://steamcommunity.com/")
    {
        Ok(())
    } else {
        Err("unsupported Steam URI".into())
    }
}

fn executable_matches(needle: &str, executable: &str) -> bool {
    let exe_name = Path::new(executable)
        .file_name()
        .and_then(|v| v.to_str())
        .unwrap_or(executable);
    needle == executable || needle == exe_name || executable.contains(needle)
}

fn command_available(name: &str) -> bool {
    if name.contains('/') {
        return Path::new(name).is_file();
    }
    env::var_os("PATH")
        .and_then(|paths| {
            env::split_paths(&paths)
                .map(|dir| dir.join(name))
                .find(|path| path.is_file())
        })
        .is_some()
}

fn find_command_path(name: &str) -> Option<PathBuf> {
    if name.contains('/') {
        let path = PathBuf::from(name);
        return is_executable_file(&path).then_some(path);
    }
    env::var_os("PATH")?
        .to_string_lossy()
        .split(':')
        .find_map(|dir| {
            let path = Path::new(dir).join(name);
            is_executable_file(&path).then_some(path)
        })
}

fn is_executable_file(path: &Path) -> bool {
    path.is_file()
        && fs::metadata(path)
            .map(|metadata| metadata.permissions().mode() & 0o111 != 0)
            .unwrap_or(false)
}

fn steam_root() -> PathBuf {
    env::var_os("ASTREA_STEAM_ROOT")
        .map(PathBuf::from)
        .unwrap_or_else(|| home_dir().join(".local/share/Steam"))
}

fn find_umu_run() -> Option<PathBuf> {
    let mut candidates = Vec::new();
    if let Some(path) = find_command_path("umu-run") {
        candidates.push(path);
    }
    candidates.extend([
        xdg_data_home().join("lutris/runtime/umu/umu-run"),
        xdg_data_home().join("lutris/runtime/umu/umu_run.py"),
        PathBuf::from("/app/share/umu/umu-run"),
        PathBuf::from("/usr/local/share/umu/umu-run"),
        PathBuf::from("/usr/share/umu/umu-run"),
        PathBuf::from("/opt/umu/umu-run"),
    ]);
    candidates.into_iter().find(|path| is_executable_file(path))
}

fn find_proton() -> Option<PathBuf> {
    let root = steam_root();
    let common = root.join("steamapps/common");
    let compatibility = root.join("compatibilitytools.d");
    let mut candidates = vec![
        compatibility.join("Proton-GE Latest/proton"),
        common.join("Proton - Experimental/proton"),
    ];
    append_named_children(&common, &mut candidates, |name| {
        name.starts_with("Proton ") && name != "Proton - Experimental"
    });
    append_named_children(&compatibility, &mut candidates, |name| {
        name.starts_with("GE-Proton") || name.starts_with("Proton-GE")
    });
    candidates.extend([PathBuf::from(
        "/usr/share/steam/compatibilitytools.d/proton-cachyos/proton",
    )]);
    candidates.into_iter().find(|path| is_executable_file(path))
}

fn append_named_children<F>(root: &Path, candidates: &mut Vec<PathBuf>, predicate: F)
where
    F: Fn(&str) -> bool,
{
    let Ok(entries) = fs::read_dir(root) else {
        return;
    };
    let mut names = entries
        .flatten()
        .filter_map(|entry| {
            let name = entry.file_name().to_string_lossy().to_string();
            predicate(&name).then_some((name, entry.path()))
        })
        .collect::<Vec<_>>();
    names.sort_by(|left, right| right.0.cmp(&left.0));
    candidates.extend(names.into_iter().map(|(_, path)| path.join("proton")));
}

fn find_runtime_interface() -> Option<PathBuf> {
    let root = steam_root().join("steamapps/common");
    let direct = [
        "SteamLinuxRuntime_sniper/pressure-vessel/bin/steam-runtime-launcher-interface-0",
        "SteamLinuxRuntime_4/pressure-vessel/bin/steam-runtime-launcher-interface-0",
        "SteamLinuxRuntime_soldier/pressure-vessel/bin/steam-runtime-launcher-interface-0",
    ];
    for relative in direct {
        let path = root.join(relative);
        if is_executable_file(&path) {
            return path.parent().map(Path::to_path_buf);
        }
    }
    None
}

fn has_32bit_gamemode_auto() -> bool {
    let dirs = env::var_os("ASTREA_LIB32_DIRS")
        .map(|value| env::split_paths(&value).collect::<Vec<_>>())
        .unwrap_or_else(|| {
            vec![
                PathBuf::from("/usr/lib32"),
                PathBuf::from("/usr/lib/i386-linux-gnu"),
                PathBuf::from("/lib/i386-linux-gnu"),
            ]
        });
    dirs.into_iter()
        .any(|dir| dir.join("libgamemodeauto.so.0").is_file())
}

fn command_for_file_path(path: &str) -> Result<Vec<String>, String> {
    let target = Path::new(path);
    if target.extension().and_then(|v| v.to_str()) == Some("desktop") && target.is_file() {
        return Ok(command_from_desktop_file(target)?.argv);
    }
    if is_shell_script(target) {
        return Ok(
            terminal_command_for_script(path).unwrap_or_else(|| vec!["sh".into(), path.into()])
        );
    }
    if is_direct_executable(target) {
        return Ok(vec![path.into()]);
    }
    Ok(vec!["xdg-open".into(), path.into()])
}

fn working_dir_for_file(path: &str) -> Option<PathBuf> {
    let target = PathBuf::from(expand_home(path));
    if is_direct_executable(&target) || is_shell_script(&target) {
        return target.parent().map(Path::to_path_buf);
    }
    None
}

fn is_shell_script(path: &Path) -> bool {
    path.extension()
        .and_then(|v| v.to_str())
        .is_some_and(|ext| ext.eq_ignore_ascii_case("sh"))
}

fn is_direct_executable(path: &Path) -> bool {
    if !path.is_file() {
        return false;
    }
    let name = path
        .file_name()
        .and_then(|v| v.to_str())
        .unwrap_or_default();
    let known_exec_ext = [".appimage", ".run", ".bin", ".elf", ".x86_64", ".bundle"]
        .iter()
        .any(|suffix| name.to_ascii_lowercase().ends_with(suffix));
    let no_extension = !name.contains('.');
    if !known_exec_ext && !no_extension {
        return false;
    }
    let Ok(bytes) = fs::read(path) else {
        return false;
    };
    bytes.starts_with(b"\x7fELF") || bytes.starts_with(b"#!")
}

fn terminal_command_for_script(path: &str) -> Option<Vec<String>> {
    let script = expand_home(path);
    let dir = Path::new(&script)
        .parent()
        .map(Path::to_path_buf)
        .unwrap_or_else(home_dir);
    let inner = format!(
        "cd -- '{}' && sh '{}'",
        shell_quote(&dir.to_string_lossy()),
        shell_quote(&script)
    );
    if command_available("xdg-terminal-exec") {
        return Some(vec![
            "xdg-terminal-exec".into(),
            "sh".into(),
            "-lc".into(),
            inner,
        ]);
    }
    for terminal in [
        "x-terminal-emulator",
        "kitty",
        "foot",
        "ghostty",
        "alacritty",
    ] {
        if command_available(terminal) {
            return Some(vec![
                terminal.into(),
                "-e".into(),
                "sh".into(),
                "-lc".into(),
                inner,
            ]);
        }
    }
    None
}

fn shell_quote(value: &str) -> String {
    value.replace('\'', "'\"'\"'")
}

fn nvidia_available() -> bool {
    Path::new("/proc/driver/nvidia/version").exists() || command_available("nvidia-smi")
}

fn nvidia_env_vars() -> Vec<(String, String)> {
    vec![
        ("__GL_THREADED_OPTIMIZATIONS".into(), "1".into()),
        ("__GL_SHADER_DISK_CACHE".into(), "1".into()),
        ("__GLX_VENDOR_LIBRARY_NAME".into(), "nvidia".into()),
    ]
}

fn expand_home(value: &str) -> String {
    if value == "~" || value == "$HOME" {
        return home_dir().to_string_lossy().to_string();
    }
    if let Some(rest) = value.strip_prefix("~/") {
        return home_dir().join(rest).to_string_lossy().to_string();
    }
    if let Some(rest) = value.strip_prefix("$HOME/") {
        return home_dir().join(rest).to_string_lossy().to_string();
    }
    value.to_string()
}

fn request_kind(request: &LaunchRequest) -> &'static str {
    match request {
        LaunchRequest::Desktop { .. } => "desktop",
        LaunchRequest::Command { .. } => "command",
        LaunchRequest::Argv { .. } => "argv",
        LaunchRequest::Windows { .. } => "windows",
        LaunchRequest::File { .. } => "file",
        LaunchRequest::Url { .. } => "url",
        LaunchRequest::Steam { .. } => "steam",
    }
}

fn request_target(request: &LaunchRequest) -> &str {
    match request {
        LaunchRequest::Desktop { id, .. } => id,
        LaunchRequest::Command { command } => command,
        LaunchRequest::Argv { argv, .. } => argv.first().map(String::as_str).unwrap_or(""),
        LaunchRequest::Windows { path } => path,
        LaunchRequest::File { path } => path,
        LaunchRequest::Url { url } => url,
        LaunchRequest::Steam { uri } => uri,
    }
}

fn truthy(value: &str) -> bool {
    matches!(value.to_ascii_lowercase().as_str(), "1" | "true" | "yes")
}

fn now_ms() -> u128 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_millis())
        .unwrap_or(0)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn launch_isolation_is_enabled_by_default() {
        assert!(LaunchConfig::default().isolate_launches);
    }

    #[test]
    fn default_latency_sockets_do_not_use_world_writable_tmp() {
        let config = LaunchConfig::default();
        assert!(
            config
                .latency
                .socket_paths
                .iter()
                .all(|path| !path.starts_with("/tmp/")),
            "latency sockets must stay in the user runtime/state area"
        );
    }

    #[test]
    fn builds_systemd_run_args_for_transient_app_service() {
        let mut environment = BTreeMap::new();
        environment.insert(
            "SECRET_VALUE".into(),
            "must-stay-out-of-argv-history".into(),
        );
        let command = CommandSpec {
            argv: vec!["/usr/bin/example".into(), "--flag".into()],
            working_dir: Some(PathBuf::from("/tmp/example")),
            desktop_file: None,
            environment,
            windows_metadata: None,
        };

        let args = systemd_run_args(&command, "astrea-launch-test.service");

        assert!(args.contains(&"--user".into()));
        assert!(args.contains(&"--collect".into()));
        assert!(args.contains(&"--no-block".into()));
        assert!(args.contains(&"--property=ExitType=cgroup".into()));
        assert!(args.contains(&"--working-directory=/tmp/example".into()));
        assert!(args.contains(&"--property=StartupCPUWeight=10000".into()));
        assert!(args.contains(&"--property=StartupIOWeight=10000".into()));
        assert!(args.contains(&"--setenv=SECRET_VALUE=must-stay-out-of-argv-history".into()));
        assert_eq!(args.iter().filter(|arg| arg.as_str() == "--").count(), 1);
        assert_eq!(
            &args[args.len() - 3..],
            &[
                "--".to_string(),
                "/usr/bin/example".to_string(),
                "--flag".to_string()
            ]
        );
    }
}

#[cfg(test)]
mod launch_spawn_tests {
    use super::*;
    use std::sync::atomic::{AtomicBool, Ordering};

    fn sample_command() -> CommandSpec {
        CommandSpec {
            argv: vec!["/usr/bin/true".into()],
            working_dir: None,
            desktop_file: None,
            environment: BTreeMap::new(),
            windows_metadata: None,
        }
    }

    #[test]
    fn systemd_success_with_zero_pid_does_not_fallback_to_direct_spawn() {
        let direct_called = AtomicBool::new(false);
        let result = spawn_command_with(
            &sample_command(),
            true,
            true,
            |_| Ok(0),
            |_| {
                direct_called.store(true, Ordering::SeqCst);
                Ok(42)
            },
        );

        assert_eq!(result.expect("launch accepted"), 0);
        assert!(!direct_called.load(Ordering::SeqCst));
    }

    #[test]
    fn systemd_failure_falls_back_to_direct_spawn() {
        let direct_called = AtomicBool::new(false);
        let result = spawn_command_with(
            &sample_command(),
            true,
            true,
            |_| Err("systemd failed".into()),
            |_| {
                direct_called.store(true, Ordering::SeqCst);
                Ok(99)
            },
        );

        assert_eq!(result.expect("fallback launch"), 99);
        assert!(direct_called.load(Ordering::SeqCst));
    }

    #[test]
    fn zero_pid_is_treated_as_none_for_pid_boosting() {
        assert_eq!(normalize_launch_pid(0), None);
        assert_eq!(normalize_launch_pid(1234), Some(1234));
    }
}
