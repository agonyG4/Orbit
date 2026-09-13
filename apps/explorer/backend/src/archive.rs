use std::ffi::OsString;
use std::fs;
use std::io::{self, Read, Write};
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
    pub bsdtar_xz_threads: bool,
    pub bsdtar_zstd_threads: bool,
    pub seven_zip_tar_family: bool,
    pub seven_zip_rar: bool,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CreatePlan {
    pub format: FormatId,
    pub provider: ProviderKind,
    pub canonical_extension: &'static str,
    pub profiles: &'static [CompressionProfile],
    pub xz_threads: bool,
    pub zstd_threads: bool,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct Capability {
    pub id: String,
    pub label: String,
    pub extension: String,
    pub create_supported: bool,
    pub extract_supported: bool,
    pub profiles: Vec<CompressionProfile>,
    pub create_provider: Option<ProviderKind>,
    pub extract_provider: Option<ProviderKind>,
    pub create_password_supported: bool,
    pub extract_password_supported: bool,
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
            Self::InvalidRequest(_) => "invalid-request",
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
    let bsdtar_xz_threads = bsdtar && probe_bsdtar_codec_threads("xz", "--xz");
    let bsdtar_zstd_threads = bsdtar_zstd && probe_bsdtar_codec_threads("zstd", "--zstd");
    let seven_zip_tar_family =
        seven_zip && probe_seven_zip_formats(&["tar", "gzip", "bzip2", "xz", "zstd"]);
    let seven_zip_rar = seven_zip && probe_seven_zip_formats(&["rar"]);
    ProviderAvailability {
        bsdtar,
        seven_zip,
        rar,
        bsdtar_zstd,
        bsdtar_xz_threads,
        bsdtar_zstd_threads,
        seven_zip_tar_family,
        seven_zip_rar,
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

fn probe_bsdtar_codec_threads(codec: &str, switch: &str) -> bool {
    let stamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos();
    let root = std::env::temp_dir().join(format!(
        ".astrea-bsdtar-probe-{}-{stamp}",
        std::process::id()
    ));
    if fs::create_dir(&root).is_err() {
        return false;
    }
    let source = root.join("sample");
    let output = root.join("probe.tar");
    let supported = fs::write(&source, b"probe").is_ok()
        && Command::new("bsdtar")
            .args([
                "-cf",
                output.to_string_lossy().as_ref(),
                "--format=ustar",
                switch,
                &format!("--options={codec}:compression-level=1,threads=0"),
                "-C",
                root.to_string_lossy().as_ref(),
                "sample",
            ])
            .env("LC_ALL", "C")
            .status()
            .map(|status| status.success())
            .unwrap_or(false);
    let _ = fs::remove_dir_all(root);
    supported
}

fn probe_seven_zip_formats(formats: &[&str]) -> bool {
    let Ok(output) = Command::new("7z")
        .args(["i", "-t7z"])
        .env("LC_ALL", "C")
        .output()
    else {
        return false;
    };
    if !output.status.success() {
        return false;
    }
    let text = String::from_utf8_lossy(&output.stdout).to_ascii_lowercase();
    formats.iter().all(|format| {
        text.lines()
            .any(|line| line.split_whitespace().any(|word| word == *format))
    })
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
        FormatId::Zip if providers.seven_zip => (ProviderKind::SevenZip, "zip", ALL_PROFILES),
        FormatId::Zip if providers.bsdtar => (ProviderKind::Bsdtar, "zip", NO_PROFILES),
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

    let xz_threads = provider == ProviderKind::Bsdtar
        && format == FormatId::TarXz
        && providers.bsdtar_xz_threads;
    let zstd_threads = provider == ProviderKind::Bsdtar
        && format == FormatId::TarZst
        && providers.bsdtar_zstd_threads;
    Ok(CreatePlan {
        format,
        provider,
        canonical_extension: extension,
        profiles,
        xz_threads,
        zstd_threads,
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
                   create_provider: Option<ProviderKind>,
                   extract_provider: Option<ProviderKind>,
                   create_password_supported: bool,
                   extract_password_supported: bool| {
        capabilities.push(Capability {
            id: id.to_string(),
            label: label.to_string(),
            extension: extension.to_string(),
            create_supported,
            extract_supported,
            profiles: profiles.to_vec(),
            create_provider,
            extract_provider,
            create_password_supported,
            extract_password_supported,
        });
    };

    if providers.bsdtar || providers.seven_zip {
        let create_provider = if providers.seven_zip {
            Some(ProviderKind::SevenZip)
        } else {
            Some(ProviderKind::Bsdtar)
        };
        let extract_provider = if providers.seven_zip {
            Some(ProviderKind::SevenZip)
        } else {
            Some(ProviderKind::Bsdtar)
        };
        let profiles = if providers.seven_zip {
            ALL_PROFILES
        } else {
            NO_PROFILES
        };
        add(
            "zip",
            "ZIP",
            "zip",
            true,
            true,
            profiles,
            create_provider,
            extract_provider,
            false,
            providers.seven_zip,
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
            Some(ProviderKind::SevenZip),
            Some(ProviderKind::SevenZip),
            false,
            true,
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
            Some(ProviderKind::Bsdtar),
            Some(ProviderKind::Bsdtar),
            false,
            false,
        );
        add(
            "tar.gz",
            "TAR.GZ",
            "tar.gz",
            true,
            true,
            ALL_PROFILES,
            Some(ProviderKind::Bsdtar),
            Some(ProviderKind::Bsdtar),
            false,
            false,
        );
        add(
            "tar.xz",
            "TAR.XZ",
            "tar.xz",
            true,
            true,
            ALL_PROFILES,
            Some(ProviderKind::Bsdtar),
            Some(ProviderKind::Bsdtar),
            false,
            false,
        );
        if providers.bsdtar_zstd {
            add(
                "tar.zst",
                "TAR.ZST",
                "tar.zst",
                true,
                true,
                ALL_PROFILES,
                Some(ProviderKind::Bsdtar),
                Some(ProviderKind::Bsdtar),
                false,
                false,
            );
        }
        add(
            "tar.bz2",
            "TAR.BZ2",
            "tar.bz2",
            false,
            true,
            NO_PROFILES,
            None,
            Some(ProviderKind::Bsdtar),
            false,
            false,
        );
    } else if providers.seven_zip && providers.seven_zip_tar_family {
        for (id, label) in [
            ("tar", "TAR"),
            ("tar.gz", "TAR.GZ"),
            ("tar.bz2", "TAR.BZ2"),
            ("tar.xz", "TAR.XZ"),
            ("tar.zst", "TAR.ZST"),
        ] {
            add(
                id,
                label,
                id,
                false,
                true,
                NO_PROFILES,
                None,
                Some(ProviderKind::SevenZip),
                false,
                true,
            );
        }
    }
    if providers.seven_zip && providers.seven_zip_rar {
        add(
            "rar",
            "RAR",
            "rar",
            false,
            true,
            NO_PROFILES,
            None,
            Some(ProviderKind::SevenZip),
            false,
            true,
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
    #[serde(rename = "destinationMode", default = "default_destination_mode")]
    destination_mode: String,
    #[serde(default)]
    format: String,
    #[serde(default = "default_profile")]
    profile: String,
    #[serde(default)]
    password: String,
    #[serde(rename = "conflictPolicy", default = "default_conflict_policy")]
    conflict_policy: String,
}

fn default_profile() -> String {
    "balanced".into()
}

fn default_conflict_policy() -> String {
    "keep-both".into()
}

fn default_destination_mode() -> String {
    "new-directory".into()
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

#[derive(Clone, Debug, Eq, PartialEq)]
struct PreparedSourcePlan {
    cwd: PathBuf,
    sources: Vec<String>,
    materialize: bool,
}

fn plan_source_preparation(
    plan: &CreatePlan,
    sources: &[SourceInfo],
) -> Result<PreparedSourcePlan, ArchiveError> {
    if plan.provider != ProviderKind::SevenZip {
        return Ok(PreparedSourcePlan {
            cwd: PathBuf::new(),
            sources: sources
                .iter()
                .map(|source| source.basename.to_string_lossy().into_owned())
                .collect(),
            materialize: false,
        });
    }
    let Some(first_parent) = sources
        .first()
        .and_then(|source| source.path.parent())
        .map(Path::to_path_buf)
    else {
        return Err(ArchiveError::InvalidSource(
            "archive source has no working parent".into(),
        ));
    };
    let same_parent = sources.iter().all(|source| {
        source
            .path
            .parent()
            .is_some_and(|parent| parent == first_parent)
    });
    Ok(PreparedSourcePlan {
        cwd: if same_parent {
            first_parent
        } else {
            PathBuf::new()
        },
        sources: sources
            .iter()
            .map(|source| source.basename.to_string_lossy().into_owned())
            .collect(),
        materialize: !same_parent,
    })
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
struct SevenZipMemberRecord {
    path: String,
    encrypted: bool,
    symbolic_link: bool,
    hard_link: bool,
    attributes: String,
}

fn parse_seven_zip_slt_records(input: &str) -> Result<Vec<SevenZipMemberRecord>, ArchiveError> {
    let mut records = Vec::new();
    for block in input.split("\n\n") {
        let mut record = SevenZipMemberRecord::default();
        let mut has_field = false;
        for line in block.lines() {
            let Some((key, value)) = line.split_once(" = ") else {
                continue;
            };
            has_field = true;
            let value = value.trim();
            match key {
                "Path" => record.path = value.to_string(),
                "Encrypted" => {
                    record.encrypted = value == "+" || value.eq_ignore_ascii_case("true")
                }
                "Symbolic Link" => record.symbolic_link = !value.is_empty() && value != "-",
                "Hard Link" => record.hard_link = !value.is_empty() && value != "-",
                "Attributes" => record.attributes = value.to_string(),
                _ => {}
            }
        }
        if !has_field || record.path.is_empty() {
            continue;
        }
        let attributes = record.attributes.to_ascii_uppercase();
        record.symbolic_link |= attributes.contains('L');
        record.hard_link |= attributes.contains('H');
        records.push(record);
    }
    if records.is_empty() && !input.trim().is_empty() {
        return Err(ArchiveError::ProviderFailed(
            "7z listing did not contain member records".into(),
        ));
    }
    Ok(records)
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
    let base = canonical_archive_stem(name);
    Ok(parent.join(format!("{base}.{extension}")))
}

const ARCHIVE_SUFFIXES: &[&str] = &[
    ".tar.zst", ".tar.bz2", ".tar.xz", ".tar.gz", ".tzst", ".tbz2", ".txz", ".tgz", ".7z", ".zip",
    ".tar", ".rar",
];

fn archive_suffix(name: &str) -> Option<&str> {
    ARCHIVE_SUFFIXES.iter().copied().find(|suffix| {
        name.len() > suffix.len() && name[name.len() - suffix.len()..].eq_ignore_ascii_case(suffix)
    })
}

fn canonical_archive_stem(name: &str) -> String {
    archive_suffix(name)
        .map(|suffix| name[..name.len() - suffix.len()].to_string())
        .unwrap_or_else(|| name.to_string())
}

fn choose_archive_target(path: &Path, policy: &str) -> Result<PathBuf, ArchiveError> {
    if !path_occupied(path) || policy == "overwrite" {
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
    let name = path
        .file_name()
        .and_then(|value| value.to_str())
        .unwrap_or("Archive");
    let stem = canonical_archive_stem(name);
    let suffix = archive_suffix(name)
        .map(str::to_string)
        .or_else(|| {
            path.extension()
                .and_then(|value| value.to_str())
                .map(|value| format!(".{value}"))
        })
        .unwrap_or_default();
    for index in 2..10000 {
        let filename = if suffix.is_empty() {
            format!("{stem} ({index})")
        } else {
            format!("{stem} ({index}){suffix}")
        };
        let candidate = path.with_file_name(filename);
        if !path_occupied(&candidate) {
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
                    let threading = if plan.xz_threads { ",threads=0" } else { "" };
                    args.push(OsString::from(format!(
                        "--options=xz:compression-level={}{threading}",
                        xz_profile_level(profile)
                    )));
                }
                FormatId::TarZst => {
                    args.push(OsString::from("--format=ustar"));
                    args.push(OsString::from("--zstd"));
                    let threading = if plan.zstd_threads { ",threads=0" } else { "" };
                    args.push(OsString::from(format!(
                        "--options=zstd:compression-level={}{threading}",
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
        copy_file_with_cancellation(source, destination, cancellation)?;
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

const SOURCE_COPY_CHUNK_BYTES: usize = 1024 * 1024;

fn copy_file_with_cancellation(
    source: &Path,
    destination: &Path,
    cancellation: &Cancellation,
) -> Result<(), ArchiveError> {
    let mut input = fs::File::open(source)
        .map_err(|error| ArchiveError::InvalidSource(format!("open source: {error}")))?;
    if let Some(parent) = destination.parent() {
        fs::create_dir_all(parent).map_err(|error| {
            ArchiveError::ProviderFailed(format!("create staged source parent: {error}"))
        })?;
    }
    let mut output = fs::File::create(destination)
        .map_err(|error| ArchiveError::ProviderFailed(format!("stage source: {error}")))?;
    let mut buffer = vec![0_u8; SOURCE_COPY_CHUNK_BYTES];
    loop {
        if cancellation.is_cancelled() {
            return Err(ArchiveError::Cancelled);
        }
        let count = input
            .read(&mut buffer)
            .map_err(|error| ArchiveError::ProviderFailed(format!("read source: {error}")))?;
        if count == 0 {
            break;
        }
        output
            .write_all(&buffer[..count])
            .map_err(|error| ArchiveError::ProviderFailed(format!("stage source: {error}")))?;
    }
    let metadata = fs::metadata(source)
        .map_err(|error| ArchiveError::InvalidSource(format!("read source metadata: {error}")))?;
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        fs::set_permissions(
            destination,
            fs::Permissions::from_mode(metadata.permissions().mode() & 0o7777),
        )
        .map_err(|error| ArchiveError::ProviderFailed(format!("preserve source mode: {error}")))?;
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
    let preparation = plan_source_preparation(&plan, &sources)?;
    let cwd = if preparation.materialize {
        let input_root = stage.path.join("input");
        emitter.emit(
            "create",
            "preparing",
            0,
            Some(total_count),
            Some(0),
            Some(total_bytes),
            0.25,
            None,
            None,
            "Preparing source staging",
            true,
        );
        materialize_sources(&input_root, &sources, cancellation)?;
        emitter.emit(
            "create",
            "preparing",
            total_count,
            Some(total_count),
            Some(total_bytes),
            Some(total_bytes),
            0.30,
            None,
            None,
            "Sources prepared",
            true,
        );
        Some(input_root)
    } else if preparation.cwd.as_os_str().is_empty() {
        None
    } else {
        Some(preparation.cwd.clone())
    };
    let compressing_start = if preparation.materialize { 0.30 } else { 0.25 };
    let args = create_arguments(&plan, profile, &payload, &sources);
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
                compressing_start
                    + (0.90 - compressing_start) * (done as f64 / total_count.max(1) as f64),
                Some(Path::new(line)),
                current_name,
                "Compressing archive",
                false,
            );
        },
    )?;
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
    let zip_archive = name.ends_with(".zip");
    let seven_zip_archive = name.ends_with(".7z");
    let rar_archive = name.ends_with(".rar");
    let tar_zst_archive = name.ends_with(".tar.zst") || name.ends_with(".tzst");
    if seven_zip_archive || rar_archive {
        return if providers.seven_zip && (!rar_archive || providers.seven_zip_rar) {
            Ok(ProviderKind::SevenZip)
        } else {
            Err(ArchiveError::ProviderUnavailable)
        };
    }
    if zip_archive && providers.seven_zip {
        return Ok(ProviderKind::SevenZip);
    }
    if !password.is_empty() && providers.seven_zip {
        return Ok(ProviderKind::SevenZip);
    }
    if providers.bsdtar && (!tar_zst_archive || providers.bsdtar_zstd) {
        return Ok(ProviderKind::Bsdtar);
    }
    if providers.seven_zip && providers.seven_zip_tar_family {
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
            let mut listing = String::new();
            let outcome = run_provider("7z", None, &args, cancellation, |line| {
                listing.push_str(line);
                listing.push('\n');
            })?;
            match outcome {
                ProviderOutcome::Success => {}
                ProviderOutcome::Cancelled => return Err(ArchiveError::Cancelled),
                ProviderOutcome::Failed(message) => {
                    return Err(classify_provider_failure(&message, !password.is_empty()));
                }
            }
            let archive_path = archive.to_string_lossy();
            let records = parse_seven_zip_slt_records(&listing)?;
            let mut encrypted = false;
            for record in records {
                if record.path == archive_path {
                    continue;
                }
                validate_member(&record.path).map_err(ArchiveError::UnsafeMember)?;
                if record.symbolic_link || record.hard_link {
                    return Err(ArchiveError::UnsafeMember(
                        "archives containing symbolic or hard links are not supported".into(),
                    ));
                }
                encrypted |= record.encrypted;
                members.push(record.path);
            }
            if encrypted && password.is_empty() {
                return Err(ArchiveError::PasswordRequired);
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
    if request.destination_mode == "into-directory" {
        let metadata = fs::symlink_metadata(&destination).map_err(|error| {
            ArchiveError::InvalidRequest(format!("read extraction container: {error}"))
        })?;
        if metadata.file_type().is_symlink() || !metadata.is_dir() {
            return Err(ArchiveError::InvalidRequest(
                "into-directory extraction requires an existing directory".into(),
            ));
        }
    } else if request.destination_mode != "new-directory" {
        return Err(ArchiveError::InvalidRequest(
            "unknown extraction destination mode".into(),
        ));
    }
    fs::create_dir_all(parent).map_err(|error| {
        ArchiveError::ProviderFailed(format!("create destination parent: {error}"))
    })?;
    if request.destination_mode == "new-directory"
        && request.conflict_policy == "prompt"
        && path_occupied(&destination)
    {
        return Err(ArchiveError::DestinationConflict(destination));
    }
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
    let target = publish_extraction(
        &mut stage,
        &destination,
        &request.destination_mode,
        &request.conflict_policy,
        cancellation,
    )?;
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

fn path_occupied(path: &Path) -> bool {
    fs::symlink_metadata(path).is_ok()
}

fn remove_owned_path(path: &Path, protected_root: Option<&Path>) -> Result<(), String> {
    if protected_root == Some(path) {
        return Err("refusing to recursively remove extraction container".into());
    }
    let metadata = match fs::symlink_metadata(path) {
        Ok(metadata) => metadata,
        Err(error) if error.kind() == io::ErrorKind::NotFound => return Ok(()),
        Err(error) => return Err(format!("inspect published path: {error}")),
    };
    if metadata.is_dir() && !metadata.file_type().is_symlink() {
        fs::remove_dir_all(path).map_err(|error| format!("remove published path: {error}"))
    } else {
        fs::remove_file(path).map_err(|error| format!("remove published path: {error}"))
    }
}

struct PublicationTransaction {
    backup: Option<Stage>,
    backup_parent: PathBuf,
    protected_root: Option<PathBuf>,
    moved: Vec<(PathBuf, PathBuf)>,
    published: Vec<PathBuf>,
    committed: bool,
}

impl PublicationTransaction {
    fn new(protected_root: Option<&Path>, backup_parent: &Path) -> Self {
        Self {
            backup: None,
            backup_parent: backup_parent.to_path_buf(),
            protected_root: protected_root.map(Path::to_path_buf),
            moved: Vec::new(),
            published: Vec::new(),
            committed: false,
        }
    }

    fn ensure_backup(&mut self) -> Result<&Path, ArchiveError> {
        if self.backup.is_none() {
            self.backup = Some(Stage::new(&self.backup_parent, "extract-backup")?);
        }
        Ok(&self.backup.as_ref().expect("backup stage").path)
    }

    fn backup_existing(&mut self, path: &Path, relative: &Path) -> Result<(), ArchiveError> {
        if self.protected_root.as_deref() == Some(path) {
            return Err(ArchiveError::InvalidRequest(
                "refusing to replace an into-directory extraction container".into(),
            ));
        }
        let backup_root = self.ensure_backup()?.to_path_buf();
        let backup_path = backup_root.join(relative);
        if let Some(parent) = backup_path.parent() {
            fs::create_dir_all(parent).map_err(|error| {
                ArchiveError::ProviderFailed(format!("create extraction backup parent: {error}"))
            })?;
        }
        fs::rename(path, &backup_path).map_err(|error| {
            ArchiveError::ProviderFailed(format!("backup conflicting extraction path: {error}"))
        })?;
        self.moved.push((path.to_path_buf(), backup_path));
        Ok(())
    }

    fn commit(&mut self) {
        if let Some(backup) = self.backup.as_mut() {
            backup.finish();
        }
        self.committed = true;
    }

    fn rollback(&mut self) -> Result<(), ArchiveError> {
        let mut first_error = None;
        for path in self.published.drain(..).rev() {
            if let Err(error) = remove_owned_path(&path, self.protected_root.as_deref()) {
                first_error.get_or_insert(error);
            }
        }
        for (original, backup) in self.moved.drain(..).rev() {
            if path_occupied(&original)
                && let Err(error) = remove_owned_path(&original, self.protected_root.as_deref())
            {
                first_error.get_or_insert(error);
                continue;
            }
            if let Err(error) = fs::rename(&backup, &original) {
                first_error.get_or_insert(format!("restore extraction backup: {error}"));
            }
        }
        if let Some(backup) = self.backup.as_mut() {
            if first_error.is_none() {
                backup.finish();
            } else {
                backup.active = false;
            }
        }
        self.committed = true;
        match first_error {
            Some(error) => Err(ArchiveError::ProviderFailed(error)),
            None => Ok(()),
        }
    }
}

impl Drop for PublicationTransaction {
    fn drop(&mut self) {
        if !self.committed {
            let _ = self.rollback();
        }
    }
}

fn unique_child_target(
    destination: &Path,
    name: &str,
    reserved: &mut std::collections::HashSet<PathBuf>,
) -> PathBuf {
    let base = Path::new(name);
    let stem = base
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or(name);
    let suffix = base
        .extension()
        .and_then(|value| value.to_str())
        .map(|value| format!(".{value}"))
        .unwrap_or_default();
    for index in 1..10000 {
        let candidate_name = if index == 1 {
            name.to_string()
        } else {
            format!("{stem} ({index}){suffix}")
        };
        let candidate = destination.join(candidate_name);
        if !path_occupied(&candidate) && reserved.insert(candidate.clone()) {
            return candidate;
        }
    }
    destination.join(format!("{name} (unique)"))
}

fn staged_top_level_paths(stage: &Path) -> Result<Vec<PathBuf>, ArchiveError> {
    let mut entries = fs::read_dir(stage)
        .map_err(|error| ArchiveError::ProviderFailed(format!("read staged archive: {error}")))?
        .map(|entry| {
            entry.map(|entry| entry.path()).map_err(|error| {
                ArchiveError::ProviderFailed(format!("read staged member: {error}"))
            })
        })
        .collect::<Result<Vec<_>, _>>()?;
    entries.sort();
    Ok(entries)
}

fn publish_entry(
    incoming: &Path,
    target: &Path,
    relative: &Path,
    transaction: &mut PublicationTransaction,
    cancellation: &Cancellation,
) -> Result<(), ArchiveError> {
    if cancellation.is_cancelled() {
        return Err(ArchiveError::Cancelled);
    }
    if !path_occupied(target) {
        fs::rename(incoming, target).map_err(|error| {
            ArchiveError::ProviderFailed(format!("publish extracted member: {error}"))
        })?;
        transaction.published.push(target.to_path_buf());
        return Ok(());
    }

    let incoming_metadata = fs::symlink_metadata(incoming)
        .map_err(|error| ArchiveError::ProviderFailed(format!("inspect staged member: {error}")))?;
    let target_metadata = fs::symlink_metadata(target).map_err(|error| {
        ArchiveError::ProviderFailed(format!("inspect destination member: {error}"))
    })?;
    if incoming_metadata.is_dir()
        && !incoming_metadata.file_type().is_symlink()
        && target_metadata.is_dir()
        && !target_metadata.file_type().is_symlink()
    {
        let mut children = fs::read_dir(incoming)
            .map_err(|error| {
                ArchiveError::ProviderFailed(format!("read staged directory: {error}"))
            })?
            .map(|entry| {
                entry.map(|entry| entry.path()).map_err(|error| {
                    ArchiveError::ProviderFailed(format!("read staged child: {error}"))
                })
            })
            .collect::<Result<Vec<_>, _>>()?;
        children.sort();
        for child in children {
            let name = child
                .file_name()
                .ok_or_else(|| ArchiveError::ProviderFailed("staged member has no name".into()))?;
            publish_entry(
                &child,
                &target.join(name),
                &relative.join(name),
                transaction,
                cancellation,
            )?;
        }
        fs::remove_dir(incoming).map_err(|error| {
            ArchiveError::ProviderFailed(format!("finish staged directory merge: {error}"))
        })?;
        return Ok(());
    }

    if target_metadata.is_dir() && !target_metadata.file_type().is_symlink() {
        return Err(ArchiveError::InvalidRequest(
            "overwrite would replace a directory containing unrelated data; choose keep-both"
                .into(),
        ));
    }

    transaction.backup_existing(target, relative)?;
    fs::rename(incoming, target).map_err(|error| {
        ArchiveError::ProviderFailed(format!("replace extracted member: {error}"))
    })?;
    transaction.published.push(target.to_path_buf());
    Ok(())
}

fn publish_extraction(
    stage: &mut Stage,
    destination: &Path,
    mode: &str,
    policy: &str,
    cancellation: &Cancellation,
) -> Result<PathBuf, ArchiveError> {
    if mode == "new-directory" {
        let target = if path_occupied(destination) {
            match policy {
                "keep-both" => {
                    choose_target(destination, "keep-both").map_err(ArchiveError::ProviderFailed)?
                }
                "overwrite" => destination.to_path_buf(),
                "prompt" => {
                    return Err(ArchiveError::DestinationConflict(destination.to_path_buf()));
                }
                _ => {
                    return Err(ArchiveError::InvalidRequest(format!(
                        "unsupported extraction conflict policy: {policy}"
                    )));
                }
            }
        } else {
            destination.to_path_buf()
        };
        let backup_parent = target.parent().unwrap_or_else(|| Path::new("."));
        let mut transaction = PublicationTransaction::new(None, backup_parent);
        if path_occupied(&target) {
            transaction.backup_existing(
                &target,
                target
                    .file_name()
                    .map(PathBuf::from)
                    .as_deref()
                    .unwrap_or_else(|| Path::new("target")),
            )?;
        }
        if cancellation.is_cancelled() {
            return Err(ArchiveError::Cancelled);
        }
        fs::rename(&stage.path, &target).map_err(|error| {
            ArchiveError::ProviderFailed(format!("publish extracted archive: {error}"))
        })?;
        transaction.published.push(target.clone());
        transaction.commit();
        return Ok(target);
    }

    let entries = staged_top_level_paths(&stage.path)?;
    let backup_parent = destination.parent().unwrap_or_else(|| Path::new("."));
    let mut transaction = PublicationTransaction::new(Some(destination), backup_parent);
    if policy == "prompt" {
        for entry in &entries {
            let name = entry
                .file_name()
                .and_then(|value| value.to_str())
                .ok_or_else(|| {
                    ArchiveError::UnsafeMember("staged member has no valid name".into())
                })?;
            if path_occupied(&destination.join(name)) {
                return Err(ArchiveError::DestinationConflict(destination.join(name)));
            }
        }
    } else if policy != "keep-both" && policy != "overwrite" {
        return Err(ArchiveError::InvalidRequest(format!(
            "unsupported extraction conflict policy: {policy}"
        )));
    }
    let mut reserved = std::collections::HashSet::new();
    for entry in entries {
        let name = entry
            .file_name()
            .and_then(|value| value.to_str())
            .ok_or_else(|| ArchiveError::UnsafeMember("staged member has no valid name".into()))?;
        let existing_target = destination.join(name);
        let target = if policy == "keep-both" && path_occupied(&existing_target) {
            unique_child_target(destination, name, &mut reserved)
        } else {
            reserved.insert(existing_target.clone());
            existing_target
        };
        let relative = target
            .strip_prefix(destination)
            .unwrap_or_else(|_| Path::new(name));
        publish_entry(&entry, &target, relative, &mut transaction, cancellation)?;
    }
    transaction.commit();
    Ok(destination.to_path_buf())
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
    let mut request: OperationRequest = serde_json::from_str(encoded)
        .map_err(|error| format!("invalid archive request: {error}"))?;
    if request.password.is_empty() && std::env::var_os("ASTREA_STDIN_PAYLOAD").is_some() {
        const MAX_ARCHIVE_SECRET_BYTES: usize = 64 * 1024;
        let mut secret = Vec::new();
        io::stdin()
            .take((MAX_ARCHIVE_SECRET_BYTES + 1) as u64)
            .read_to_end(&mut secret)
            .map_err(|error| format!("read archive secret payload: {error}"))?;
        if secret.len() > MAX_ARCHIVE_SECRET_BYTES {
            return Err("archive secret payload exceeded the configured limit".into());
        }
        if !secret.is_empty() {
            request.password = String::from_utf8(secret)
                .map_err(|_| "archive secret payload was not valid UTF-8".to_string())?;
        }
    }
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
        "createProvider": capability.create_provider.map(ProviderKind::as_id),
        "extractProvider": capability.extract_provider.map(ProviderKind::as_id),
        "createPasswordSupported": capability.create_password_supported,
        "extractPasswordSupported": capability.extract_password_supported,
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
    if !path_occupied(destination) || policy == "overwrite" {
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
        if !path_occupied(&candidate) {
            return Ok(candidate);
        }
    }
    Err("could not choose a unique extraction target".into())
}

#[cfg(test)]
mod tests {
    use super::{
        ArchiveError, Cancellation, CompressionProfile, FormatId, OperationRequest,
        ProgressEmitter, ProviderAvailability, ProviderKind, SourceInfo, Stage,
        canonical_archive_path, capabilities_for, create_archive, create_arguments,
        extract_archive, plan_create, run_provider,
    };
    use serde_json::{Value, json};
    use std::fs;
    use std::path::{Path, PathBuf};
    use std::sync::atomic::Ordering;
    use std::time::Duration;

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
            bsdtar_xz_threads: true,
            bsdtar_zstd_threads: true,
            seven_zip_tar_family: true,
            seven_zip_rar: true,
        };
        let cases = [
            (FormatId::Zip, ProviderKind::SevenZip, "zip"),
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
            bsdtar_xz_threads: false,
            bsdtar_zstd_threads: false,
            seven_zip_tar_family: false,
            seven_zip_rar: false,
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
                bsdtar_xz_threads: false,
                bsdtar_zstd_threads: false,
                seven_zip_tar_family: false,
                seven_zip_rar: false,
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
                bsdtar_xz_threads: false,
                bsdtar_zstd_threads: false,
                seven_zip_tar_family: false,
                seven_zip_rar: false,
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
    fn advertised_compression_profiles_map_to_distinct_provider_arguments() {
        let plan = plan_create(
            FormatId::Zip,
            CompressionProfile::Balanced,
            ProviderAvailability {
                seven_zip: true,
                ..ProviderAvailability::default()
            },
        )
        .unwrap();
        let levels = [
            (CompressionProfile::Fast, "-mx=1"),
            (CompressionProfile::Balanced, "-mx=5"),
            (CompressionProfile::Maximum, "-mx=9"),
        ];
        for (profile, expected) in levels {
            let args = create_arguments(
                &plan,
                profile,
                Path::new("/tmp/archive.zip"),
                &[SourceInfo {
                    path: PathBuf::from("/tmp/source.txt"),
                    basename: "source.txt".into(),
                }],
            );
            assert!(args.iter().any(|arg| arg == expected));
        }

        let bsdtar_plan = plan_create(
            FormatId::Zip,
            CompressionProfile::Balanced,
            ProviderAvailability {
                bsdtar: true,
                ..ProviderAvailability::default()
            },
        )
        .unwrap();
        assert!(bsdtar_plan.profiles.is_empty());
    }

    #[test]
    fn bsdtar_threading_arguments_are_added_only_when_probe_succeeds() {
        let providers = ProviderAvailability {
            bsdtar: true,
            bsdtar_zstd: true,
            bsdtar_xz_threads: true,
            bsdtar_zstd_threads: true,
            ..ProviderAvailability::default()
        };
        for (format, extension, option) in [
            (
                FormatId::TarXz,
                "tar.xz",
                "--options=xz:compression-level=6,threads=0",
            ),
            (
                FormatId::TarZst,
                "tar.zst",
                "--options=zstd:compression-level=6,threads=0",
            ),
        ] {
            let plan = plan_create(format, CompressionProfile::Balanced, providers).unwrap();
            let args = create_arguments(
                &plan,
                CompressionProfile::Balanced,
                Path::new(&format!("/tmp/archive.{extension}")),
                &[SourceInfo {
                    path: PathBuf::from("/tmp/source.txt"),
                    basename: "source.txt".into(),
                }],
            );
            assert!(args.iter().any(|arg| arg == option));
        }

        let plan = plan_create(
            FormatId::TarXz,
            CompressionProfile::Balanced,
            ProviderAvailability {
                bsdtar: true,
                ..ProviderAvailability::default()
            },
        )
        .unwrap();
        let args = create_arguments(
            &plan,
            CompressionProfile::Balanced,
            Path::new("/tmp/archive.tar.xz"),
            &[SourceInfo {
                path: PathBuf::from("/tmp/source.txt"),
                basename: "source.txt".into(),
            }],
        );
        assert!(
            args.iter()
                .any(|arg| arg == "--options=xz:compression-level=6")
        );
    }

    #[test]
    fn capabilities_never_advertise_unavailable_create_formats() {
        let capabilities = capabilities_for(ProviderAvailability {
            bsdtar: true,
            seven_zip: false,
            rar: false,
            bsdtar_zstd: true,
            bsdtar_xz_threads: false,
            bsdtar_zstd_threads: false,
            seven_zip_tar_family: false,
            seven_zip_rar: false,
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
            destination_mode: "new-directory".into(),
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
            destination_mode: "new-directory".into(),
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
            destination_mode: "new-directory".into(),
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
            destination_mode: "new-directory".into(),
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
            destination_mode: "new-directory".into(),
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

    #[cfg(unix)]
    #[test]
    fn real_7z_symbolic_link_is_rejected_before_extraction() {
        if !super::discover_providers().seven_zip {
            return;
        }
        let root = test_root("7z-link");
        let source = root.join("source");
        fs::create_dir_all(&source).unwrap();
        fs::write(source.join("target.txt"), b"target").unwrap();
        std::os::unix::fs::symlink("target.txt", source.join("link.txt")).unwrap();
        let archive = root.join("link.7z");
        let provider = std::process::Command::new("7z")
            .current_dir(&source)
            .args([
                "a",
                "-bd",
                "-snl",
                archive.to_str().unwrap(),
                "target.txt",
                "link.txt",
            ])
            .output()
            .unwrap();
        assert!(provider.status.success());

        let external = root.join("external.txt");
        fs::write(&external, b"must remain untouched").unwrap();
        let destination = root.join("destination");
        let request: OperationRequest = serde_json::from_value(json!({
            "kind": "extract",
            "archivePath": archive,
            "destination": destination,
            "destinationMode": "new-directory",
            "conflictPolicy": "keep-both"
        }))
        .unwrap();
        let error = extract_archive(
            &request,
            &Cancellation { marker: None },
            &mut ProgressEmitter::new(),
        )
        .expect_err("7z links must be rejected during inspection");
        assert!(matches!(error, ArchiveError::UnsafeMember(_)));
        assert!(!destination.exists());
        assert_eq!(fs::read(external).unwrap(), b"must remain untouched");
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
            destination_mode: "new-directory".into(),
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
            destination_mode: "new-directory".into(),
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
            destination_mode: "new-directory".into(),
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
            destination_mode: "new-directory".into(),
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
            bsdtar_xz_threads: false,
            bsdtar_zstd_threads: false,
            seven_zip_tar_family: true,
            seven_zip_rar: true,
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
        assert!(
            seven_zip_only
                .iter()
                .any(|item| item.id == "tar.gz" && item.extract_supported)
        );

        assert!(capabilities_for(ProviderAvailability::default()).is_empty());
    }

    #[test]
    fn into_directory_overwrite_preserves_unrelated_contents() {
        if !super::discover_providers().bsdtar {
            return;
        }
        let root = test_root("into-directory-overwrite");
        let archive_source = root.join("archive-source");
        let destination = root.join("destination");
        fs::create_dir_all(&archive_source).unwrap();
        fs::create_dir_all(destination.join("folder")).unwrap();
        fs::write(archive_source.join("archive-file.txt"), b"incoming").unwrap();
        fs::create_dir_all(archive_source.join("folder")).unwrap();
        fs::write(archive_source.join("folder/a.txt"), b"incoming child").unwrap();
        fs::write(destination.join("archive-file.txt"), b"old conflicting").unwrap();
        fs::write(destination.join("unrelated.txt"), b"must survive").unwrap();
        fs::write(
            destination.join("folder/unrelated.txt"),
            b"nested must survive",
        )
        .unwrap();
        let archive = root.join("archive.zip");
        let provider = std::process::Command::new("bsdtar")
            .args([
                "-cf",
                archive.to_str().unwrap(),
                "--format=zip",
                "-C",
                archive_source.to_str().unwrap(),
                "archive-file.txt",
                "folder",
            ])
            .output()
            .unwrap();
        assert!(provider.status.success());

        let request: OperationRequest = serde_json::from_value(json!({
            "kind": "extract",
            "archivePath": archive,
            "destination": destination,
            "destinationMode": "into-directory",
            "profile": "balanced",
            "password": "",
            "conflictPolicy": "overwrite"
        }))
        .unwrap();
        let result = extract_archive(
            &request,
            &Cancellation { marker: None },
            &mut ProgressEmitter::new(),
        )
        .unwrap();
        assert_eq!(result["state"], Value::String("success".into()));
        assert_eq!(
            fs::read(destination.join("archive-file.txt")).unwrap(),
            b"incoming"
        );
        assert_eq!(
            fs::read(destination.join("unrelated.txt")).unwrap(),
            b"must survive"
        );
        assert_eq!(
            fs::read(destination.join("folder/a.txt")).unwrap(),
            b"incoming child"
        );
        assert_eq!(
            fs::read(destination.join("folder/unrelated.txt")).unwrap(),
            b"nested must survive"
        );
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn into_directory_keep_both_and_prompt_preserve_the_container() {
        if !super::discover_providers().bsdtar {
            return;
        }
        let root = test_root("into-directory-policies");
        let archive_source = root.join("source");
        fs::create_dir_all(&archive_source).unwrap();
        fs::write(archive_source.join("file.txt"), b"incoming").unwrap();
        let archive = root.join("archive.zip");
        let provider = std::process::Command::new("bsdtar")
            .args([
                "-cf",
                archive.to_str().unwrap(),
                "--format=zip",
                "-C",
                archive_source.to_str().unwrap(),
                "file.txt",
            ])
            .output()
            .unwrap();
        assert!(provider.status.success());

        let keep_both_destination = root.join("keep-both");
        fs::create_dir_all(&keep_both_destination).unwrap();
        fs::write(keep_both_destination.join("file.txt"), b"existing").unwrap();
        fs::write(keep_both_destination.join("unrelated.txt"), b"unrelated").unwrap();
        let keep_both: OperationRequest = serde_json::from_value(json!({
            "kind": "extract",
            "archivePath": archive.clone(),
            "destination": keep_both_destination.clone(),
            "destinationMode": "into-directory",
            "conflictPolicy": "keep-both"
        }))
        .unwrap();
        extract_archive(
            &keep_both,
            &Cancellation { marker: None },
            &mut ProgressEmitter::new(),
        )
        .unwrap();
        assert_eq!(
            fs::read(keep_both_destination.join("file.txt")).unwrap(),
            b"existing"
        );
        assert_eq!(
            fs::read(keep_both_destination.join("file (2).txt")).unwrap(),
            b"incoming"
        );
        assert_eq!(
            fs::read(keep_both_destination.join("unrelated.txt")).unwrap(),
            b"unrelated"
        );

        let prompt_destination = root.join("prompt");
        fs::create_dir_all(&prompt_destination).unwrap();
        fs::write(prompt_destination.join("file.txt"), b"existing").unwrap();
        fs::write(prompt_destination.join("unrelated.txt"), b"unrelated").unwrap();
        let prompt: OperationRequest = serde_json::from_value(json!({
            "kind": "extract",
            "archivePath": archive.clone(),
            "destination": prompt_destination.clone(),
            "destinationMode": "into-directory",
            "conflictPolicy": "prompt"
        }))
        .unwrap();
        let error = extract_archive(
            &prompt,
            &Cancellation { marker: None },
            &mut ProgressEmitter::new(),
        )
        .expect_err("prompt must suspend on a member conflict");
        assert!(matches!(error, ArchiveError::DestinationConflict(_)));
        assert_eq!(
            fs::read(prompt_destination.join("file.txt")).unwrap(),
            b"existing"
        );
        assert_eq!(
            fs::read(prompt_destination.join("unrelated.txt")).unwrap(),
            b"unrelated"
        );
        assert!(
            !fs::read_dir(&prompt_destination)
                .unwrap()
                .flatten()
                .any(|entry| {
                    entry
                        .file_name()
                        .to_string_lossy()
                        .starts_with(".astrea-extract-")
                })
        );

        let type_conflict_destination = root.join("type-conflict");
        fs::create_dir_all(type_conflict_destination.join("file.txt")).unwrap();
        fs::write(
            type_conflict_destination.join("file.txt/unrelated-child.txt"),
            b"must survive",
        )
        .unwrap();
        let overwrite: OperationRequest = serde_json::from_value(json!({
            "kind": "extract",
            "archivePath": archive,
            "destination": type_conflict_destination,
            "destinationMode": "into-directory",
            "conflictPolicy": "overwrite"
        }))
        .unwrap();
        let error = extract_archive(
            &overwrite,
            &Cancellation { marker: None },
            &mut ProgressEmitter::new(),
        )
        .expect_err("file-versus-directory overwrite must not discard descendants");
        assert!(matches!(error, ArchiveError::InvalidRequest(_)));
        assert_eq!(
            fs::read(type_conflict_destination.join("file.txt/unrelated-child.txt")).unwrap(),
            b"must survive"
        );
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn publication_rollback_guard_refuses_to_remove_the_into_directory_container() {
        let root = test_root("publication-root-guard");
        let error = super::remove_owned_path(&root, Some(&root))
            .expect_err("an into-directory container must never be recursively removed");
        assert!(error.contains("extraction container"));
        assert!(root.is_dir());
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn seven_zip_slt_link_state_never_aliases_encryption_state() {
        let records = super::parse_seven_zip_slt_records(
            "Path = regular.txt\nEncrypted = -\nAttributes = A\n\n\
Path = folder\nFolder = +\nEncrypted = -\nAttributes = D\n\n\
Path = encrypted.txt\nEncrypted = +\nAttributes = A\n\n\
Path = symbolic\nEncrypted = -\nSymbolic Link = target.txt\nAttributes = L\n\n\
Path = hard\nEncrypted = -\nHard Link = regular.txt\nAttributes = A\n\n\
Path = encrypted-symbolic\nEncrypted = +\nSymbolic Link = target.txt\nAttributes = L\n",
        )
        .unwrap();
        assert_eq!(records.len(), 6);
        assert!(!records[0].encrypted && !records[0].symbolic_link && !records[0].hard_link);
        assert!(!records[1].encrypted && !records[1].symbolic_link && !records[1].hard_link);
        assert!(records[2].encrypted && !records[2].symbolic_link && !records[2].hard_link);
        assert!(!records[3].encrypted && records[3].symbolic_link);
        assert!(!records[4].encrypted && records[4].hard_link);
        assert!(records[5].encrypted && records[5].symbolic_link);
    }

    #[test]
    fn canonical_archive_stem_matches_longest_supported_suffix() {
        let cases = [
            ("Archive.zip", "Archive"),
            ("Archive.7z", "Archive"),
            ("Archive.rar", "Archive"),
            ("Archive.tar", "Archive"),
            ("Archive.tar.gz", "Archive"),
            ("Archive.tgz", "Archive"),
            ("Archive.tar.bz2", "Archive"),
            ("Archive.tbz2", "Archive"),
            ("Archive.tar.xz", "Archive"),
            ("Archive.txz", "Archive"),
            ("Archive.tar.zst", "Archive"),
            ("Archive.tzst", "Archive"),
        ];
        for (input, expected) in cases {
            assert_eq!(super::canonical_archive_stem(input), expected, "{input}");
        }
    }

    #[test]
    fn same_parent_seven_zip_creation_does_not_materialize_sources() {
        let root = test_root("same-parent-plan");
        let first = root.join("first.txt");
        let second = root.join("second.txt");
        fs::write(&first, b"first").unwrap();
        fs::write(&second, b"second").unwrap();
        let sources = vec![
            SourceInfo {
                path: first,
                basename: "first.txt".into(),
            },
            SourceInfo {
                path: second,
                basename: "second.txt".into(),
            },
        ];
        let plan = plan_create(
            FormatId::SevenZip,
            CompressionProfile::Balanced,
            ProviderAvailability {
                seven_zip: true,
                ..ProviderAvailability::default()
            },
        )
        .unwrap();
        let prepared = super::plan_source_preparation(&plan, &sources).unwrap();
        assert!(!prepared.materialize);
        assert_eq!(prepared.cwd, root);
        assert_eq!(prepared.sources, vec!["first.txt", "second.txt"]);
        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn cancellation_during_source_preparation_removes_the_input_clone() {
        if !super::discover_providers().seven_zip {
            return;
        }
        let root = test_root("preparation-cancel");
        let first_parent = root.join("first");
        let second_parent = root.join("second");
        let output_parent = root.join("output");
        fs::create_dir_all(&first_parent).unwrap();
        fs::create_dir_all(&second_parent).unwrap();
        fs::create_dir_all(&output_parent).unwrap();
        let first = first_parent.join("large.bin");
        let second = second_parent.join("other.txt");
        fs::File::create(&first)
            .unwrap()
            .set_len(64 * 1024 * 1024)
            .unwrap();
        fs::write(&second, b"other").unwrap();
        let marker = root.join("cancel.marker");
        let watcher_parent = output_parent.clone();
        let watcher_marker = marker.clone();
        let watcher = std::thread::spawn(move || {
            for _ in 0..10_000 {
                let input_visible = fs::read_dir(&watcher_parent)
                    .unwrap()
                    .flatten()
                    .any(|entry| {
                        entry
                            .file_name()
                            .to_string_lossy()
                            .starts_with(".astrea-archive-")
                            && entry.path().join("input").is_dir()
                    });
                if input_visible {
                    fs::write(watcher_marker, b"cancel").unwrap();
                    return;
                }
                std::thread::sleep(Duration::from_millis(1));
            }
        });
        let request = OperationRequest {
            kind: "create".into(),
            sources: vec![first, second],
            archive_path: output_parent.join("Archive"),
            destination: PathBuf::new(),
            destination_mode: "new-directory".into(),
            format: "zip".into(),
            profile: "balanced".into(),
            password: String::new(),
            conflict_policy: "keep-both".into(),
        };
        let result = create_archive(
            &request,
            &Cancellation {
                marker: Some(marker.clone()),
            },
            &mut ProgressEmitter::new(),
        );
        watcher.join().unwrap();
        assert!(
            marker.exists(),
            "preparation watcher did not observe the input clone"
        );
        let error = result.expect_err("cancellation must be observed during preparation");
        assert_eq!(error, ArchiveError::Cancelled);
        assert!(!output_parent.join("Archive.zip").exists());
        assert!(
            !fs::read_dir(&output_parent)
                .unwrap()
                .flatten()
                .any(|entry| {
                    entry
                        .file_name()
                        .to_string_lossy()
                        .starts_with(".astrea-archive-")
                })
        );
        fs::remove_dir_all(root).unwrap();
    }
}
