use std::env;
use std::fs;
use std::path::{Component, Path, PathBuf};
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

pub fn run(args: &[String]) -> Result<(), String> {
    let operation = args.first().map(String::as_str).unwrap_or_default();
    let result = match operation {
        "create-folder" => create_folder(args),
        "rename" => rename_path(args),
        "suggest-dirs" => suggest_dirs(args),
        "which" => which(args),
        "properties" => properties(args),
        "create-desktop-shortcut" => create_desktop_shortcut(args),
        "network-mount-probe" => network_mount_probe(args),
        "network-mount" => network_mount(args),
        "warm-thumbnails" => warm_thumbnails(args),
        "install-appimage" => install_appimage(args),
        "archive-extract" => crate::archive::extract(&args[1..]),
        "archive-compress" => crate::archive::compress(&args[1..]),
        "trash" => trash(args),
        "restore-trash" => restore_trash(args),
        "empty-trash" => empty_trash(args),
        other => Err(format!("unsupported utility operation: {other}")),
    };

    match result {
        Ok(payload) => println!("{payload}"),
        Err(message) => println!(
            "{{\"ok\":false,\"operation\":\"{}\",\"errorCode\":\"{}\",\"error\":\"{}\"}}",
            escape_json(operation),
            escape_json("operation_failed"),
            escape_json(&message)
        ),
    }
    Ok(())
}

fn warm_thumbnails(args: &[String]) -> Result<String, String> {
    let warmed = crate::thumbnails::warm_count(&args[1..])?;
    Ok(format!(
        "{{\"ok\":true,\"operation\":\"warm-thumbnails\",\"warmed\":{warmed}}}"
    ))
}

fn install_appimage(args: &[String]) -> Result<String, String> {
    crate::appimage::install(&args[1..])
}

fn create_folder(args: &[String]) -> Result<String, String> {
    let base = required(args, 1, "base")?;
    let name = safe_child_name(required(args, 2, "folder name")?, "folder_name")?;
    let base = expand_home(Path::new(base));
    if !base.is_dir() {
        return Err(format!("base directory does not exist: {}", base.display()));
    }

    let mut target = base.join(name);
    let mut index = 2;
    while path_exists(&target) {
        target = base.join(format!("{} {}", name, index));
        index += 1;
    }
    fs::create_dir(&target).map_err(|error| format!("create {}: {error}", target.display()))?;
    Ok(success_with_path("create-folder", &target))
}

fn rename_path(args: &[String]) -> Result<String, String> {
    let source = expand_home(Path::new(required(args, 1, "source")?));
    let name = safe_child_name(required(args, 2, "file name")?, "file_name")?;
    if !path_exists(&source) {
        return Err(format!("source not found: {}", source.display()));
    }
    let target = source
        .parent()
        .ok_or_else(|| "source has no parent directory".to_string())?
        .join(name);
    if source == target {
        return Ok(success_with_path("rename", &target));
    }
    if path_exists(&target) {
        return Err(format!("target already exists: {}", target.display()));
    }
    fs::rename(&source, &target)
        .map_err(|error| format!("rename {}: {error}", source.display()))?;
    Ok(success_with_path("rename", &target))
}

fn suggest_dirs(args: &[String]) -> Result<String, String> {
    let base = expand_home(Path::new(required(args, 1, "base")?));
    let prefix = args.get(2).map(String::as_str).unwrap_or_default();
    if !base.is_dir() {
        return Ok(success_with_suggestions(&[]));
    }

    let mut matches = Vec::new();
    for entry in fs::read_dir(&base).map_err(|error| format!("read {}: {error}", base.display()))? {
        let entry = entry.map_err(|error| format!("read directory entry: {error}"))?;
        let path = entry.path();
        if path.is_dir()
            && path
                .file_name()
                .and_then(|value| value.to_str())
                .is_some_and(|name| name.starts_with(prefix))
        {
            matches.push(path);
        }
    }
    matches.sort_by(|left, right| left.to_string_lossy().cmp(&right.to_string_lossy()));
    matches.truncate(12);
    Ok(success_with_suggestions(&matches))
}

fn which(args: &[String]) -> Result<String, String> {
    let program = required(args, 1, "program")?;
    let found = env::var_os("PATH")
        .into_iter()
        .flat_map(|path| env::split_paths(&path).collect::<Vec<_>>())
        .map(|directory| directory.join(program))
        .any(|candidate| candidate.is_file() && is_executable(&candidate));
    Ok(format!(
        "{{\"ok\":true,\"operation\":\"which\",\"found\":{found}}}"
    ))
}

fn properties(args: &[String]) -> Result<String, String> {
    let path = expand_home(Path::new(required(args, 1, "path")?));
    let metadata = fs::symlink_metadata(&path)
        .map_err(|error| format!("metadata {}: {error}", path.display()))?;
    let is_dir = metadata.is_dir();
    let item_type = if is_dir { "Pasta" } else { "Arquivo" };
    let size = if is_dir { 0 } else { metadata.len() };
    let modified = metadata
        .modified()
        .ok()
        .and_then(to_millis)
        .unwrap_or_default();
    let accessed = metadata
        .accessed()
        .ok()
        .and_then(to_millis)
        .unwrap_or_default();
    let permissions = permissions_string(&metadata);
    let contains = if is_dir {
        fs::read_dir(&path)
            .map(|entries| entries.filter_map(Result::ok).count())
            .unwrap_or_default()
    } else {
        0
    };
    Ok(format!(
        "{{\"ok\":true,\"operation\":\"properties\",\"type\":\"{}\",\"size\":{},\"modifiedMs\":{},\"accessedMs\":{},\"permissions\":\"{}\",\"contains\":{}}}",
        escape_json(item_type),
        size,
        modified,
        accessed,
        escape_json(&permissions),
        contains
    ))
}

fn create_desktop_shortcut(args: &[String]) -> Result<String, String> {
    let target = expand_home(Path::new(required(args, 1, "target")?));
    if !path_exists(&target) {
        return Err("target_not_found".to_string());
    }
    let desktop = desktop_directory();
    fs::create_dir_all(&desktop)
        .map_err(|error| format!("create {}: {error}", desktop.display()))?;
    let name = target
        .file_name()
        .and_then(|value| value.to_str())
        .ok_or_else(|| "target has no valid name".to_string())?;
    let mut destination = desktop.join(name);
    let mut index = 2;
    while path_exists(&destination) {
        destination = desktop.join(format!("{} {}", name, index));
        index += 1;
    }
    std::os::unix::fs::symlink(&target, &destination)
        .map_err(|error| format!("create shortcut {}: {error}", destination.display()))?;
    Ok(format!(
        "{{\"ok\":true,\"operation\":\"create-desktop-shortcut\",\"destination\":\"{}\"}}",
        escape_json(&destination.to_string_lossy())
    ))
}

fn network_mount_probe(args: &[String]) -> Result<String, String> {
    let root = expand_home(Path::new(required(args, 1, "root")?));
    if !root.is_dir() {
        return Err(format!("network root does not exist: {}", root.display()));
    }
    let first = fs::read_dir(&root)
        .map_err(|error| format!("read {}: {error}", root.display()))?
        .filter_map(Result::ok)
        .map(|entry| entry.path().to_string_lossy().into_owned())
        .next()
        .unwrap_or_default();
    let mount_count = fs::read_dir(&root)
        .map(|entries| entries.filter_map(Result::ok).count())
        .unwrap_or_default();
    Ok(format!(
        "{{\"ok\":true,\"operation\":\"network-mount-probe\",\"path\":\"{}\",\"mountCount\":{}}}",
        escape_json(&first),
        mount_count
    ))
}

fn network_mount(args: &[String]) -> Result<String, String> {
    let address = required(args, 1, "network address")?;
    if !address.contains("://") || address.contains('\0') {
        return Err("invalid network address".to_string());
    }
    let output = Command::new("gio")
        .args(["mount", address])
        .output()
        .map_err(|error| format!("start gio mount: {error}"))?;
    if !output.status.success() {
        let message = String::from_utf8_lossy(&output.stderr).trim().to_string();
        return Err(if message.is_empty() {
            "network mount failed".to_string()
        } else {
            message
        });
    }
    Ok(format!(
        "{{\"ok\":true,\"operation\":\"network-mount\",\"address\":\"{}\"}}",
        escape_json(address)
    ))
}

fn trash(args: &[String]) -> Result<String, String> {
    if args.len() < 4 {
        return Err("usage: utility trash <trash-files> <trash-info> <paths...>".to_string());
    }
    let files = expand_home(Path::new(&args[1]));
    let info = expand_home(Path::new(&args[2]));
    fs::create_dir_all(&files).map_err(|error| format!("create trash: {error}"))?;
    fs::create_dir_all(&info).map_err(|error| format!("create trash info: {error}"))?;
    let mut count = 0usize;
    for raw in &args[3..] {
        let source = expand_home(Path::new(raw));
        if !path_exists(&source) {
            return Err(format!("source not found: {}", source.display()));
        }
        let name = source
            .file_name()
            .and_then(|value| value.to_str())
            .ok_or_else(|| "source has no valid name".to_string())?;
        let destination = unique_target(&files, name);
        let info_path = info.join(format!(
            "{}.trashinfo",
            destination.file_name().unwrap().to_string_lossy()
        ));
        fs::rename(&source, &destination)
            .map_err(|error| format!("move to trash {}: {error}", source.display()))?;
        let content = format!(
            "[Trash Info]\nPath={}\nDeletionDate={}\n",
            encode_file_url(&source),
            unix_date()
        );
        fs::write(&info_path, content).map_err(|error| format!("write trash info: {error}"))?;
        count += 1;
    }
    Ok(format!(
        "{{\"ok\":true,\"operation\":\"trash\",\"count\":{}}}",
        count
    ))
}

fn restore_trash(args: &[String]) -> Result<String, String> {
    if args.len() < 4 {
        return Err(
            "usage: utility restore-trash <trash-info> <fallback-dir> <paths...>".to_string(),
        );
    }
    let info = expand_home(Path::new(&args[1]));
    let fallback = expand_home(Path::new(&args[2]));
    let mut count = 0usize;
    for raw in &args[3..] {
        let source = expand_home(Path::new(raw));
        if !path_exists(&source) {
            continue;
        }
        let name = source
            .file_name()
            .and_then(|value| value.to_str())
            .unwrap_or("restored");
        let info_path = info.join(format!("{name}.trashinfo"));
        let original = fs::read_to_string(&info_path)
            .ok()
            .and_then(|text| {
                text.lines()
                    .find_map(|line| line.strip_prefix("Path=").map(str::to_owned))
            })
            .map(|value| decode_file_url(&value))
            .filter(|value| !value.is_empty())
            .map(PathBuf::from)
            .unwrap_or_else(|| fallback.join(name));
        let parent = original.parent().unwrap_or(&fallback);
        fs::create_dir_all(parent).map_err(|error| format!("create restore directory: {error}"))?;
        let destination = if path_exists(&original) {
            unique_target(
                parent,
                original
                    .file_name()
                    .and_then(|v| v.to_str())
                    .unwrap_or(name),
            )
        } else {
            original
        };
        fs::rename(&source, &destination)
            .map_err(|error| format!("restore {}: {error}", source.display()))?;
        let _ = fs::remove_file(info_path);
        count += 1;
    }
    Ok(format!(
        "{{\"ok\":true,\"operation\":\"restore-trash\",\"count\":{}}}",
        count
    ))
}

fn empty_trash(args: &[String]) -> Result<String, String> {
    if args.len() < 3 {
        return Err("usage: utility empty-trash <trash-files> <trash-info>".to_string());
    }
    let mut count = 0usize;
    for root in [
        &expand_home(Path::new(&args[1])),
        &expand_home(Path::new(&args[2])),
    ] {
        if !root.is_dir() {
            continue;
        }
        for entry in fs::read_dir(root).map_err(|error| format!("read trash: {error}"))? {
            let path = entry
                .map_err(|error| format!("read trash entry: {error}"))?
                .path();
            if path.is_dir() {
                fs::remove_dir_all(path).map_err(|error| format!("empty trash: {error}"))?;
            } else {
                fs::remove_file(path).map_err(|error| format!("empty trash: {error}"))?;
            }
            count += 1;
        }
    }
    Ok(format!(
        "{{\"ok\":true,\"operation\":\"empty-trash\",\"count\":{}}}",
        count
    ))
}

fn unique_target(root: &Path, name: &str) -> PathBuf {
    let mut candidate = root.join(name);
    let mut index = 2;
    while path_exists(&candidate) {
        candidate = root.join(format!("{} {}", name, index));
        index += 1;
    }
    candidate
}

fn unix_date() -> String {
    let seconds = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_secs())
        .unwrap_or_default();
    let days = (seconds / 86_400) as i64;
    let day_seconds = seconds % 86_400;

    // Civil-date conversion from Unix days, using UTC and no locale or
    // subprocess dependency. This is the format required by the freedesktop
    // trash specification's DeletionDate field.
    let z = days + 719_468;
    let era = if z >= 0 { z } else { z - 146_096 } / 146_097;
    let doe = z - era * 146_097;
    let yoe = (doe - doe / 1_460 + doe / 36_524 - doe / 146_096) / 365;
    let mut year = yoe + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let month_part = (5 * doy + 2) / 153;
    let day = doy - (153 * month_part + 2) / 5 + 1;
    let month = month_part + if month_part < 10 { 3 } else { -9 };
    year += if month <= 2 { 1 } else { 0 };

    let hour = day_seconds / 3_600;
    let minute = (day_seconds % 3_600) / 60;
    let second = day_seconds % 60;
    format!("{year:04}-{month:02}-{day:02}T{hour:02}:{minute:02}:{second:02}")
}

fn encode_file_url(path: &Path) -> String {
    let mut encoded = String::from("file://");
    for byte in path.to_string_lossy().as_bytes() {
        if byte.is_ascii_alphanumeric() || matches!(byte, b'/' | b'-' | b'_' | b'.' | b'~') {
            encoded.push(*byte as char);
        } else {
            encoded.push_str(&format!("%{:02X}", byte));
        }
    }
    encoded
}

fn decode_file_url(value: &str) -> String {
    let value = value.strip_prefix("file://").unwrap_or(value);
    let bytes = value.as_bytes();
    let mut output = Vec::with_capacity(bytes.len());
    let mut index = 0;
    while index < bytes.len() {
        if bytes[index] == b'%' && index + 2 < bytes.len() {
            if let Ok(decoded) = u8::from_str_radix(&value[index + 1..index + 3], 16) {
                output.push(decoded);
                index += 3;
                continue;
            }
        }
        output.push(bytes[index]);
        index += 1;
    }
    String::from_utf8_lossy(&output).into_owned()
}

fn desktop_directory() -> PathBuf {
    if let Ok(value) = env::var("XDG_DESKTOP_DIR") {
        let path = expand_home(Path::new(value.trim()));
        if !path.as_os_str().is_empty() {
            return path;
        }
    }
    if let Ok(config) = fs::read_to_string(expand_home(Path::new("~/.config/user-dirs.dirs"))) {
        for line in config.lines() {
            if let Some(value) = line.strip_prefix("XDG_DESKTOP_DIR=") {
                let value = value.trim().trim_matches('"').replace("$HOME", &home_dir());
                if !value.is_empty() {
                    return PathBuf::from(value);
                }
            }
        }
    }
    let localized = expand_home(Path::new("~/Área de trabalho"));
    if localized.is_dir() {
        return localized;
    }
    expand_home(Path::new("~/Desktop"))
}

fn home_dir() -> String {
    env::var("HOME").unwrap_or_else(|_| String::from("/"))
}

fn expand_home(path: &Path) -> PathBuf {
    let value = path.to_string_lossy();
    if value == "~" {
        return PathBuf::from(home_dir());
    }
    if let Some(rest) = value.strip_prefix("~/") {
        return PathBuf::from(home_dir()).join(rest);
    }
    path.to_path_buf()
}

fn path_exists(path: &Path) -> bool {
    path.exists() || path.is_symlink()
}

fn safe_child_name<'a>(value: &'a str, label: &str) -> Result<&'a str, String> {
    let value = value.trim();
    let mut components = Path::new(value).components();
    if value.is_empty()
        || value == "."
        || value == ".."
        || Path::new(value).is_absolute()
        || !matches!(components.next(), Some(Component::Normal(_)))
        || components.next().is_some()
    {
        return Err(format!("invalid_{label}"));
    }
    Ok(value)
}

fn required<'a>(args: &'a [String], index: usize, label: &str) -> Result<&'a str, String> {
    args.get(index)
        .map(String::as_str)
        .ok_or_else(|| format!("missing {label}"))
}

fn is_executable(path: &Path) -> bool {
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        return fs::metadata(path)
            .map(|metadata| metadata.permissions().mode() & 0o111 != 0)
            .unwrap_or(false);
    }
    #[cfg(not(unix))]
    {
        true
    }
}

fn permissions_string(metadata: &fs::Metadata) -> String {
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        return format!("{:o}", metadata.permissions().mode() & 0o777);
    }
    #[cfg(not(unix))]
    {
        String::from("--")
    }
}

fn to_millis(time: SystemTime) -> Option<u128> {
    time.duration_since(UNIX_EPOCH)
        .ok()
        .map(|duration| duration.as_millis())
}

fn success_with_path(operation: &str, path: &Path) -> String {
    format!(
        "{{\"ok\":true,\"operation\":\"{}\",\"path\":\"{}\"}}",
        escape_json(operation),
        escape_json(&path.to_string_lossy())
    )
}

fn success_with_suggestions(paths: &[PathBuf]) -> String {
    let values = paths
        .iter()
        .map(|path| format!("\"{}\"", escape_json(&path.to_string_lossy())))
        .collect::<Vec<_>>()
        .join(",");
    format!(
        "{{\"ok\":true,\"operation\":\"suggest-dirs\",\"suggestions\":[{}]}}",
        values
    )
}

fn escape_json(value: &str) -> String {
    value
        .chars()
        .flat_map(|character| match character {
            '"' => "\\\"".chars().collect::<Vec<_>>(),
            '\\' => "\\\\".chars().collect::<Vec<_>>(),
            '\n' => "\\n".chars().collect::<Vec<_>>(),
            '\r' => "\\r".chars().collect::<Vec<_>>(),
            '\t' => "\\t".chars().collect::<Vec<_>>(),
            character if character.is_control() => {
                format!("\\u{:04x}", character as u32).chars().collect()
            }
            character => vec![character],
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    #[test]
    fn child_name_rejects_path_escape() {
        assert!(safe_child_name("../escape", "name").is_err());
        assert!(safe_child_name("nested/name", "name").is_err());
        assert!(safe_child_name("safe name", "name").is_ok());
    }

    #[test]
    fn suggestions_are_bounded_and_sorted() {
        let root = tempfile_path("suggestions");
        fs::create_dir_all(&root).unwrap();
        for index in (0..20).rev() {
            fs::create_dir(root.join(format!("item-{index:02}"))).unwrap();
        }
        let result = suggest_dirs(&[
            String::new(),
            root.to_string_lossy().into_owned(),
            "item-".into(),
        ])
        .unwrap();
        assert!(result.contains("item-00"));
        assert_eq!(result.matches("item-").count(), 12);
        fs::remove_dir_all(root).unwrap();
    }

    fn tempfile_path(name: &str) -> PathBuf {
        env::temp_dir().join(format!("astrea-utility-{name}-{}", std::process::id()))
    }
}
