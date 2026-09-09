use std::fs;
use std::path::{Path, PathBuf};
use std::time::UNIX_EPOCH;

use gio::prelude::*;

const MAX_BATCH_SIZE: usize = 64;
const MAX_ARGUMENT_BYTES: usize = 256 * 1024;
const GIO_ATTRIBUTES: &str = concat!(
    "standard::icon,standard::is-symlink,",
    "access::can-read,access::can-write,",
    "metadata::custom-icon,metadata::custom-icon-name,metadata::emblems"
);

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct NormalizedIcon {
    pub names: Vec<String>,
    pub file_url: Option<String>,
    pub file_version: Option<String>,
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
struct VisualMetadata {
    file_path: String,
    icon: NormalizedIcon,
    emblems: Vec<String>,
    is_symlink: bool,
    symlink_broken: bool,
    error: Option<String>,
}

pub fn run(args: &[String]) -> Result<String, String> {
    if args.is_empty() {
        return Err("file-visual-metadata requires at least one local path".into());
    }
    if args.len() > MAX_BATCH_SIZE {
        return Err(format!(
            "file-visual-metadata batch exceeds {MAX_BATCH_SIZE} paths"
        ));
    }
    let payload_bytes: usize = args.iter().map(|path| path.len()).sum();
    if payload_bytes > MAX_ARGUMENT_BYTES {
        return Err("file-visual-metadata request exceeds payload limit".into());
    }

    let items = args
        .iter()
        .map(|path| query_visual_metadata(path))
        .map(|item| visual_metadata_to_json(&item))
        .collect::<Vec<_>>();
    Ok(format!(
        "{{\"ok\":true,\"operation\":\"file-visual-metadata\",\"items\":[{}]}}",
        items.join(",")
    ))
}

fn query_visual_metadata(path: &str) -> VisualMetadata {
    let path_buf = PathBuf::from(path);
    let (is_symlink, symlink_broken) = symlink_state(&path_buf);
    let mut result = VisualMetadata {
        file_path: path.to_string(),
        is_symlink,
        symlink_broken,
        ..VisualMetadata::default()
    };

    if !path_buf.is_absolute() {
        result.error = Some("path is not absolute".into());
        return result;
    }

    let file = gio::File::for_path(&path_buf);
    if !file.is_native() {
        result.error = Some("path is not native".into());
        return result;
    }

    let info = file
        .query_info(
            GIO_ATTRIBUTES,
            gio::FileQueryInfoFlags::NONE,
            None::<&gio::Cancellable>,
        )
        .or_else(|_| {
            file.query_info(
                GIO_ATTRIBUTES,
                gio::FileQueryInfoFlags::NOFOLLOW_SYMLINKS,
                None::<&gio::Cancellable>,
            )
        });
    let Ok(info) = info else {
        result.error = Some("GIO metadata unavailable".into());
        return result;
    };

    result.is_symlink = result.is_symlink || info.is_symlink();
    let standard_icon = info.icon();
    let mut icon_emblems = Vec::new();
    if let Some(custom_uri) = info.attribute_string("metadata::custom-icon") {
        if let Some(custom_file) = resolve_custom_icon_file(&file, &path_buf, &custom_uri) {
            let custom_icon = gio::FileIcon::new(&custom_file);
            let (icon, emblems) = normalize_visual_icon_with_emblems(
                Some(custom_icon.upcast_ref()),
                standard_icon.as_ref(),
                &path_buf,
            );
            result.icon = icon;
            icon_emblems = emblems;
        }
    }

    if result.icon.names.is_empty() && result.icon.file_url.is_none() {
        if let Some(custom_name) = info.attribute_string("metadata::custom-icon-name") {
            result.icon =
                normalize_custom_themed_name(Some(custom_name.as_str()), standard_icon.as_ref());
            if let Some(standard_icon) = standard_icon.as_ref() {
                icon_emblems = normalize_icon_and_emblems(standard_icon, &path_buf).1;
            }
        }
    }

    if result.icon.names.is_empty() && result.icon.file_url.is_none() {
        if let Some(icon) = standard_icon.as_ref() {
            (result.icon, icon_emblems) = normalize_icon_and_emblems(icon, &path_buf);
        }
    }

    let automatic = automatic_emblem_names(&info, result.is_symlink, &path_buf);
    let metadata = info
        .attribute_stringv("metadata::emblems")
        .iter()
        .map(|name| name.to_string())
        .collect::<Vec<_>>();
    result.emblems = merge_emblem_names(&automatic, &icon_emblems);
    result.emblems = merge_emblem_names(&result.emblems, &metadata);
    result
}

fn symlink_state(path: &Path) -> (bool, bool) {
    let Ok(link_metadata) = fs::symlink_metadata(path) else {
        return (false, false);
    };
    if !link_metadata.file_type().is_symlink() {
        return (false, false);
    }
    (true, fs::metadata(path).is_err())
}

fn resolve_custom_icon_file(item: &gio::File, path: &Path, raw: &str) -> Option<gio::File> {
    let value = raw.trim();
    if value.is_empty() {
        return None;
    }
    let icon_file = if value.contains("://") {
        gio::File::for_uri(value)
    } else if Path::new(value).is_absolute() {
        gio::File::for_path(value)
    } else if path.is_dir() {
        item.resolve_relative_path(value)
    } else {
        item.parent()?.resolve_relative_path(value)
    };
    if !icon_file.is_native() {
        return None;
    }
    let icon_path = icon_file.path()?;
    icon_path.is_file().then_some(icon_file)
}

fn normalize_icon(icon: &gio::Icon, path: &Path) -> NormalizedIcon {
    if let Some(themed) = icon.downcast_ref::<gio::ThemedIcon>() {
        return NormalizedIcon {
            names: normalize_themed_icon(themed),
            ..NormalizedIcon::default()
        };
    }
    if let Some(file_icon) = icon.downcast_ref::<gio::FileIcon>() {
        return normalize_file_icon(file_icon, path);
    }
    if let Some(emblemed) = icon.downcast_ref::<gio::EmblemedIcon>() {
        return normalize_icon(&emblemed.icon(), path);
    }
    NormalizedIcon::default()
}

fn normalize_icon_and_emblems(icon: &gio::Icon, path: &Path) -> (NormalizedIcon, Vec<String>) {
    if let Some(emblemed) = icon.downcast_ref::<gio::EmblemedIcon>() {
        let mut emblems = Vec::new();
        for emblem in emblemed.emblems() {
            let normalized = normalize_icon(&emblem.icon(), path);
            if let Some(name) = normalized.names.first() {
                append_icon_names(&mut emblems, vec![name.clone()]);
            }
        }
        return (normalize_icon(&emblemed.icon(), path), emblems);
    }
    (normalize_icon(icon, path), Vec::new())
}

fn normalize_visual_icon_with_emblems(
    custom: Option<&gio::Icon>,
    standard: Option<&gio::Icon>,
    path: &Path,
) -> (NormalizedIcon, Vec<String>) {
    if let Some(custom) = custom {
        let (mut icon, custom_emblems) = normalize_icon_and_emblems(custom, path);
        if !icon.names.is_empty() || icon.file_url.is_some() {
            let mut emblems = custom_emblems;
            if let Some(standard) = standard {
                let (standard_icon, standard_emblems) = normalize_icon_and_emblems(standard, path);
                append_icon_names(&mut icon.names, standard_icon.names);
                emblems = merge_emblem_names(&emblems, &standard_emblems);
            }
            return (icon, emblems);
        }
    }
    standard
        .map(|icon| normalize_icon_and_emblems(icon, path))
        .unwrap_or_default()
}

fn normalize_file_icon(icon: &gio::FileIcon, _path: &Path) -> NormalizedIcon {
    let file = icon.file();
    if !file.is_native() {
        return NormalizedIcon::default();
    }
    let Some(path) = file.path() else {
        return NormalizedIcon::default();
    };
    if !path.is_file() {
        return NormalizedIcon::default();
    }
    let Some(version) = file_version(&path) else {
        return NormalizedIcon::default();
    };
    NormalizedIcon {
        names: Vec::new(),
        file_url: Some(file.uri().to_string()),
        file_version: Some(version),
    }
}

fn normalize_themed_icon(icon: &gio::ThemedIcon) -> Vec<String> {
    icon.names()
        .into_iter()
        .map(|name| name.to_string())
        .filter(|name| !name.trim().is_empty())
        .collect()
}

fn normalize_custom_themed_name(
    custom_name: Option<&str>,
    standard: Option<&gio::Icon>,
) -> NormalizedIcon {
    let mut names = custom_name
        .filter(|name| !name.trim().is_empty())
        .map(|name| normalize_themed_icon(&gio::ThemedIcon::with_default_fallbacks(name)))
        .unwrap_or_default();
    if let Some(standard) = standard {
        append_icon_names(&mut names, normalize_icon(standard, Path::new("/")).names);
    }
    NormalizedIcon {
        names,
        ..NormalizedIcon::default()
    }
}

fn append_icon_names(names: &mut Vec<String>, candidates: Vec<String>) {
    for name in candidates {
        if !names.contains(&name) {
            names.push(name);
        }
    }
}

fn canonical_emblem_key(name: &str) -> String {
    name.trim()
        .strip_prefix("emblem-")
        .unwrap_or(name.trim())
        .to_string()
}

fn merge_emblem_names(automatic: &[String], metadata: &[String]) -> Vec<String> {
    let mut result = Vec::new();
    let mut keys = Vec::new();
    for name in automatic.iter().chain(metadata.iter()) {
        let identity = name.trim();
        let key = canonical_emblem_key(identity);
        if !key.is_empty() && !keys.contains(&key) {
            keys.push(key);
            result.push(identity.to_string());
        }
    }
    result
}

fn automatic_emblem_names(info: &gio::FileInfo, is_symlink: bool, path: &Path) -> Vec<String> {
    let mut automatic = Vec::new();
    if is_symlink {
        automatic.push("symbolic-link-symbolic".to_string());
    }
    if info.has_attribute("access::can-read") && !info.boolean("access::can-read") {
        automatic.push("not-accessible-symbolic".to_string());
    } else if info.has_attribute("access::can-write")
        && !info.boolean("access::can-write")
        && !path.starts_with(trash_root())
    {
        automatic.push("readonly-symbolic".to_string());
    }
    automatic
}

fn file_version(path: &Path) -> Option<String> {
    let metadata = fs::metadata(path).ok()?;
    let modified = metadata
        .modified()
        .ok()?
        .duration_since(UNIX_EPOCH)
        .ok()?
        .as_nanos();
    Some(format!("{modified}-{}", metadata.len()))
}

fn trash_root() -> PathBuf {
    std::env::var_os("XDG_DATA_HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            std::env::var_os("HOME")
                .map(PathBuf::from)
                .unwrap_or_default()
                .join(".local/share")
        })
        .join("Trash/files")
}

fn visual_metadata_to_json(item: &VisualMetadata) -> String {
    let file_url = item.icon.file_url.as_deref().unwrap_or_default();
    let file_version = item.icon.file_version.as_deref().unwrap_or_default();
    let names = item
        .icon
        .names
        .iter()
        .map(|name| format!("\"{}\"", crate::json::escape(name)))
        .collect::<Vec<_>>()
        .join(",");
    let emblems = item
        .emblems
        .iter()
        .map(|name| format!("\"{}\"", crate::json::escape(name)))
        .collect::<Vec<_>>()
        .join(",");
    let error = item
        .error
        .as_deref()
        .map(|value| {
            format!(
                ",\"fileIconMetadataError\":\"{}\"",
                crate::json::escape(value)
            )
        })
        .unwrap_or_default();
    let mut output = format!(
        "{{\"filePath\":\"{}\",\"fileIconNames\":[{}],\"fileIconFileUrl\":\"{}\",\"fileIconFileVersion\":\"{}\",\"fileEmblemNames\":[{}],\"fileIconMetadataReady\":true,\"fileIsSymlink\":{},\"fileSymlinkBroken\":{}",
        crate::json::escape(&item.file_path),
        names,
        crate::json::escape(file_url),
        crate::json::escape(file_version),
        emblems,
        item.is_symlink,
        item.symlink_broken,
    );
    output.push_str(&error);
    output.push('}');
    output
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    #[test]
    fn themed_icon_normalization_preserves_gio_name_order() {
        let icon = gio::ThemedIcon::from_names(&[
            "text-x-generic",
            "application-octet-stream",
            "document",
        ]);

        let normalized = normalize_themed_icon(&icon);
        assert_eq!(
            &normalized[..3],
            &[
                "text-x-generic".to_string(),
                "application-octet-stream".to_string(),
                "document".to_string(),
            ]
        );
        assert_eq!(
            normalized,
            icon.names()
                .iter()
                .map(ToString::to_string)
                .collect::<Vec<_>>()
        );
    }

    #[test]
    fn custom_local_file_icon_has_precedence_over_standard_icon() {
        let root = std::env::temp_dir().join(format!(
            "astrea-visual-custom-icon-test-{}",
            std::process::id()
        ));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        let custom = root.join("custom.svg");
        fs::write(&custom, "<svg/>").unwrap();

        let standard = gio::ThemedIcon::new("text-x-generic");
        let custom_icon: gio::Icon = gio::FileIcon::new(&gio::File::for_path(&custom)).upcast();
        let standard_icon: gio::Icon = standard.upcast();
        let normalized =
            normalize_visual_icon_with_emblems(Some(&custom_icon), Some(&standard_icon), &custom).0;

        assert_eq!(
            normalized.file_url,
            Some(gio::File::for_path(&custom).uri().to_string())
        );
        assert!(normalized.names.contains(&"text-x-generic".to_string()));
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn custom_local_file_icon_keeps_standard_themed_fallback_names() {
        let root = std::env::temp_dir().join(format!(
            "astrea-visual-custom-fallback-test-{}",
            std::process::id()
        ));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        let custom = root.join("custom.svg");
        fs::write(&custom, "<svg/>").unwrap();

        let standard = gio::ThemedIcon::from_names(&["text-x-generic", "document"]);
        let custom_icon: gio::Icon = gio::FileIcon::new(&gio::File::for_path(&custom)).upcast();
        let standard_icon: gio::Icon = standard.upcast();
        let normalized =
            normalize_visual_icon_with_emblems(Some(&custom_icon), Some(&standard_icon), &custom).0;

        assert!(normalized.file_url.is_some());
        assert_eq!(
            &normalized.names[..2],
            &["text-x-generic".to_string(), "document".to_string()]
        );
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn custom_themed_icon_name_precedes_standard_identity() {
        let standard = gio::ThemedIcon::new("text-x-generic");
        let standard_icon: gio::Icon = standard.upcast();
        let normalized =
            normalize_custom_themed_name(Some("folder-documents"), Some(&standard_icon));

        assert_eq!(
            normalized.names.first().map(String::as_str),
            Some("folder-documents")
        );
        assert!(normalized.names.contains(&"text-x-generic".to_string()));
    }

    #[test]
    fn non_local_file_icon_is_not_downloaded() {
        let file = gio::File::for_uri("smb://server/share/icon.png");
        let icon: gio::Icon = gio::FileIcon::new(&file).upcast();
        let normalized =
            normalize_visual_icon_with_emblems(Some(&icon), None, Path::new("/tmp/item")).0;

        assert!(normalized.file_url.is_none());
        assert!(normalized.names.is_empty());
    }

    #[test]
    fn emblemed_icon_normalization_keeps_base_and_emblems_separate() {
        let base = gio::ThemedIcon::new("text-x-generic");
        let emblem_icon = gio::ThemedIcon::new("emblem-readonly");
        let emblem = gio::Emblem::new(&emblem_icon);
        let emblemed = gio::EmblemedIcon::new(&base, Some(&emblem));
        let icon: gio::Icon = emblemed.upcast();

        let (normalized, emblems) = normalize_icon_and_emblems(&icon, Path::new("/tmp/item"));
        assert_eq!(
            normalized.names.first().map(String::as_str),
            Some("text-x-generic")
        );
        assert_eq!(emblems, vec!["emblem-readonly".to_string()]);
    }

    #[test]
    fn emblem_merge_deduplicates_automatic_and_metadata_names() {
        let automatic = vec![
            "symbolic-link-symbolic".to_string(),
            "readonly-symbolic".to_string(),
        ];
        let metadata = vec![
            "emblem-readonly-symbolic".to_string(),
            "readonly-symbolic".to_string(),
            "emblem-favorite".to_string(),
            "symbolic-link-symbolic".to_string(),
        ];

        assert_eq!(
            merge_emblem_names(&automatic, &metadata),
            vec![
                "symbolic-link-symbolic".to_string(),
                "readonly-symbolic".to_string(),
                "emblem-favorite".to_string(),
            ]
        );
    }

    #[test]
    fn unsupported_icon_normalizes_to_empty_identity() {
        let icon: gio::Icon =
            gio::BytesIcon::new(&gio::glib::Bytes::from_static(b"unsupported")).upcast();
        let normalized = normalize_icon(&icon, Path::new("/tmp/item"));

        assert_eq!(normalized, NormalizedIcon::default());
    }

    #[test]
    fn missing_optional_file_info_attributes_are_safe() {
        let info = gio::FileInfo::new();

        assert!(!info.has_attribute("access::can-read"));
        assert!(!info.has_attribute("access::can-write"));
        assert!(info.attribute_string("metadata::custom-icon").is_none());
        assert!(
            info.attribute_string("metadata::custom-icon-name")
                .is_none()
        );
        assert!(info.attribute_stringv("metadata::emblems").is_empty());
    }

    #[test]
    fn automatic_access_emblems_use_gio_flags() {
        let unreadable = gio::FileInfo::new();
        unreadable.set_attribute_boolean("access::can-read", false);
        unreadable.set_attribute_boolean("access::can-write", false);
        assert_eq!(
            automatic_emblem_names(&unreadable, false, Path::new("/tmp/item")),
            vec!["not-accessible-symbolic".to_string()]
        );

        let readonly = gio::FileInfo::new();
        readonly.set_attribute_boolean("access::can-read", true);
        readonly.set_attribute_boolean("access::can-write", false);
        assert_eq!(
            automatic_emblem_names(&readonly, false, Path::new("/tmp/item")),
            vec!["readonly-symbolic".to_string()]
        );
    }

    #[test]
    fn run_returns_utility_object_with_one_item_per_path() {
        let payload = run(&["/etc/hosts".to_string(), "/etc/hostname".to_string()]).unwrap();
        assert!(payload.starts_with("{\"ok\":true,\"operation\":\"file-visual-metadata\""));
        assert!(payload.contains("\"items\":["));
        assert!(payload.contains("\"filePath\":\"/etc/hosts\""));
        assert!(payload.contains("\"filePath\":\"/etc/hostname\""));
    }

    #[test]
    fn run_rejects_batches_above_bound() {
        let paths = (0..=MAX_BATCH_SIZE)
            .map(|index| format!("/tmp/file-{index}"))
            .collect::<Vec<_>>();
        assert!(run(&paths).is_err());
    }
}
