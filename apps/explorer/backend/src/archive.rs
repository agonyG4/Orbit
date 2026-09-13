use std::fs;
use std::path::{Component, Path, PathBuf};
use std::process::Command;

use crate::json;

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
}

impl ArchiveError {
    pub fn code(&self) -> &'static str {
        match self {
            Self::ProviderUnavailable => "provider-unavailable",
            Self::UnsupportedFormat => "unsupported-format",
            Self::InvalidSource(_) => "invalid-source",
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
            providers.seven_zip,
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
            true,
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

pub fn run_extract(args: &[String]) -> Result<(), String> {
    println!("{}", extract(args)?);
    Ok(())
}

pub fn run_compress(args: &[String]) -> Result<(), String> {
    println!("{}", compress(args)?);
    Ok(())
}

pub fn extract(args: &[String]) -> Result<String, String> {
    let archive = args.first().ok_or_else(|| {
        "usage: archive-extract <archive> <destination> [password] [policy]".to_string()
    })?;
    let destination = args
        .get(1)
        .ok_or_else(|| "missing extraction destination".to_string())?;
    let password = args.get(2).map(String::as_str).unwrap_or_default();
    let policy = args.get(3).map(String::as_str).unwrap_or("keep-both");
    let archive = Path::new(archive);
    let destination = Path::new(destination);
    if !archive.is_file() {
        return Err(format!("archive not found: {}", archive.display()));
    }
    if let Some(parent) = destination.parent() {
        fs::create_dir_all(parent).map_err(|error| format!("create extraction parent: {error}"))?;
    }

    let listing = Command::new("bsdtar")
        .args(["-tf"])
        .arg(archive)
        .output()
        .map_err(|error| format!("list archive: {error}"))?;
    if !listing.status.success() {
        return Err(String::from_utf8_lossy(&listing.stderr).trim().to_string());
    }
    for raw in String::from_utf8_lossy(&listing.stdout).lines() {
        validate_member(raw)?;
    }

    // Never publish an archive containing links. A staged extraction prevents
    // path traversal, while rejecting links also prevents an archive member
    // from redirecting later writes outside the staged tree.
    let verbose_listing = Command::new("bsdtar")
        .args(["-tvf"])
        .arg(archive)
        .output()
        .map_err(|error| format!("inspect archive links: {error}"))?;
    if !verbose_listing.status.success() {
        return Err(String::from_utf8_lossy(&verbose_listing.stderr)
            .trim()
            .to_string());
    }
    for line in String::from_utf8_lossy(&verbose_listing.stdout).lines() {
        if line.starts_with('l') || line.starts_with('h') || line.contains(" -> ") {
            return Err("archives containing symbolic or hard links are not supported".into());
        }
    }

    let target = choose_target(destination, policy)?;
    let stage = target
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .join(format!(".astrea-extract-{}", std::process::id()));
    let _ = fs::remove_dir_all(&stage);
    fs::create_dir_all(&stage)
        .map_err(|error| format!("create extraction staging directory: {error}"))?;

    let result = if password.is_empty() {
        Command::new("bsdtar")
            .args(["-xf"])
            .arg(archive)
            .args(["-C"])
            .arg(&stage)
            .output()
    } else {
        Command::new("7z")
            .args(["x", "-y"])
            .arg(format!("-p{password}"))
            .arg(archive)
            .arg(format!("-o{}", stage.display()))
            .output()
    };
    let output = result.map_err(|error| format!("extract archive: {error}"))?;
    if !output.status.success() {
        let _ = fs::remove_dir_all(&stage);
        return Err(String::from_utf8_lossy(&output.stderr).trim().to_string());
    }
    if target.exists() {
        if policy == "overwrite" {
            remove_path(&target)?;
        } else {
            return Err(format!(
                "extraction target already exists: {}",
                target.display()
            ));
        }
    }
    fs::rename(&stage, &target).map_err(|error| format!("publish extracted archive: {error}"))?;
    Ok(format!(
        "{{\"ok\":true,\"operation\":\"archive-extract\",\"destination\":\"{}\"}}",
        json::escape(&target.to_string_lossy())
    ))
}

pub fn compress(args: &[String]) -> Result<String, String> {
    let source = args
        .first()
        .ok_or_else(|| "usage: archive-compress <source> <archive> [format]".to_string())?;
    let output = args
        .get(1)
        .ok_or_else(|| "missing archive output".to_string())?;
    let source = Path::new(source);
    let output = Path::new(output);
    if !source.exists() {
        return Err(format!("source not found: {}", source.display()));
    }
    if let Some(parent) = output.parent() {
        fs::create_dir_all(parent).map_err(|error| format!("create archive parent: {error}"))?;
    }
    let temp = output.with_extension(format!(
        "{}.tmp-{}",
        output
            .extension()
            .and_then(|v| v.to_str())
            .unwrap_or("archive"),
        std::process::id()
    ));
    let parent = source.parent().unwrap_or_else(|| Path::new("."));
    let name = source
        .file_name()
        .ok_or_else(|| "source has no file name".to_string())?;
    let result = Command::new("bsdtar")
        .args(["-caf"])
        .arg(&temp)
        .args(["-C"])
        .arg(parent)
        .arg(name)
        .output()
        .map_err(|error| format!("compress archive: {error}"))?;
    if !result.status.success() {
        let _ = fs::remove_file(&temp);
        return Err(String::from_utf8_lossy(&result.stderr).trim().to_string());
    }
    fs::rename(&temp, output).map_err(|error| format!("publish archive: {error}"))?;
    Ok(format!(
        "{{\"ok\":true,\"operation\":\"archive-compress\",\"path\":\"{}\"}}",
        json::escape(&output.to_string_lossy())
    ))
}

fn validate_member(value: &str) -> Result<(), String> {
    let path = Path::new(value);
    if value.is_empty() || path.is_absolute() {
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
        CompressionProfile, FormatId, ProviderAvailability, ProviderKind, capabilities_for,
        plan_create,
    };

    #[test]
    fn rejects_absolute_and_parent_archive_members() {
        assert!(super::validate_member("/tmp/escape").is_err());
        assert!(super::validate_member("../escape").is_err());
        assert!(super::validate_member("nested/../../escape").is_err());
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
}
