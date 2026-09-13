use std::ffi::OsString;
use std::fs;
use std::io::{self, Read};
use std::path::{Component, Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::mpsc::{SyncSender, TrySendError, sync_channel};
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use serde::Deserialize;
use serde_json::{Value, json};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum FormatId {
    Zip,
    SevenZip,
    Tar,
    TarGz,
    TarXz,
    TarZst,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CompressionProfile {
    Fast,
    Balanced,
    Maximum,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ProviderKind {
    Bsdtar,
    SevenZip,
    Rar,
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct ProviderAvailability {
    pub bsdtar: bool,
    pub seven_zip: bool,
    pub rar: bool,
    pub bsdtar_zstd: bool,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CreatePlan {
    pub format: FormatId,
    pub provider: ProviderKind,
    pub canonical_extension: &'static str,
    pub profiles: &'static [CompressionProfile],
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Capability {
    pub id: String,
    pub label: String,
    pub extension: String,
    pub create_supported: bool,
    pub extract_supported: bool,
    pub profiles: Vec<CompressionProfile>,
    pub password_supported: bool,
    pub provider: ProviderKind,
}

#[derive(Debug, Eq, PartialEq)]
pub enum ArchiveError {
    ProviderUnavailable,
    UnsupportedFormat,
    InvalidSource(String),
    Cancelled,
    PasswordRequired,
    BadPassword,
    DestinationConflict(PathBuf),
    UnsafeMember(String),
    ProviderFailed(String),
    InvalidRequest(String),
}

impl ArchiveError {
    pub fn code(&self) -> &'static str {
        match self {
            Self::ProviderUnavailable => "provider-unavailable",
            Self::UnsupportedFormat => "unsupported-format",
            Self::InvalidSource(_) => "invalid-source",
            Self::Cancelled => "cancelled",
            Self::PasswordRequired => "password-required",
            Self::BadPassword => "bad-password",
            Self::DestinationConflict(_) => "destination-conflict",
            Self::UnsafeMember(_) => "unsafe-member",
            Self::ProviderFailed(_) => "provider-failed",
            Self::InvalidRequest(_) => "invalid-source",
        }
    }

    fn message(&self) -> String {
        match self {
            Self::ProviderUnavailable => "required archive provider is unavailable".into(),
            Self::UnsupportedFormat => "archive format is not supported".into(),
            Self::InvalidSource(message)
            | Self::UnsafeMember(message)
            | Self::ProviderFailed(message)
            | Self::InvalidRequest(message) => message.clone(),
            Self::Cancelled => "archive operation cancelled".into(),
            Self::PasswordRequired => String::new(),
            Self::BadPassword => "archive password is incorrect".into(),
            Self::DestinationConflict(path) => {
                format!("archive destination already exists: {}", path.display())
            }
        }
    }
}

impl FormatId {
    fn from_id(value: &str) -> Option<Self> {
        match value {
            "zip" => Some(Self::Zip),
            "7z" => Some(Self::SevenZip),
            "tar" => Some(Self::Tar),
            "tar.gz" | "tgz" => Some(Self::TarGz),
            "tar.xz" | "txz" => Some(Self::TarXz),
            "tar.zst" | "tzst" => Some(Self::TarZst),
            _ => None,
        }
    }
}

impl CompressionProfile {
    fn from_id(value: &str) -> Option<Self> {
        match value {
            "fast" => Some(Self::Fast),
            "balanced" => Some(Self::Balanced),
            "maximum" => Some(Self::Maximum),
            _ => None,
        }
    }

    fn as_id(self) -> &'static str {
        match self {
            Self::Fast => "fast",
            Self::Balanced => "balanced",
            Self::Maximum => "maximum",
        }
    }
}

impl ProviderKind {
    fn as_id(self) -> &'static str {
        match self {
            Self::Bsdtar => "bsdtar",
            Self::SevenZip => "7z",
            Self::Rar => "rar",
        }
    }
}

pub fn discover_capabilities() -> Vec<Capability> {
    capabilities_for(discover_providers())
}

fn discover_providers() -> ProviderAvailability {
    let bsdtar = provider_responds("bsdtar", "--version");
    let seven_zip = provider_responds("7z", "--help");
    let rar = provider_responds("rar", "-h") && probe_rar_creation();
    let bsdtar_zstd = bsdtar && provider_help_contains("bsdtar", "zstd");
    ProviderAvailability {
        bsdtar,
        seven_zip,
        rar,
        bsdtar_zstd,
    }
}

fn provider_responds(program: &str, argument: &str) -> bool {
    Command::new(program)
        .arg(argument)
        .env("LC_ALL", "C")
        .output()
        .map(|output| output.status.success())
        .unwrap_or(false)
}

fn provider_help_contains(program: &str, needle: &str) -> bool {
    let Ok(output) = Command::new(program)
        .arg("--help")
        .env("LC_ALL", "C")
        .output()
    else {
        return false;
    };
    let mut text = String::from_utf8_lossy(&output.stdout).into_owned();
    text.push_str(&String::from_utf8_lossy(&output.stderr));
    text.to_ascii_lowercase()
        .contains(&needle.to_ascii_lowercase())
}

fn probe_rar_creation() -> bool {
    false
}

const ALL_PROFILES: &[CompressionProfile] = &[
    CompressionProfile::Fast,
    CompressionProfile::Balanced,
    CompressionProfile::Maximum,
];
const NO_PROFILES: &[CompressionProfile] = &[];

pub fn plan_create(
    format: FormatId,
    _profile: CompressionProfile,
    providers: ProviderAvailability,
) -> Result<CreatePlan, ArchiveError> {
    let (provider, extension, profiles) = match format {
        FormatId::Zip if providers.bsdtar => (ProviderKind::Bsdtar, "zip", ALL_PROFILES),
        FormatId::Zip if providers.seven_zip => (ProviderKind::SevenZip, "zip", ALL_PROFILES),
        FormatId::SevenZip if providers.seven_zip => (ProviderKind::SevenZip, "7z", ALL_PROFILES),
        FormatId::Tar if providers.bsdtar => (ProviderKind::Bsdtar, "tar", NO_PROFILES),
        FormatId::TarGz if providers.bsdtar => (ProviderKind::Bsdtar, "tar.gz", ALL_PROFILES),
        FormatId::TarXz if providers.bsdtar => (ProviderKind::Bsdtar, "tar.xz", ALL_PROFILES),
        FormatId::TarZst if providers.bsdtar && providers.bsdtar_zstd => {
            (ProviderKind::Bsdtar, "tar.zst", ALL_PROFILES)
        }
        FormatId::TarZst if providers.bsdtar => {
            return Err(ArchiveError::ProviderUnavailable);
        }
        FormatId::TarZst => return Err(ArchiveError::ProviderUnavailable),
        FormatId::Zip | FormatId::SevenZip | FormatId::Tar | FormatId::TarGz | FormatId::TarXz => {
            return Err(ArchiveError::ProviderUnavailable);
        }
    };

    Ok(CreatePlan {
        format,
        provider,
        canonical_extension: extension,
        profiles,
    })
}

pub fn capabilities_for(providers: ProviderAvailability) -> Vec<Capability> {
    let mut capabilities = Vec::new();
    let mut add = |id: &str,
                   label: &str,
                   extension: &str,
                   create_supported: bool,
                   extract_supported: bool,
                   profiles: &[CompressionProfile],
                   password_supported: bool,
                   provider: ProviderKind| {
        capabilities.push(Capability {
            id: id.to_string(),
            label: label.to_string(),
            extension: extension.to_string(),
            create_supported,
            extract_supported,
            profiles: profiles.to_vec(),
            password_supported,
            provider,
        });
    };

    if providers.bsdtar || providers.seven_zip {
        add(
            "zip",
            "ZIP",
            "zip",
            providers.bsdtar || providers.seven_zip,
            true,
            ALL_PROFILES,
            false,
            if providers.bsdtar {
                ProviderKind::Bsdtar
            } else {
                ProviderKind::SevenZip
            },
        );
    }
    if providers.seven_zip {
        add(
            "7z",
            "7Z",
            "7z",
            true,
            true,
            ALL_PROFILES,
            false,
            ProviderKind::SevenZip,
        );
    }
    if providers.bsdtar {
        add(
            "tar",
            "TAR",
            "tar",
            true,
            true,
            NO_PROFILES,
            false,
            ProviderKind::Bsdtar,
        );
        add(
            "tar.gz",
            "TAR.GZ",
            "tar.gz",
            true,
            true,
            ALL_PROFILES,
            false,
            ProviderKind::Bsdtar,
        );
        add(
            "tar.xz",
            "TAR.XZ",
            "tar.xz",
            true,
            true,
            ALL_PROFILES,
            false,
            ProviderKind::Bsdtar,
        );
        if providers.bsdtar_zstd {
            add(
                "tar.zst",
                "TAR.ZST",
                "tar.zst",
                true,
                true,
                ALL_PROFILES,
                false,
                ProviderKind::Bsdtar,
            );
        }
        add(
            "tar.bz2",
            "TAR.BZ2",
            "tar.bz2",
            false,
            true,
            NO_PROFILES,
            false,
            ProviderKind::Bsdtar,
        );
        if providers.rar || providers.seven_zip {
            add(
                "rar",
                "RAR",
                "rar",
                providers.rar,
                true,
                NO_PROFILES,
                providers.seven_zip,
                if providers.rar {
                    ProviderKind::Rar
                } else {
                    ProviderKind::SevenZip
                },
            );
        }
    } else if providers.seven_zip {
        add(
            "rar",
            "RAR",
            "rar",
            false,
            true,
            NO_PROFILES,
            true,
            ProviderKind::SevenZip,
        );
    }

    capabilities
}

#[derive(Debug, Deserialize)]
struct OperationRequest {
    kind: String,
    #[serde(default)]
    sources: Vec<PathBuf>,
    #[serde(rename = "archivePath", default)]
    archive_path: PathBuf,
    #[serde(default)]
    destination: PathBuf,
    #[serde(default)]
    format: String,
    #[serde(default = "default_profile")]
    profile: String,
    #[serde(default)]
    password: String,
    #[serde(default = "default_conflict_policy")]
    conflict_policy: String,
}

fn default_profile() -> String {
    "balanced".into()
}

fn default_conflict_policy() -> String {
    "keep-both".into()
}

struct Cancellation {
    marker: Option<PathBuf>,
}

impl Cancellation {
    fn from_environment() -> Self {
        Self {
            marker: std::env::var_os("ASTREA_CANCEL_FILE").map(PathBuf::from),
        }
    }

    fn is_cancelled(&self) -> bool {
        self.marker.as_ref().is_some_and(|path| path.exists())
    }
}

struct ProgressEmitter {
    last_emit: Instant,
    last_percent: i32,
}

impl ProgressEmitter {
    fn new() -> Self {
        Self {
            last_emit: Instant::now() - Duration::from_secs(1),
            last_percent: -1,
        }
    }

    fn emit(
        &mut self,
        operation: &str,
        phase: &str,
        done_count: usize,
        total_count: Option<usize>,
        bytes_done: Option<u64>,
        bytes_total: Option<u64>,
        progress: f64,
        current_path: Option<&Path>,
        current_name: Option<&str>,
        status_text: &str,
        force: bool,
    ) {
        let percent = (progress.clamp(0.0, 1.0) * 100.0).round() as i32;
        let now = Instant::now();
        if !force
            && now.duration_since(self.last_emit) < Duration::from_millis(100)
            && percent == self.last_percent
        {
            return;
        }
        self.last_emit = now;
        self.last_percent = percent;
        let value = json!({
            "event": "progress",
            "operation": operation,
            "phase": phase,
            "doneCount": done_count,
            "totalCount": total_count.map(|value| value as i64).unwrap_or(-1),
            "bytesDone": bytes_done.map(|value| value as i64).unwrap_or(-1),
            "bytesTotal": bytes_total.map(|value| value as i64).unwrap_or(-1),
            "progress": progress.clamp(0.0, 1.0),
            "percent": percent,
            "currentPath": current_path.map(|value| value.to_string_lossy()).unwrap_or_default(),
            "currentName": current_name.unwrap_or_default(),
            "statusText": status_text,
        });
        println!("{value}");
    }
}

struct Stage {
    path: PathBuf,
    active: bool,
}

impl Stage {
    fn new(parent: &Path, prefix: &str) -> Result<Self, ArchiveError> {
        fs::create_dir_all(parent).map_err(|error| {
            ArchiveError::ProviderFailed(format!("create staging parent: {error}"))
        })?;
        let serial = STAGE_COUNTER.fetch_add(1, Ordering::Relaxed);
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_nanos();
        for attempt in 0..1000 {
            let path = parent.join(format!(
                ".astrea-{prefix}-{}-{serial}-{timestamp}-{attempt}",
                std::process::id()
            ));
            match fs::create_dir(&path) {
                Ok(()) => {
                    #[cfg(unix)]
                    {
                        use std::os::unix::fs::PermissionsExt;
                        fs::set_permissions(&path, fs::Permissions::from_mode(0o700)).map_err(
                            |error| {
                                ArchiveError::ProviderFailed(format!(
                                    "secure staging directory: {error}"
                                ))
                            },
                        )?;
                    }
                    return Ok(Self { path, active: true });
                }
                Err(error) if error.kind() == io::ErrorKind::AlreadyExists => continue,
                Err(error) => {
                    return Err(ArchiveError::ProviderFailed(format!(
                        "create staging directory: {error}"
                    )));
                }
            }
        }
        Err(ArchiveError::ProviderFailed(
            "could not allocate unique staging directory".into(),
        ))
    }

    fn payload(&self, extension: &str) -> PathBuf {
        self.path.join(format!("payload.{extension}"))
    }

    fn finish(&mut self) {
        let _ = fs::remove_dir_all(&self.path);
        self.active = false;
    }
}

impl Drop for Stage {
    fn drop(&mut self) {
        if self.active {
            let _ = fs::remove_dir_all(&self.path);
        }
    }
}

static STAGE_COUNTER: AtomicU64 = AtomicU64::new(0);

struct SourceInfo {
    path: PathBuf,
    basename: OsString,
}

fn scan_sources(
    paths: &[PathBuf],
    cancellation: &Cancellation,
) -> Result<(Vec<SourceInfo>, usize, u64), ArchiveError> {
    if paths.is_empty() {
        return Err(ArchiveError::InvalidSource(
            "no archive sources were selected".into(),
        ));
    }
    let mut sources = Vec::with_capacity(paths.len());
    let mut names = std::collections::HashSet::new();
    let mut total_count: usize = 0;
    let mut total_bytes: u64 = 0;
    for path in paths {
        if cancellation.is_cancelled() {
            return Err(ArchiveError::Cancelled);
        }
        let metadata = fs::symlink_metadata(path).map_err(|error| {
            ArchiveError::InvalidSource(format!("source is not readable: {error}"))
        })?;
        if metadata.file_type().is_symlink() {
            return Err(ArchiveError::InvalidSource(format!(
                "symbolic-link sources are not supported: {}",
                path.display()
            )));
        }
        if !metadata.is_file() && !metadata.is_dir() {
            return Err(ArchiveError::InvalidSource(format!(
                "source is not a regular file or directory: {}",
                path.display()
            )));
        }
        let basename = path.file_name().ok_or_else(|| {
            ArchiveError::InvalidSource(format!("source has no basename: {}", path.display()))
        })?;
        if !names.insert(basename.to_os_string()) {
            return Err(ArchiveError::InvalidSource(format!(
                "duplicate archive root basename: {}",
                basename.to_string_lossy()
            )));
        }
        let (item_count, bytes) = scan_tree(path, cancellation)?;
        total_count += item_count;
        total_bytes = total_bytes.saturating_add(bytes);
        sources.push(SourceInfo {
            path: path.clone(),
            basename: basename.to_os_string(),
        });
    }
    Ok((sources, total_count, total_bytes))
}

fn scan_tree(path: &Path, cancellation: &Cancellation) -> Result<(usize, u64), ArchiveError> {
    if cancellation.is_cancelled() {
        return Err(ArchiveError::Cancelled);
    }
    let metadata = fs::symlink_metadata(path).map_err(|error| {
        ArchiveError::InvalidSource(format!("source changed during scan: {error}"))
    })?;
    if metadata.file_type().is_symlink() {
        return Err(ArchiveError::InvalidSource(format!(
            "symbolic-link members are not supported: {}",
            path.display()
        )));
    }
    if metadata.is_file() {
        return Ok((1, metadata.len()));
    }
    if !metadata.is_dir() {
        return Err(ArchiveError::InvalidSource(format!(
            "unsupported source member: {}",
            path.display()
        )));
    }
    let mut count = 1;
    let mut bytes: u64 = 0;
    let mut entries: Vec<PathBuf> = fs::read_dir(path)
        .map_err(|error| ArchiveError::InvalidSource(format!("read source directory: {error}")))?
        .map(|entry| {
            entry
                .map(|entry| entry.path())
                .map_err(|error| ArchiveError::InvalidSource(format!("read source entry: {error}")))
        })
        .collect::<Result<_, _>>()?;
    entries.sort();
    for entry in entries {
        let (child_count, child_bytes) = scan_tree(&entry, cancellation)?;
        count += child_count;
        bytes = bytes.saturating_add(child_bytes);
    }
    Ok((count, bytes))
}

fn canonical_archive_path(path: &Path, extension: &str) -> Result<PathBuf, ArchiveError> {
    let parent = path.parent().unwrap_or_else(|| Path::new("."));
    let name = path
        .file_name()
        .and_then(|value| value.to_str())
        .filter(|value| !value.is_empty())
        .ok_or_else(|| ArchiveError::InvalidRequest("archive output has no filename".into()))?;
    let known_extensions = [
        ".tar.zst", ".tar.xz", ".tar.gz", ".tar.bz2", ".tzst", ".txz", ".tgz", ".tbz2", ".7z",
        ".zip", ".tar", ".rar",
    ];
    let base = known_extensions
        .iter()
        .find_map(|suffix| {
            name.strip_suffix(suffix)
                .filter(|value| !value.is_empty())
                .map(str::to_string)
        })
        .unwrap_or_else(|| name.to_string());
    Ok(parent.join(format!("{base}.{extension}")))
}

fn choose_archive_target(path: &Path, policy: &str) -> Result<PathBuf, ArchiveError> {
    if !path.exists() || policy == "overwrite" {
        return Ok(path.to_path_buf());
    }
    if policy == "prompt" {
        return Err(ArchiveError::DestinationConflict(path.to_path_buf()));
    }
    if policy != "keep-both" && policy != "rename" {
        return Err(ArchiveError::InvalidRequest(format!(
            "unsupported conflict policy: {policy}"
        )));
    }
    let stem = path
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("Archive");
    let extension = path
        .extension()
        .and_then(|value| value.to_str())
        .unwrap_or("");
    for index in 2..10000 {
        let filename = if extension.is_empty() {
            format!("{stem} ({index})")
        } else {
            format!("{stem} ({index}).{extension}")
        };
        let candidate = path.with_file_name(filename);
        if !candidate.exists() {
            return Ok(candidate);
        }
    }
    Err(ArchiveError::ProviderFailed(
        "could not choose a unique archive destination".into(),
    ))
}

fn create_arguments(
    plan: &CreatePlan,
    profile: CompressionProfile,
    payload: &Path,
    sources: &[SourceInfo],
) -> Vec<OsString> {
    let mut args = Vec::new();
    match plan.provider {
        ProviderKind::Bsdtar => {
            args.extend([OsString::from("-cvf"), payload.as_os_str().to_os_string()]);
            match plan.format {
                FormatId::Zip => args.push(OsString::from("--format=zip")),
                FormatId::Tar => args.push(OsString::from("--format=ustar")),
                FormatId::TarGz => {
                    args.push(OsString::from("--format=ustar"));
                    args.push(OsString::from("--gzip"));
                    args.push(OsString::from(format!(
                        "--options=gzip:compression-level={}",
                        profile_level(profile)
                    )));
                }
                FormatId::TarXz => {
                    args.push(OsString::from("--format=ustar"));
                    args.push(OsString::from("--xz"));
                    args.push(OsString::from(format!(
                        "--options=xz:compression-level={}",
                        xz_profile_level(profile)
                    )));
                }
                FormatId::TarZst => {
                    args.push(OsString::from("--format=ustar"));
                    args.push(OsString::from("--zstd"));
                    args.push(OsString::from(format!(
                        "--options=zstd:compression-level={}",
                        zstd_profile_level(profile)
                    )));
                }
                FormatId::SevenZip => {}
            }
            for source in sources {
                args.push(OsString::from("-C"));
                args.push(
                    source
                        .path
                        .parent()
                        .unwrap_or_else(|| Path::new("."))
                        .as_os_str()
                        .to_os_string(),
                );
                args.push(source.basename.clone());
            }
        }
        ProviderKind::SevenZip => {
            args.extend([
                OsString::from("a"),
                OsString::from("-bd"),
                OsString::from("-bb1"),
                OsString::from(format!("-mx={}", seven_zip_profile_level(profile))),
                OsString::from(match plan.format {
                    FormatId::Zip => "-tzip",
                    FormatId::SevenZip => "-t7z",
                    _ => "-t7z",
                }),
                payload.as_os_str().to_os_string(),
            ]);
            for source in sources {
                args.push(source.basename.clone());
            }
        }
        ProviderKind::Rar => {}
    }
    args
}

fn profile_level(profile: CompressionProfile) -> u8 {
    match profile {
        CompressionProfile::Fast => 1,
        CompressionProfile::Balanced => 6,
        CompressionProfile::Maximum => 9,
    }
}

fn xz_profile_level(profile: CompressionProfile) -> u8 {
    match profile {
        CompressionProfile::Fast => 1,
        CompressionProfile::Balanced => 6,
        CompressionProfile::Maximum => 8,
    }
}

fn zstd_profile_level(profile: CompressionProfile) -> u8 {
    match profile {
        CompressionProfile::Fast => 1,
        CompressionProfile::Balanced => 6,
        CompressionProfile::Maximum => 12,
    }
}

fn seven_zip_profile_level(profile: CompressionProfile) -> u8 {
    match profile {
        CompressionProfile::Fast => 1,
        CompressionProfile::Balanced => 5,
        CompressionProfile::Maximum => 9,
    }
}

fn materialize_sources(
    root: &Path,
    sources: &[SourceInfo],
    cancellation: &Cancellation,
) -> Result<(), ArchiveError> {
    fs::create_dir_all(root)
        .map_err(|error| ArchiveError::ProviderFailed(format!("create 7z input root: {error}")))?;
    for source in sources {
        if cancellation.is_cancelled() {
            return Err(ArchiveError::Cancelled);
        }
        copy_source_without_links(&source.path, &root.join(&source.basename), cancellation)?;
    }
    Ok(())
}

fn copy_source_without_links(
    source: &Path,
    destination: &Path,
    cancellation: &Cancellation,
) -> Result<(), ArchiveError> {
    let metadata = fs::symlink_metadata(source)
        .map_err(|error| ArchiveError::InvalidSource(format!("copy source: {error}")))?;
    if metadata.file_type().is_symlink() {
        return Err(ArchiveError::InvalidSource(format!(
            "symbolic-link members are not supported: {}",
            source.display()
        )));
    }
    if metadata.is_file() {
        fs::copy(source, destination)
            .map_err(|error| ArchiveError::ProviderFailed(format!("stage source: {error}")))?;
        return Ok(());
    }
    fs::create_dir_all(destination).map_err(|error| {
        ArchiveError::ProviderFailed(format!("stage source directory: {error}"))
    })?;
    let mut entries: Vec<PathBuf> = fs::read_dir(source)
        .map_err(|error| ArchiveError::InvalidSource(format!("read source: {error}")))?
        .map(|entry| {
            entry
                .map(|entry| entry.path())
                .map_err(|error| ArchiveError::InvalidSource(format!("read source entry: {error}")))
        })
        .collect::<Result<_, _>>()?;
    entries.sort();
    for entry in entries {
        if cancellation.is_cancelled() {
            return Err(ArchiveError::Cancelled);
        }
        let name = entry
            .file_name()
            .ok_or_else(|| ArchiveError::InvalidSource("source member has no basename".into()))?;
        copy_source_without_links(&entry, &destination.join(name), cancellation)?;
    }
    Ok(())
}

fn publish_file(staged: &Path, target: &Path, policy: &str) -> Result<(), ArchiveError> {
    if target.exists() {
        if policy == "overwrite" {
            fs::rename(staged, target).map_err(|error| {
                ArchiveError::ProviderFailed(format!("publish archive: {error}"))
            })?;
            return Ok(());
        }
        return Err(ArchiveError::DestinationConflict(target.to_path_buf()));
    }
    fs::rename(staged, target)
        .map_err(|error| ArchiveError::ProviderFailed(format!("publish archive: {error}")))
}

#[derive(Debug)]
enum ProviderOutcome {
    Success,
    Cancelled,
    Failed(String),
}

fn run_provider<F>(
    program: &str,
    cwd: Option<&Path>,
    args: &[OsString],
    cancellation: &Cancellation,
    mut on_line: F,
) -> Result<ProviderOutcome, ArchiveError>
where
    F: FnMut(&str),
{
    let mut command = Command::new(program);
    command
        .args(args)
        .env("LC_ALL", "C")
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    if let Some(cwd) = cwd {
        command.current_dir(cwd);
    }
    #[cfg(unix)]
    unsafe {
        use std::os::unix::process::CommandExt;
        command.pre_exec(|| {
            if libc::setsid() == -1 {
                return Err(io::Error::last_os_error());
            }
            Ok(())
        });
    }
    let mut child = command
        .spawn()
        .map_err(|error| ArchiveError::ProviderFailed(format!("start {program}: {error}")))?;
    let stdout = child
        .stdout
        .take()
        .ok_or_else(|| ArchiveError::ProviderFailed("provider stdout unavailable".into()))?;
    let stderr = child
        .stderr
        .take()
        .ok_or_else(|| ArchiveError::ProviderFailed("provider stderr unavailable".into()))?;
    let (line_sender, line_receiver) = sync_channel(64);
    let stdout_thread = thread::spawn(move || read_provider_lines(stdout, line_sender));
    let (stderr_sender, stderr_receiver) = sync_channel(64);
    let stderr_thread = thread::spawn(move || read_provider_lines(stderr, stderr_sender));
    let mut cancelled = false;
    loop {
        while let Ok(line) = line_receiver.try_recv() {
            if !line.is_empty() {
                on_line(&line);
            }
        }
        while let Ok(line) = stderr_receiver.try_recv() {
            if !line.is_empty() {
                on_line(&line);
            }
        }
        if cancellation.is_cancelled() {
            cancelled = true;
            kill_provider(&mut child);
            break;
        }
        match child.try_wait() {
            Ok(Some(_)) => break,
            Ok(None) => thread::sleep(Duration::from_millis(20)),
            Err(error) => {
                kill_provider(&mut child);
                return Err(ArchiveError::ProviderFailed(format!(
                    "wait for provider: {error}"
                )));
            }
        }
    }
    let status = child
        .wait()
        .map_err(|error| ArchiveError::ProviderFailed(format!("reap provider: {error}")))?;
    let stdout_result = stdout_thread
        .join()
        .map_err(|_| ArchiveError::ProviderFailed("provider stdout reader panicked".into()))?
        .map_err(|error| ArchiveError::ProviderFailed(format!("read provider stdout: {error}")))?;
    let stderr_result = stderr_thread
        .join()
        .map_err(|_| ArchiveError::ProviderFailed("provider stderr reader panicked".into()))?
        .map_err(|error| ArchiveError::ProviderFailed(format!("read provider stderr: {error}")))?;
    while let Ok(line) = line_receiver.try_recv() {
        if !line.is_empty() {
            on_line(&line);
        }
    }
    while let Ok(line) = stderr_receiver.try_recv() {
        if !line.is_empty() {
            on_line(&line);
        }
    }
    if cancelled {
        return Ok(ProviderOutcome::Cancelled);
    }
    if status.success() {
        return Ok(ProviderOutcome::Success);
    }
    let stderr = String::from_utf8_lossy(&stderr_result.bytes)
        .trim()
        .to_string();
    let _ = stdout_result;
    Ok(ProviderOutcome::Failed(if stderr.is_empty() {
        "archive provider failed".into()
    } else {
        stderr
    }))
}

const MAX_PROVIDER_CAPTURE_BYTES: usize = 128 * 1024;
const MAX_PROVIDER_LINE_BYTES: usize = 64 * 1024;

struct BoundedCapture {
    bytes: Vec<u8>,
}

fn read_provider_lines(
    stream: impl Read,
    sender: SyncSender<String>,
) -> io::Result<BoundedCapture> {
    let mut capture = Vec::new();
    let mut reader = stream;
    let mut buffer = [0_u8; 8192];
    let mut line = Vec::with_capacity(MAX_PROVIDER_LINE_BYTES.min(8192));
    loop {
        let count = reader.read(&mut buffer)?;
        if count == 0 {
            break;
        }
        for byte in &buffer[..count] {
            if *byte == b'\n' {
                if !send_provider_line(&mut capture, &sender, &line) {
                    return Ok(BoundedCapture { bytes: capture });
                }
                line.clear();
            } else if line.len() < MAX_PROVIDER_LINE_BYTES {
                line.push(*byte);
            }
        }
    }
    if !line.is_empty() {
        let _ = send_provider_line(&mut capture, &sender, &line);
    }
    Ok(BoundedCapture { bytes: capture })
}

fn send_provider_line(capture: &mut Vec<u8>, sender: &SyncSender<String>, bytes: &[u8]) -> bool {
    let line = String::from_utf8_lossy(bytes)
        .trim_end_matches('\r')
        .to_string();
    let remaining = MAX_PROVIDER_CAPTURE_BYTES.saturating_sub(capture.len());
    capture.extend_from_slice(&bytes[..bytes.len().min(remaining)]);
    match sender.try_send(line) {
        Ok(()) | Err(TrySendError::Full(_)) => true,
        Err(TrySendError::Disconnected(_)) => false,
    }
}

fn kill_provider(child: &mut Child) {
    #[cfg(unix)]
    unsafe {
        let _ = libc::kill(-(child.id() as i32), libc::SIGKILL);
    }
    let _ = child.kill();
    let _ = child.wait();
}

fn classify_provider_failure(message: &str, password_supplied: bool) -> ArchiveError {
    let lower = message.to_ascii_lowercase();
    if lower.contains("password") || lower.contains("encrypted") || lower.contains("wrong password")
    {
        if password_supplied {
            ArchiveError::BadPassword
        } else {
            ArchiveError::PasswordRequired
        }
    } else {
        ArchiveError::ProviderFailed("archive provider failed".into())
    }
}

fn verify_archive(
    provider: ProviderKind,
    archive: &Path,
    cancellation: &Cancellation,
) -> Result<(), ArchiveError> {
    let (program, args) = match provider {
        ProviderKind::Bsdtar => (
            "bsdtar",
            vec![OsString::from("-tf"), archive.as_os_str().to_os_string()],
        ),
        ProviderKind::SevenZip => (
            "7z",
            vec![
                OsString::from("t"),
                OsString::from("-bd"),
                archive.as_os_str().to_os_string(),
            ],
        ),
        ProviderKind::Rar => return Err(ArchiveError::ProviderUnavailable),
    };
    match run_provider(program, None, &args, cancellation, |_| {})? {
        ProviderOutcome::Success => Ok(()),
        ProviderOutcome::Cancelled => Err(ArchiveError::Cancelled),
        ProviderOutcome::Failed(message) => Err(ArchiveError::ProviderFailed(message)),
    }
}

fn create_archive(
    request: &OperationRequest,
    cancellation: &Cancellation,
    emitter: &mut ProgressEmitter,
) -> Result<Value, ArchiveError> {
    let format = FormatId::from_id(&request.format).ok_or(ArchiveError::UnsupportedFormat)?;
    let profile = CompressionProfile::from_id(&request.profile).ok_or(
        ArchiveError::InvalidRequest("unknown compression profile".into()),
    )?;
    let providers = discover_providers();
    let plan = plan_create(format, profile, providers)?;
    let (sources, total_count, total_bytes) = scan_sources(&request.sources, cancellation)?;
    let output = canonical_archive_path(&request.archive_path, plan.canonical_extension)?;
    let parent = output.parent().unwrap_or_else(|| Path::new("."));
    let mut stage = Stage::new(parent, "archive")?;
    let payload = stage.payload(plan.canonical_extension);
    emitter.emit(
        "create",
        "scanning",
        0,
        Some(total_count),
        Some(0),
        Some(total_bytes),
        0.0,
        None,
        None,
        "Scanning sources",
        true,
    );
    emitter.emit(
        "create",
        "scanning",
        total_count,
        Some(total_count),
        Some(total_bytes),
        Some(total_bytes),
        0.25,
        None,
        None,
        "Sources scanned",
        true,
    );
    let (cwd, args, materialized_root) = if plan.provider == ProviderKind::SevenZip {
        let input_root = stage.path.join("input");
        materialize_sources(&input_root, &sources, cancellation)?;
        (
            Some(input_root),
            create_arguments(&plan, profile, &payload, &sources),
            true,
        )
    } else {
        (
            None,
            create_arguments(&plan, profile, &payload, &sources),
            false,
        )
    };
    let mut done = 0usize;
    let outcome = run_provider(
        match plan.provider {
            ProviderKind::Bsdtar => "bsdtar",
            ProviderKind::SevenZip => "7z",
            ProviderKind::Rar => "rar",
        },
        cwd.as_deref(),
        &args,
        cancellation,
        |line| {
            let line = match plan.provider {
                ProviderKind::SevenZip => line.strip_prefix("+ ").map(str::trim).unwrap_or(""),
                ProviderKind::Bsdtar => line.strip_prefix("a ").map(str::trim).unwrap_or(""),
                ProviderKind::Rar => "",
            };
            if line.is_empty() {
                return;
            }
            done = (done + 1).min(total_count);
            let current_name = Path::new(line).file_name().and_then(|value| value.to_str());
            emitter.emit(
                "create",
                "compressing",
                done,
                Some(total_count),
                None,
                None,
                0.25 + 0.65 * (done as f64 / total_count.max(1) as f64),
                Some(Path::new(line)),
                current_name,
                "Compressing archive",
                false,
            );
        },
    )?;
    let _ = materialized_root;
    match outcome {
        ProviderOutcome::Success => {}
        ProviderOutcome::Cancelled => return Err(ArchiveError::Cancelled),
        ProviderOutcome::Failed(message) => return Err(ArchiveError::ProviderFailed(message)),
    }
    verify_archive(plan.provider, &payload, cancellation)?;
    if cancellation.is_cancelled() {
        return Err(ArchiveError::Cancelled);
    }
    emitter.emit(
        "create",
        "publishing",
        total_count,
        Some(total_count),
        Some(total_bytes),
        Some(total_bytes),
        0.95,
        None,
        None,
        "Publishing archive",
        true,
    );
    let target = choose_archive_target(&output, &request.conflict_policy)?;
    if cancellation.is_cancelled() {
        return Err(ArchiveError::Cancelled);
    }
    publish_file(&payload, &target, &request.conflict_policy)?;
    stage.finish();
    emitter.emit(
        "create",
        "publishing",
        total_count,
        Some(total_count),
        Some(total_bytes),
        Some(total_bytes),
        1.0,
        Some(&target),
        target.file_name().and_then(|value| value.to_str()),
        "Archive ready",
        true,
    );
    Ok(json!({
        "operation": "create",
        "state": "success",
        "destination": target,
        "phase": "publishing",
        "doneCount": total_count,
        "totalCount": total_count,
        "bytesDone": total_bytes,
        "bytesTotal": total_bytes,
        "progress": 1.0,
        "percent": 100,
    }))
}

fn extraction_provider(archive: &Path, password: &str) -> Result<ProviderKind, ArchiveError> {
    let providers = discover_providers();
    let name = archive.to_string_lossy().to_ascii_lowercase();
    let seven_zip_archive = name.ends_with(".7z") || name.ends_with(".rar");
    if seven_zip_archive {
        return if providers.seven_zip {
            Ok(ProviderKind::SevenZip)
        } else {
            Err(ArchiveError::ProviderUnavailable)
        };
    }
    if !password.is_empty() && providers.seven_zip {
        return Ok(ProviderKind::SevenZip);
    }
    if providers.bsdtar {
        return Ok(ProviderKind::Bsdtar);
    }
    if providers.seven_zip {
        return Ok(ProviderKind::SevenZip);
    }
    Err(ArchiveError::ProviderUnavailable)
}

fn inspect_archive(
    archive: &Path,
    provider: ProviderKind,
    password: &str,
    cancellation: &Cancellation,
    emitter: &mut ProgressEmitter,
) -> Result<Vec<String>, ArchiveError> {
    let mut members = Vec::new();
    match provider {
        ProviderKind::Bsdtar => {
            let args = vec![OsString::from("-tf"), archive.as_os_str().to_os_string()];
            let outcome = run_provider("bsdtar", None, &args, cancellation, |line| {
                if !line.trim().is_empty() {
                    members.push(line.to_string());
                    emitter.emit(
                        "extract",
                        "inspecting",
                        members.len(),
                        None,
                        None,
                        None,
                        0.0,
                        None,
                        Some(line),
                        "Inspecting archive",
                        false,
                    );
                }
            })?;
            match outcome {
                ProviderOutcome::Success => {}
                ProviderOutcome::Cancelled => return Err(ArchiveError::Cancelled),
                ProviderOutcome::Failed(message) => {
                    return Err(classify_provider_failure(&message, !password.is_empty()));
                }
            }
            for member in &members {
                validate_member(member).map_err(ArchiveError::UnsafeMember)?;
            }
            let verbose_args = vec![OsString::from("-tvf"), archive.as_os_str().to_os_string()];
            let mut links = false;
            let outcome = run_provider("bsdtar", None, &verbose_args, cancellation, |line| {
                if line.starts_with('l') || line.starts_with('h') || line.contains(" -> ") {
                    links = true;
                }
            })?;
            if matches!(outcome, ProviderOutcome::Cancelled) {
                return Err(ArchiveError::Cancelled);
            }
            if links {
                return Err(ArchiveError::UnsafeMember(
                    "archives containing symbolic or hard links are not supported".into(),
                ));
            }
            if let ProviderOutcome::Failed(message) = outcome {
                return Err(classify_provider_failure(&message, !password.is_empty()));
            }
        }
        ProviderKind::SevenZip => {
            let mut args = vec![
                OsString::from("l"),
                OsString::from("-slt"),
                OsString::from("-bd"),
                OsString::from(format!("-p{password}")),
            ];
            args.push(archive.as_os_str().to_os_string());
            let mut encrypted = false;
            let outcome = run_provider("7z", None, &args, cancellation, |line| {
                if let Some(path) = line.strip_prefix("Path = ") {
                    if !path.is_empty() && path != archive.to_string_lossy() {
                        members.push(path.to_string());
                    }
                }
                if line.starts_with("Encrypted = +") {
                    encrypted = true;
                }
                if line.starts_with("Attributes = ") && line.contains('L') {
                    encrypted = true;
                }
            })?;
            match outcome {
                ProviderOutcome::Success if encrypted && password.is_empty() => {
                    return Err(ArchiveError::PasswordRequired);
                }
                ProviderOutcome::Success => {}
                ProviderOutcome::Cancelled => return Err(ArchiveError::Cancelled),
                ProviderOutcome::Failed(message) => {
                    return Err(classify_provider_failure(&message, !password.is_empty()));
                }
            }
            for member in &members {
                validate_member(member).map_err(ArchiveError::UnsafeMember)?;
            }
        }
        ProviderKind::Rar => return Err(ArchiveError::ProviderUnavailable),
    }
    Ok(members)
}

fn extract_archive(
    request: &OperationRequest,
    cancellation: &Cancellation,
    emitter: &mut ProgressEmitter,
) -> Result<Value, ArchiveError> {
    if request.archive_path.as_os_str().is_empty() || !request.archive_path.is_file() {
        return Err(ArchiveError::InvalidSource("archive was not found".into()));
    }
    let provider = extraction_provider(&request.archive_path, &request.password)?;
    emitter.emit(
        "extract",
        "inspecting",
        0,
        None,
        None,
        None,
        0.0,
        Some(&request.archive_path),
        request
            .archive_path
            .file_name()
            .and_then(|value| value.to_str()),
        "Inspecting archive",
        true,
    );
    let members = inspect_archive(
        &request.archive_path,
        provider,
        &request.password,
        cancellation,
        emitter,
    )?;
    let destination = request.destination.clone();
    let parent = destination.parent().unwrap_or_else(|| Path::new("."));
    fs::create_dir_all(parent).map_err(|error| {
        ArchiveError::ProviderFailed(format!("create destination parent: {error}"))
    })?;
    let target = choose_target_for_extraction(&destination, &request.conflict_policy)?;
    let mut stage = Stage::new(parent, "extract")?;
    let mut args = Vec::new();
    match provider {
        ProviderKind::Bsdtar => {
            args.extend([
                OsString::from("-xvf"),
                request.archive_path.as_os_str().to_os_string(),
                OsString::from("-C"),
                stage.path.clone().into_os_string(),
            ]);
        }
        ProviderKind::SevenZip => {
            args.extend([
                OsString::from("x"),
                OsString::from("-y"),
                OsString::from("-bb1"),
            ]);
            // 7z has no supported stdin password mode in this integration; the
            // provider limitation is isolated here and the password is never logged.
            args.push(OsString::from(format!("-p{}", request.password)));
            args.push(request.archive_path.as_os_str().to_os_string());
            args.push(OsString::from(format!("-o{}", stage.path.display())));
        }
        ProviderKind::Rar => return Err(ArchiveError::ProviderUnavailable),
    }
    let mut done = 0usize;
    emitter.emit(
        "extract",
        "extracting",
        0,
        Some(members.len()),
        None,
        None,
        0.05,
        None,
        None,
        "Extracting archive",
        true,
    );
    let outcome = run_provider(
        match provider {
            ProviderKind::Bsdtar => "bsdtar",
            ProviderKind::SevenZip => "7z",
            ProviderKind::Rar => "rar",
        },
        None,
        &args,
        cancellation,
        |line| {
            let line = match provider {
                ProviderKind::SevenZip => line.strip_prefix("- ").map(str::trim).unwrap_or(""),
                ProviderKind::Bsdtar => line.strip_prefix("x ").map(str::trim).unwrap_or(""),
                ProviderKind::Rar => "",
            };
            if line.is_empty() {
                return;
            }
            done = (done + 1).min(members.len());
            emitter.emit(
                "extract",
                "extracting",
                done,
                Some(members.len()),
                None,
                None,
                0.05 + 0.85 * (done as f64 / members.len().max(1) as f64),
                Some(Path::new(line)),
                Some(line),
                "Extracting archive",
                false,
            );
        },
    )?;
    match outcome {
        ProviderOutcome::Success => {}
        ProviderOutcome::Cancelled => return Err(ArchiveError::Cancelled),
        ProviderOutcome::Failed(message) => {
            return Err(classify_provider_failure(
                &message,
                !request.password.is_empty(),
            ));
        }
    }
    validate_extracted_tree(&stage.path)?;
    if cancellation.is_cancelled() {
        return Err(ArchiveError::Cancelled);
    }
    emitter.emit(
        "extract",
        "publishing",
        members.len(),
        Some(members.len()),
        None,
        None,
        0.95,
        None,
        None,
        "Publishing extracted files",
        true,
    );
    if cancellation.is_cancelled() {
        return Err(ArchiveError::Cancelled);
    }
    if target.exists() {
        if request.conflict_policy == "overwrite" {
            remove_path(&target).map_err(ArchiveError::ProviderFailed)?;
        } else {
            return Err(ArchiveError::DestinationConflict(target));
        }
    }
    fs::rename(&stage.path, &target).map_err(|error| {
        ArchiveError::ProviderFailed(format!("publish extracted archive: {error}"))
    })?;
    stage.finish();
    emitter.emit(
        "extract",
        "publishing",
        members.len(),
        Some(members.len()),
        None,
        None,
        1.0,
        Some(&target),
        target.file_name().and_then(|value| value.to_str()),
        "Extraction ready",
        true,
    );
    Ok(json!({
        "operation": "extract",
        "state": "success",
        "destination": target,
        "phase": "publishing",
        "doneCount": members.len(),
        "totalCount": members.len(),
        "bytesDone": -1,
        "bytesTotal": -1,
        "progress": 1.0,
        "percent": 100,
    }))
}

fn choose_target_for_extraction(destination: &Path, policy: &str) -> Result<PathBuf, ArchiveError> {
    if !destination.exists() || policy == "overwrite" {
        return Ok(destination.to_path_buf());
    }
    if policy == "prompt" {
        return Err(ArchiveError::DestinationConflict(destination.to_path_buf()));
    }
    choose_target(destination, policy).map_err(|message| ArchiveError::ProviderFailed(message))
}

fn validate_extracted_tree(root: &Path) -> Result<(), ArchiveError> {
    let metadata = fs::symlink_metadata(root).map_err(|error| {
        ArchiveError::ProviderFailed(format!("inspect staged extraction: {error}"))
    })?;
    if metadata.file_type().is_symlink() {
        return Err(ArchiveError::UnsafeMember(
            "extraction created a symbolic link".into(),
        ));
    }
    if !metadata.is_dir() {
        return Ok(());
    }
    let entries = fs::read_dir(root).map_err(|error| {
        ArchiveError::ProviderFailed(format!("read staged extraction: {error}"))
    })?;
    for entry in entries {
        let path = entry
            .map_err(|error| ArchiveError::ProviderFailed(format!("read staged member: {error}")))?
            .path();
        validate_extracted_tree(&path)?;
    }
    Ok(())
}

pub fn run_operation(args: &[String]) -> Result<(), String> {
    let encoded = args
        .iter()
        .position(|value| value == "--json")
        .and_then(|index| args.get(index + 1))
        .ok_or_else(|| "archive-operation requires --json <request>".to_string())?;
    let request: OperationRequest = serde_json::from_str(encoded)
        .map_err(|error| format!("invalid archive request: {error}"))?;
    let cancellation = Cancellation::from_environment();
    let mut emitter = ProgressEmitter::new();
    let result = match request.kind.as_str() {
        "capabilities" => Ok(json!({
            "operation": "capabilities",
            "state": "success",
            "capabilities": discover_capabilities().iter().map(capability_json).collect::<Vec<_>>(),
            "progress": 1.0,
            "percent": 100,
        })),
        "create" => create_archive(&request, &cancellation, &mut emitter),
        "extract" => extract_archive(&request, &cancellation, &mut emitter),
        _ => Err(ArchiveError::InvalidRequest(
            "unknown archive operation".into(),
        )),
    };
    let value = match result {
        Ok(mut value) => {
            value["event"] = Value::String("result".into());
            value
        }
        Err(error) => json!({
            "event": "result",
            "operation": request.kind,
            "state": error.code(),
            "errorCode": error.code(),
            "errorMessage": error.message(),
            "destination": request.destination,
            "progress": 0.0,
            "percent": 0,
        }),
    };
    println!("{value}");
    Ok(())
}

fn capability_json(capability: &Capability) -> Value {
    json!({
        "id": capability.id,
        "label": capability.label,
        "extension": capability.extension,
        "createSupported": capability.create_supported,
        "extractSupported": capability.extract_supported,
        "profiles": capability.profiles.iter().map(|profile| profile.as_id()).collect::<Vec<_>>(),
        "passwordSupported": capability.password_supported,
        "provider": capability.provider.as_id(),
    })
}

fn validate_member(value: &str) -> Result<(), String> {
    let normalized = value.replace('\\', "/");
    let path = Path::new(&normalized);
    let windows_absolute = normalized.len() >= 3
        && normalized.as_bytes()[1] == b':'
        && normalized.as_bytes()[2] == b'/';
    if value.is_empty() || path.is_absolute() || windows_absolute {
        return Err(format!("unsafe archive member: {value:?}"));
    }
    for component in path.components() {
        if matches!(component, Component::ParentDir) {
            return Err(format!("archive member escapes destination: {value}"));
        }
    }
    Ok(())
}

fn choose_target(destination: &Path, policy: &str) -> Result<PathBuf, String> {
    if !destination.exists() || policy == "overwrite" {
        return Ok(destination.to_path_buf());
    }
    if policy != "keep-both" && policy != "rename" {
        return Err(format!(
            "extraction target already exists: {}",
            destination.display()
        ));
    }
    for index in 2..10000 {
        let name = destination
            .file_name()
            .and_then(|v| v.to_str())
            .unwrap_or("extracted");
        let candidate = destination.with_file_name(format!("{name} ({index})"));
        if !candidate.exists() {
            return Ok(candidate);
        }
    }
    Err("could not choose a unique extraction target".into())
}

fn remove_path(path: &Path) -> Result<(), String> {
    if path.is_dir() && !path.is_symlink() {
        fs::remove_dir_all(path).map_err(|error| format!("remove existing target: {error}"))
    } else {
        fs::remove_file(path).map_err(|error| format!("remove existing target: {error}"))
    }
}

#[cfg(test)]
mod tests {
    use super::{
        ArchiveError, Cancellation, CompressionProfile, FormatId, OperationRequest,
        ProgressEmitter, ProviderAvailability, ProviderKind, SourceInfo, Stage,
        canonical_archive_path, capabilities_for, create_archive, create_arguments,
        extract_archive, plan_create, run_provider,
    };
    use serde_json::Value;
    use std::fs;
    use std::path::{Path, PathBuf};
    use std::sync::atomic::Ordering;

    fn test_root(name: &str) -> PathBuf {
        let root = std::env::temp_dir().join(format!(
            "astrea-archive-{name}-{}-{}",
            std::process::id(),
            super::STAGE_COUNTER.fetch_add(1, Ordering::Relaxed)
        ));
        fs::create_dir_all(&root).unwrap();
        root
    }

    fn mixed_fixture(root: &Path) {
        fs::create_dir_all(root.join("photos")).unwrap();
        fs::create_dir_all(root.join("notes")).unwrap();
        fs::write(root.join("report.txt"), b"report").unwrap();
        fs::write(root.join("photos/a.jpg"), b"photo").unwrap();
        fs::write(root.join("notes/note.txt"), b"note").unwrap();
    }

    #[test]
    fn rejects_absolute_and_parent_archive_members() {
        assert!(super::validate_member("/tmp/escape").is_err());
        assert!(super::validate_member("../escape").is_err());
        assert!(super::validate_member("nested/../../escape").is_err());
        assert!(super::validate_member(r"nested\..\escape").is_err());
        assert!(super::validate_member(r"C:\escape").is_err());
    }

    #[test]
    fn accepts_normal_archive_members() {
        assert!(super::validate_member("folder/file.txt").is_ok());
    }

    #[test]
    fn create_format_selects_an_explicit_provider_plan() {
        let providers = ProviderAvailability {
            bsdtar: true,
            seven_zip: true,
            rar: false,
            bsdtar_zstd: true,
        };
        let cases = [
            (FormatId::Zip, ProviderKind::Bsdtar, "zip"),
            (FormatId::SevenZip, ProviderKind::SevenZip, "7z"),
            (FormatId::Tar, ProviderKind::Bsdtar, "tar"),
            (FormatId::TarGz, ProviderKind::Bsdtar, "tar.gz"),
            (FormatId::TarXz, ProviderKind::Bsdtar, "tar.xz"),
            (FormatId::TarZst, ProviderKind::Bsdtar, "tar.zst"),
        ];

        for (format, provider, extension) in cases {
            let plan = plan_create(format, CompressionProfile::Balanced, providers).unwrap();
            assert_eq!(plan.format, format);
            assert_eq!(plan.provider, provider);
            assert_eq!(plan.canonical_extension, extension);
        }
    }

    #[test]
    fn unavailable_provider_is_not_hidden_by_format_name() {
        let providers = ProviderAvailability {
            bsdtar: true,
            seven_zip: false,
            rar: false,
            bsdtar_zstd: true,
        };

        let error = plan_create(FormatId::SevenZip, CompressionProfile::Fast, providers)
            .expect_err("7z creation must require the 7z provider");
        assert_eq!(error.code(), "provider-unavailable");
    }

    #[test]
    fn seven_zip_provider_keeps_zip_and_7z_format_authority_distinct() {
        let source = SourceInfo {
            path: PathBuf::from("/tmp/report.txt"),
            basename: "report.txt".into(),
        };
        let zip_plan = plan_create(
            FormatId::Zip,
            CompressionProfile::Balanced,
            ProviderAvailability {
                bsdtar: false,
                seven_zip: true,
                rar: false,
                bsdtar_zstd: false,
            },
        )
        .unwrap();
        let zip_args = create_arguments(
            &zip_plan,
            CompressionProfile::Balanced,
            Path::new("/tmp/payload.zip"),
            &[source],
        );
        assert!(zip_args.iter().any(|arg| arg == "-tzip"));
        assert!(!zip_args.iter().any(|arg| arg == "-t7z"));

        let seven_zip_plan = plan_create(
            FormatId::SevenZip,
            CompressionProfile::Balanced,
            ProviderAvailability {
                bsdtar: false,
                seven_zip: true,
                rar: false,
                bsdtar_zstd: false,
            },
        )
        .unwrap();
        let seven_zip_args = create_arguments(
            &seven_zip_plan,
            CompressionProfile::Balanced,
            Path::new("/tmp/payload.7z"),
            &[SourceInfo {
                path: PathBuf::from("/tmp/report.txt"),
                basename: "report.txt".into(),
            }],
        );
        assert!(seven_zip_args.iter().any(|arg| arg == "-t7z"));
        assert!(!seven_zip_args.iter().any(|arg| arg == "-tzip"));
    }

    #[test]
    fn capabilities_never_advertise_unavailable_create_formats() {
        let capabilities = capabilities_for(ProviderAvailability {
            bsdtar: true,
            seven_zip: false,
            rar: false,
            bsdtar_zstd: true,
        });

        assert!(
            capabilities
                .iter()
                .any(|item| item.id == "zip" && item.create_supported)
        );
        assert!(
            capabilities
                .iter()
                .any(|item| item.id == "tar.zst" && item.create_supported)
        );
        assert!(
            !capabilities
                .iter()
                .any(|item| item.id == "7z" && item.create_supported)
        );
        assert!(
            !capabilities
                .iter()
                .any(|item| item.id == "rar" && item.create_supported)
        );
    }

    #[test]
    fn staging_is_unique_private_and_cleaned_after_finish_or_drop() {
        let root = test_root("staging");
        let first = Stage::new(&root, "archive").unwrap();
        let second = Stage::new(&root, "archive").unwrap();
        assert_ne!(first.path, second.path);
        assert!(first.path.join("payload.tar.zst").file_name().is_some());
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            assert_eq!(
                fs::metadata(&first.path).unwrap().permissions().mode() & 0o777,
                0o700
            );
        }
        let first_path = first.path.clone();
        let mut second = second;
        second.finish();
        assert!(!second.path.exists());
        drop(first);
        assert!(!first_path.exists());
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn canonical_archive_name_always_uses_the_selected_extension() {
        assert_eq!(
            canonical_archive_path(Path::new("/tmp/Project.tmp-123"), "tar.zst").unwrap(),
            PathBuf::from("/tmp/Project.tmp-123.tar.zst")
        );
        assert_eq!(
            canonical_archive_path(Path::new("/tmp/Project.zip"), "tar.zst").unwrap(),
            PathBuf::from("/tmp/Project.tar.zst")
        );
    }

    #[test]
    fn duplicate_source_basenames_are_rejected_before_provider_execution() {
        let root = test_root("duplicate-basename");
        let first = root.join("one").join("same.txt");
        let second = root.join("two").join("same.txt");
        fs::create_dir_all(first.parent().unwrap()).unwrap();
        fs::create_dir_all(second.parent().unwrap()).unwrap();
        fs::write(&first, b"one").unwrap();
        fs::write(&second, b"two").unwrap();
        let request = OperationRequest {
            kind: "create".into(),
            sources: vec![first, second],
            archive_path: root.join("Archive"),
            destination: PathBuf::new(),
            format: "zip".into(),
            profile: "balanced".into(),
            password: String::new(),
            conflict_policy: "keep-both".into(),
        };
        let error = create_archive(
            &request,
            &Cancellation { marker: None },
            &mut ProgressEmitter::new(),
        )
        .expect_err("duplicate archive roots must be rejected");
        assert_eq!(error.code(), "invalid-source");
        assert!(error.message().contains("duplicate archive root basename"));
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn malformed_archive_returns_structured_provider_failure_without_destination() {
        if !super::discover_providers().bsdtar {
            return;
        }
        let root = test_root("malformed");
        let archive = root.join("broken.zip");
        fs::write(&archive, b"not an archive").unwrap();
        let destination = root.join("Extracted");
        let request = OperationRequest {
            kind: "extract".into(),
            sources: Vec::new(),
            archive_path: archive,
            destination: destination.clone(),
            format: String::new(),
            profile: "balanced".into(),
            password: String::new(),
            conflict_policy: "keep-both".into(),
        };
        let error = extract_archive(
            &request,
            &Cancellation { marker: None },
            &mut ProgressEmitter::new(),
        )
        .expect_err("malformed archive must fail inspection");
        assert_eq!(error.code(), "provider-failed");
        assert!(!destination.exists());
        assert!(!fs::read_dir(&root).unwrap().flatten().any(|entry| {
            entry
                .file_name()
                .to_string_lossy()
                .starts_with(".astrea-extract-")
        }));
        fs::remove_dir_all(root).unwrap();
    }

    #[cfg(unix)]
    #[test]
    fn cancellation_terminates_and_reaps_a_provider_process() {
        let root = test_root("provider-cancel");
        let marker = root.join("cancel.marker");
        let writer_marker = marker.clone();
        let writer = std::thread::spawn(move || {
            std::thread::sleep(std::time::Duration::from_millis(80));
            fs::write(writer_marker, b"cancel").unwrap();
        });
        let outcome = run_provider(
            "sh",
            None,
            &[
                "-c".into(),
                "while true; do echo provider-progress; done".into(),
            ],
            &Cancellation {
                marker: Some(marker),
            },
            |_| {},
        )
        .unwrap();
        writer.join().unwrap();
        assert!(matches!(outcome, super::ProviderOutcome::Cancelled));
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn real_provider_create_contains_only_selected_top_level_basenames() {
        if !super::discover_providers().bsdtar {
            return;
        }
        let root = test_root("create");
        let fixture = root.join("fixture");
        mixed_fixture(&fixture);
        let output_base = root.join("Archive");
        let request = OperationRequest {
            kind: "create".into(),
            sources: vec![
                fixture.join("report.txt"),
                fixture.join("photos"),
                fixture.join("notes"),
            ],
            archive_path: output_base,
            destination: PathBuf::new(),
            format: "zip".into(),
            profile: "balanced".into(),
            password: String::new(),
            conflict_policy: "keep-both".into(),
        };
        let result = create_archive(
            &request,
            &Cancellation { marker: None },
            &mut ProgressEmitter::new(),
        )
        .unwrap();
        assert_eq!(result["state"], Value::String("success".into()));
        let archive = PathBuf::from(result["destination"].as_str().unwrap());
        let listing = std::process::Command::new("bsdtar")
            .args(["-tf", archive.to_str().unwrap()])
            .output()
            .unwrap();
        let listing = String::from_utf8_lossy(&listing.stdout);
        assert!(listing.lines().any(|line| line == "report.txt"));
        assert!(listing.lines().any(|line| line == "photos/"));
        assert!(listing.lines().any(|line| line == "photos/a.jpg"));
        assert!(listing.lines().any(|line| line == "notes/"));
        assert!(!listing.contains(fixture.to_string_lossy().as_ref()));
        assert!(!fs::read_dir(&root).unwrap().flatten().any(|entry| {
            entry
                .file_name()
                .to_string_lossy()
                .starts_with(".astrea-archive-")
        }));
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn real_7z_provider_creates_verifies_and_extracts_mixed_sources() {
        if !super::discover_providers().seven_zip {
            return;
        }
        let root = test_root("7z");
        let fixture = root.join("fixture");
        mixed_fixture(&fixture);
        let create_request = OperationRequest {
            kind: "create".into(),
            sources: vec![
                fixture.join("report.txt"),
                fixture.join("photos"),
                fixture.join("notes"),
            ],
            archive_path: root.join("Archive"),
            destination: PathBuf::new(),
            format: "7z".into(),
            profile: "balanced".into(),
            password: String::new(),
            conflict_policy: "keep-both".into(),
        };
        let create_result = create_archive(
            &create_request,
            &Cancellation { marker: None },
            &mut ProgressEmitter::new(),
        )
        .unwrap();
        let archive = PathBuf::from(create_result["destination"].as_str().unwrap());
        let listing = std::process::Command::new("7z")
            .args(["l", "-slt", archive.to_str().unwrap()])
            .output()
            .unwrap();
        assert!(listing.status.success());
        let listing = String::from_utf8_lossy(&listing.stdout);
        assert!(listing.contains("Path = report.txt"));
        assert!(listing.contains("Path = photos/a.jpg"));
        assert!(listing.contains("Path = notes/note.txt"));
        assert!(!listing.contains(fixture.to_string_lossy().as_ref()));

        let extract_request = OperationRequest {
            kind: "extract".into(),
            sources: Vec::new(),
            archive_path: archive,
            destination: root.join("Extracted"),
            format: String::new(),
            profile: "balanced".into(),
            password: String::new(),
            conflict_policy: "keep-both".into(),
        };
        let extract_result = extract_archive(
            &extract_request,
            &Cancellation { marker: None },
            &mut ProgressEmitter::new(),
        )
        .unwrap();
        assert_eq!(extract_result["state"], Value::String("success".into()));
        assert!(root.join("Extracted/report.txt").is_file());
        assert!(root.join("Extracted/photos/a.jpg").is_file());
        assert!(root.join("Extracted/notes/note.txt").is_file());
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn real_7z_password_states_are_structured_and_retryable() {
        if !super::discover_providers().seven_zip {
            return;
        }
        let root = test_root("password");
        let source = root.join("secret.txt");
        let archive = root.join("protected.7z");
        fs::write(&source, b"secret-data").unwrap();
        let provider = std::process::Command::new("7z")
            .args([
                "a",
                "-bd",
                "-psecret",
                "-mhe=on",
                archive.to_str().unwrap(),
                source.to_str().unwrap(),
            ])
            .output()
            .unwrap();
        assert!(provider.status.success());

        let request = |password: &str| OperationRequest {
            kind: "extract".into(),
            sources: Vec::new(),
            archive_path: archive.clone(),
            destination: root.join("Extracted"),
            format: String::new(),
            profile: "balanced".into(),
            password: password.into(),
            conflict_policy: "keep-both".into(),
        };
        let required = extract_archive(
            &request(""),
            &Cancellation { marker: None },
            &mut ProgressEmitter::new(),
        )
        .expect_err("encrypted archive without a password must continue");
        assert_eq!(required, ArchiveError::PasswordRequired);

        let wrong = extract_archive(
            &request("wrong"),
            &Cancellation { marker: None },
            &mut ProgressEmitter::new(),
        )
        .expect_err("wrong password must remain retryable");
        assert_eq!(wrong, ArchiveError::BadPassword);

        let success = extract_archive(
            &request("secret"),
            &Cancellation { marker: None },
            &mut ProgressEmitter::new(),
        )
        .unwrap();
        assert_eq!(success["state"], Value::String("success".into()));
        assert!(root.join("Extracted/secret.txt").is_file());
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn real_provider_extracts_to_staging_then_publishes() {
        if !super::discover_providers().bsdtar {
            return;
        }
        let root = test_root("extract");
        let fixture = root.join("fixture");
        mixed_fixture(&fixture);
        let create_request = OperationRequest {
            kind: "create".into(),
            sources: vec![fixture.join("report.txt"), fixture.join("photos")],
            archive_path: root.join("Archive"),
            destination: PathBuf::new(),
            format: "tar.zst".into(),
            profile: "balanced".into(),
            password: String::new(),
            conflict_policy: "keep-both".into(),
        };
        let create_result = create_archive(
            &create_request,
            &Cancellation { marker: None },
            &mut ProgressEmitter::new(),
        )
        .unwrap();
        let archive = PathBuf::from(create_result["destination"].as_str().unwrap());
        let extract_request = OperationRequest {
            kind: "extract".into(),
            sources: Vec::new(),
            archive_path: archive,
            destination: root.join("Extracted"),
            format: String::new(),
            profile: "balanced".into(),
            password: String::new(),
            conflict_policy: "keep-both".into(),
        };
        let result = extract_archive(
            &extract_request,
            &Cancellation { marker: None },
            &mut ProgressEmitter::new(),
        )
        .unwrap();
        assert_eq!(result["state"], Value::String("success".into()));
        assert!(root.join("Extracted/report.txt").is_file());
        assert!(root.join("Extracted/photos/a.jpg").is_file());
        assert!(!fs::read_dir(&root).unwrap().flatten().any(|entry| {
            entry
                .file_name()
                .to_string_lossy()
                .starts_with(".astrea-extract-")
        }));
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn cancellation_cleans_staging_and_preserves_existing_destination() {
        if !super::discover_providers().bsdtar {
            return;
        }
        let root = test_root("cancel");
        let fixture = root.join("fixture");
        mixed_fixture(&fixture);
        let destination = root.join("Archive.zip");
        fs::write(&destination, b"old archive").unwrap();
        let marker = root.join("cancel.marker");
        fs::write(&marker, b"cancel").unwrap();
        let request = OperationRequest {
            kind: "create".into(),
            sources: vec![fixture.join("report.txt")],
            archive_path: root.join("Archive"),
            destination: PathBuf::new(),
            format: "zip".into(),
            profile: "balanced".into(),
            password: String::new(),
            conflict_policy: "overwrite".into(),
        };
        let result = create_archive(
            &request,
            &Cancellation {
                marker: Some(marker.clone()),
            },
            &mut ProgressEmitter::new(),
        )
        .expect_err("pre-cancelled provider must not publish");
        assert_eq!(result, ArchiveError::Cancelled);
        assert_eq!(fs::read(&destination).unwrap(), b"old archive");
        assert!(!fs::read_dir(&root).unwrap().flatten().any(|entry| {
            entry
                .file_name()
                .to_string_lossy()
                .starts_with(".astrea-archive-")
        }));
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn capability_matrix_handles_7z_only_and_no_provider() {
        let seven_zip_only = capabilities_for(ProviderAvailability {
            bsdtar: false,
            seven_zip: true,
            rar: false,
            bsdtar_zstd: false,
        });
        assert!(
            seven_zip_only
                .iter()
                .any(|item| item.id == "7z" && item.create_supported)
        );
        assert!(
            seven_zip_only
                .iter()
                .any(|item| item.id == "zip" && item.create_supported)
        );
        assert!(
            seven_zip_only
                .iter()
                .any(|item| item.id == "rar" && item.extract_supported)
        );
        assert!(!seven_zip_only.iter().any(|item| item.id == "tar.zst"));

        assert!(capabilities_for(ProviderAvailability::default()).is_empty());
    }
}
