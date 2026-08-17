use std::env;
use std::fs::{self, OpenOptions};
use std::path::{Component, Path, PathBuf};
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

#[cfg(unix)]
use std::os::unix::ffi::OsStrExt;
#[cfg(unix)]
use std::os::unix::ffi::OsStringExt;
#[cfg(unix)]
use std::os::unix::fs::PermissionsExt;

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
        "delete-permanently" => delete_permanently(args),
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

pub fn list_trash_entries_json() -> Result<String, String> {
    let mut locations = vec![home_trash_location()];
    for mount in mount_table() {
        if is_remote_filesystem(&mount.fs_type) {
            continue;
        }
        for location in candidate_secondary_locations(&mount) {
            if (location.files.is_dir() || location.info.is_dir())
                && !locations.iter().any(|root| root.files == location.files)
            {
                locations.push(location);
            }
        }
    }

    let mut entries = Vec::new();
    for location in locations {
        if !location.files.is_dir() {
            continue;
        }
        let location_id = location.files.to_string_lossy().into_owned();
        for entry in fs::read_dir(&location.files)
            .map_err(|error| format!("read trash files {}: {error}", location.files.display()))?
        {
            let entry = entry.map_err(|error| format!("read trash entry: {error}"))?;
            let path = entry.path();
            let name = entry.file_name().to_string_lossy().into_owned();
            let info_path = location.info.join(format!("{name}.trashinfo"));
            let metadata = if info_path.is_file() {
                read_trash_metadata(&info_path).ok()
            } else {
                None
            };
            let (original_path, deletion_date, orphan_state) = match metadata {
                Some((path_value, deletion_date)) => {
                    let orphan_state = if deletion_date.is_empty() {
                        "invalid-metadata"
                    } else {
                        "none"
                    };
                    (
                        decode_trash_path(&path_value, location.mount_root.as_deref())
                            .map(|path| path.to_string_lossy().into_owned())
                            .unwrap_or_default(),
                        deletion_date,
                        orphan_state,
                    )
                }
                None => (String::new(), String::new(), "orphan"),
            };
            let file_metadata = fs::symlink_metadata(&path)
                .map_err(|error| format!("metadata trash item {}: {error}", path.display()))?;
            let modified_ms = file_metadata
                .modified()
                .ok()
                .and_then(to_millis)
                .unwrap_or_default();
            let is_dir = file_metadata.is_dir() && !file_metadata.file_type().is_symlink();
            let item_id = format!("{location_id}:{name}");
            let mount_topdir = location
                .mount_root
                .as_ref()
                .map(|root| root.to_string_lossy().into_owned())
                .unwrap_or_default();
            entries.push(format!(
                "{{\"fileName\":\"{}\",\"filePath\":\"{}\",\"fileUrl\":\"{}\",\"fileIsDir\":{},\"fileExecutable\":false,\"fileHidden\":{},\"fileSize\":{},\"fileModified\":{},\"fileKind\":\"{}\",\"filePreviewUrl\":\"\",\"fileRemote\":false,\"fileMetadataLimited\":false,\"fileFilesystem\":\"{}\",\"trashItemId\":\"{}\",\"trashInfoPath\":\"{}\",\"trashLocationId\":\"{}\",\"trashOriginalPath\":\"{}\",\"trashDeletionDate\":\"{}\",\"trashMountTopdir\":\"{}\",\"trashAvailable\":true,\"trashOrphanState\":\"{}\"}}",
                escape_json(&name),
                escape_json(&path.to_string_lossy()),
                escape_json(&crate::json::file_url(&path)),
                is_dir,
                name.starts_with('.'),
                if is_dir { 0 } else { file_metadata.len() },
                modified_ms,
                escape_json(if orphan_state == "none" {
                    if is_dir { "Pasta" } else { "Arquivo" }
                } else {
                    "Orphan"
                }),
                escape_json(&location_id),
                escape_json(&item_id),
                escape_json(&info_path.to_string_lossy()),
                escape_json(&location_id),
                escape_json(&original_path),
                escape_json(&deletion_date),
                escape_json(&mount_topdir),
                orphan_state,
            ));
        }
    }
    entries.sort();
    Ok(format!("[{}]", entries.join(",")))
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

fn batch_item_json(path: &Path, target: Option<&Path>, status: &str, error: &str) -> String {
    format!(
        "{{\"path\":\"{}\",\"target\":\"{}\",\"status\":\"{}\",\"errorCode\":\"{}\",\"message\":\"{}\"}}",
        escape_json(&path.to_string_lossy()),
        escape_json(
            &target
                .map(|value| value.to_string_lossy().into_owned())
                .unwrap_or_default(),
        ),
        escape_json(status),
        if error.is_empty() {
            String::new()
        } else {
            String::from("operation_failed")
        },
        escape_json(error),
    )
}

fn batch_result_json(
    operation: &str,
    count: usize,
    items: &[String],
    first_error: Option<&str>,
) -> String {
    let ok = first_error.is_none();
    let state = if ok {
        "success"
    } else if count > 0 {
        "partial-success"
    } else {
        "failed"
    };
    format!(
        "{{\"ok\":{},\"operation\":\"{}\",\"count\":{},\"state\":\"{}\",\"items\":[{}]{} }}",
        ok,
        escape_json(operation),
        count,
        state,
        items.join(","),
        first_error
            .map(|error| format!(",\"error\":\"{}\"", escape_json(error)))
            .unwrap_or_default(),
    )
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
    let primary = TrashLocation {
        files: expand_home(Path::new(&args[1])),
        info: expand_home(Path::new(&args[2])),
        mount_root: None,
    };
    let mut items = Vec::new();
    let mut first_error = None;
    let mut planned = Vec::new();
    for raw in &args[3..] {
        let source = expand_home(Path::new(raw));
        if !path_exists(&source) {
            let error = format!("source not found: {}", source.display());
            first_error.get_or_insert(error.clone());
            items.push(batch_item_json(&source, None, "failed", &error));
            continue;
        }
        let name = match source.file_name().and_then(|value| value.to_str()) {
            Some(name) => name.to_string(),
            None => {
                let error = format!("source has no valid name: {}", source.display());
                first_error.get_or_insert(error.clone());
                items.push(batch_item_json(&source, None, "failed", &error));
                continue;
            }
        };
        let location = match trash_location_for(&source, &primary) {
            Ok(location) => location,
            Err(error) => {
                first_error.get_or_insert(error.clone());
                items.push(batch_item_json(&source, None, "failed", &error));
                continue;
            }
        };
        planned.push((source, name, location));
    }
    if let Some(error) = first_error {
        for (source, _, _) in planned {
            items.push(batch_item_json(
                &source,
                None,
                "not-attempted",
                "batch preflight failed",
            ));
        }
        return Ok(batch_result_json("trash", 0, &items, Some(&error)));
    }

    let mut count = 0usize;
    for (source, name, location) in planned {
        ensure_trash_dirs(&location)?;
        let (destination, info_path) = match reserve_trash_item(&location, &name) {
            Ok(value) => value,
            Err(error) => {
                first_error.get_or_insert(error.clone());
                items.push(batch_item_json(&source, None, "failed", &error));
                continue;
            }
        };
        let path_value = encode_trash_path(&source, location.mount_root.as_deref());
        let content = format!(
            "[Trash Info]\nPath={path_value}\nDeletionDate={}\n",
            unix_date()
        );
        if let Err(error) = fs::rename(&source, &destination) {
            let _ = fs::remove_file(&info_path);
            let message = format!("move to trash {}: {error}", source.display());
            first_error.get_or_insert(message.clone());
            items.push(batch_item_json(
                &source,
                Some(&destination),
                "failed",
                &message,
            ));
            continue;
        }
        if let Err(error) = fs::write(&info_path, content) {
            let _ = fs::rename(&destination, &source);
            let _ = fs::remove_file(&info_path);
            let message = format!("write trash info: {error}");
            first_error.get_or_insert(message.clone());
            items.push(batch_item_json(
                &source,
                Some(&destination),
                "failed",
                &message,
            ));
            continue;
        }
        count += 1;
        items.push(batch_item_json(&source, Some(&destination), "trashed", ""));
    }
    Ok(batch_result_json(
        "trash",
        count,
        &items,
        first_error.as_deref(),
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
    let mut items = Vec::new();
    let mut first_error = None;
    for raw in &args[3..] {
        let source = expand_home(Path::new(raw));
        if !path_exists(&source) {
            let error = format!("trash item not found: {}", source.display());
            first_error.get_or_insert(error.clone());
            items.push(batch_item_json(&source, None, "failed", &error));
            continue;
        }
        let name = source
            .file_name()
            .and_then(|value| value.to_str())
            .unwrap_or("restored");
        let metadata = match find_trash_metadata(&info, name) {
            Ok(Some(value)) => value,
            Ok(None) => {
                let error = format!("trash metadata missing for {name}");
                first_error.get_or_insert(error.clone());
                items.push(batch_item_json(&source, None, "failed", &error));
                continue;
            }
            Err(error) => {
                first_error.get_or_insert(error.clone());
                items.push(batch_item_json(&source, None, "failed", &error));
                continue;
            }
        };
        let (info_path, path_value, mount_root) = metadata;
        let original = match decode_trash_path(&path_value, mount_root.as_deref()) {
            Some(path) => path,
            None => {
                let error = format!("invalid original path for {name}");
                first_error.get_or_insert(error.clone());
                items.push(batch_item_json(&source, None, "failed", &error));
                continue;
            }
        };
        let parent = original.parent().unwrap_or(&fallback);
        if let Err(error) = fs::create_dir_all(parent) {
            let message = format!("create restore directory: {error}");
            first_error.get_or_insert(message.clone());
            items.push(batch_item_json(
                &source,
                Some(&original),
                "failed",
                &message,
            ));
            continue;
        }
        if path_exists(&original) {
            let error = format!("restore conflict: {}", original.display());
            first_error.get_or_insert(error.clone());
            items.push(batch_item_json(&source, Some(&original), "failed", &error));
            continue;
        }
        let destination = original;
        if let Err(error) = fs::rename(&source, &destination) {
            let message = format!("restore {}: {error}", source.display());
            first_error.get_or_insert(message.clone());
            items.push(batch_item_json(
                &source,
                Some(&destination),
                "failed",
                &message,
            ));
            continue;
        }
        let _ = fs::remove_file(info_path);
        count += 1;
        items.push(batch_item_json(&source, Some(&destination), "restored", ""));
    }
    Ok(batch_result_json(
        "restore-trash",
        count,
        &items,
        first_error.as_deref(),
    ))
}

fn empty_trash(args: &[String]) -> Result<String, String> {
    if args.len() < 3 {
        return Err("usage: utility empty-trash <trash-files> <trash-info>".to_string());
    }
    let primary = TrashLocation {
        files: expand_home(Path::new(&args[1])),
        info: expand_home(Path::new(&args[2])),
        mount_root: None,
    };
    let mut roots = vec![primary];
    for mount in mount_table() {
        if is_remote_filesystem(&mount.fs_type) {
            continue;
        }
        for location in candidate_secondary_locations(&mount) {
            if (location.files.is_dir() || location.info.is_dir())
                && !roots.iter().any(|root| root.files == location.files)
            {
                roots.push(location);
            }
        }
    }
    let mut count = 0usize;
    let mut items = Vec::new();
    let mut first_error = None;
    for root in roots {
        let logical_names: std::collections::HashSet<String> = if root.info.is_dir() {
            fs::read_dir(&root.info)
                .map_err(|error| format!("read trash info: {error}"))?
                .filter_map(Result::ok)
                .filter_map(|entry| {
                    let path = entry.path();
                    if path.extension().and_then(|value| value.to_str()) == Some("trashinfo") {
                        path.file_stem()
                            .map(|stem| stem.to_string_lossy().into_owned())
                    } else {
                        None
                    }
                })
                .collect()
        } else {
            std::collections::HashSet::new()
        };
        if root.files.is_dir() {
            let entries = match fs::read_dir(&root.files) {
                Ok(entries) => entries,
                Err(error) => {
                    let message = format!("read trash: {error}");
                    first_error.get_or_insert(message.clone());
                    items.push(batch_item_json(&root.files, None, "failed", &message));
                    continue;
                }
            };
            for entry in entries {
                let path = match entry {
                    Ok(entry) => entry.path(),
                    Err(error) => {
                        let message = format!("read trash entry: {error}");
                        first_error.get_or_insert(message.clone());
                        items.push(batch_item_json(&root.files, None, "failed", &message));
                        continue;
                    }
                };
                let name = path
                    .file_name()
                    .unwrap_or_default()
                    .to_string_lossy()
                    .into_owned();
                let logical = logical_names.contains(&name);
                match remove_path(&path) {
                    Ok(()) => {
                        if logical {
                            count += 1;
                        }
                        items.push(batch_item_json(&path, None, "deleted", ""));
                    }
                    Err(error) => {
                        first_error.get_or_insert(error.clone());
                        items.push(batch_item_json(&path, None, "failed", &error));
                    }
                }
            }
        }
        if root.info.is_dir() {
            let entries = match fs::read_dir(&root.info) {
                Ok(entries) => entries,
                Err(error) => {
                    let message = format!("read trash info: {error}");
                    first_error.get_or_insert(message.clone());
                    items.push(batch_item_json(&root.info, None, "failed", &message));
                    continue;
                }
            };
            for entry in entries {
                let path = match entry {
                    Ok(entry) => entry.path(),
                    Err(error) => {
                        let message = format!("read trash info entry: {error}");
                        first_error.get_or_insert(message.clone());
                        items.push(batch_item_json(&root.info, None, "failed", &message));
                        continue;
                    }
                };
                if let Err(error) = remove_path(&path) {
                    first_error.get_or_insert(error.clone());
                    items.push(batch_item_json(&path, None, "failed", &error));
                }
            }
        }
    }
    Ok(batch_result_json(
        "empty-trash",
        count,
        &items,
        first_error.as_deref(),
    ))
}

fn delete_permanently(args: &[String]) -> Result<String, String> {
    if args.len() < 2 {
        return Err("usage: utility delete-permanently <paths...>".to_string());
    }
    let metadata_index = args[1..]
        .iter()
        .position(|value| value == "--metadata")
        .map(|index| index + 1);
    let path_end = metadata_index.unwrap_or(args.len());
    let metadata_start = metadata_index.map(|index| index + 1);
    let mut count = 0usize;
    let mut items = Vec::new();
    let mut first_error = None;
    for (index, raw) in args[1..path_end].iter().enumerate() {
        let path = expand_home(Path::new(raw));
        if !path_exists(&path) {
            let error = format!("path not found: {}", path.display());
            first_error.get_or_insert(error.clone());
            items.push(batch_item_json(&path, None, "failed", &error));
            continue;
        }
        if let Err(error) = remove_path(&path) {
            first_error.get_or_insert(error.clone());
            items.push(batch_item_json(&path, None, "failed", &error));
            continue;
        }
        count += 1;
        let metadata_path = metadata_start
            .and_then(|start| args.get(start + index))
            .filter(|value| !value.is_empty())
            .map(|value| expand_home(Path::new(value)));
        if let Some(metadata_path) = metadata_path {
            if let Err(error) = fs::remove_file(&metadata_path) {
                let message = format!("remove trash metadata {}: {error}", metadata_path.display());
                first_error.get_or_insert(message.clone());
                items.push(batch_item_json(
                    &path,
                    Some(&metadata_path),
                    "deleted",
                    &message,
                ));
                continue;
            }
        }
        items.push(batch_item_json(&path, None, "deleted", ""));
    }
    Ok(batch_result_json(
        "delete-permanently",
        count,
        &items,
        first_error.as_deref(),
    ))
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct TrashLocation {
    files: PathBuf,
    info: PathBuf,
    mount_root: Option<PathBuf>,
}

#[derive(Clone, Debug)]
struct MountLocation {
    mount_point: PathBuf,
    fs_type: String,
}

fn ensure_trash_dirs(location: &TrashLocation) -> Result<(), String> {
    fs::create_dir_all(&location.files)
        .map_err(|error| format!("create trash files {}: {error}", location.files.display()))?;
    fs::create_dir_all(&location.info)
        .map_err(|error| format!("create trash info {}: {error}", location.info.display()))?;
    Ok(())
}

fn reserve_trash_item(location: &TrashLocation, name: &str) -> Result<(PathBuf, PathBuf), String> {
    for index in 1..10_000usize {
        let candidate_name = if index == 1 {
            name.to_string()
        } else {
            format!("{name} {index}")
        };
        let destination = location.files.join(&candidate_name);
        if path_exists(&destination) {
            continue;
        }
        let info_path = location.info.join(format!("{candidate_name}.trashinfo"));
        match OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&info_path)
        {
            Ok(_) => return Ok((destination, info_path)),
            Err(error) if error.kind() == std::io::ErrorKind::AlreadyExists => continue,
            Err(error) => return Err(format!("reserve trash metadata: {error}")),
        }
    }
    Err(format!("unable to reserve trash name for {name}"))
}

fn trash_location_for(source: &Path, primary: &TrashLocation) -> Result<TrashLocation, String> {
    let Some(mount) = owning_secondary_mount(source) else {
        return Ok(primary.clone());
    };
    let primary_mount = owning_secondary_mount(&primary.files);
    if primary_mount
        .as_ref()
        .is_some_and(|candidate| candidate.mount_point == mount.mount_point)
    {
        return Ok(primary.clone());
    }
    if is_remote_filesystem(&mount.fs_type) {
        return Err(format!(
            "trash unsupported on remote filesystem {}",
            mount.fs_type
        ));
    }
    secondary_location(&mount)
}

fn find_trash_metadata(
    primary_info: &Path,
    name: &str,
) -> Result<Option<(PathBuf, String, Option<PathBuf>)>, String> {
    if primary_info.is_file() {
        let expected = format!("{name}.trashinfo");
        if primary_info.file_name().and_then(|value| value.to_str()) != Some(expected.as_str()) {
            return Err(format!("trash metadata does not match {name}"));
        }
        let mount_root = primary_info.parent().and_then(|info_dir| {
            mount_table().into_iter().find_map(|mount| {
                candidate_secondary_locations(&mount)
                    .into_iter()
                    .find(|location| location.info == info_dir)
                    .and_then(|location| location.mount_root)
            })
        });
        return Ok(Some((
            primary_info.to_path_buf(),
            read_trash_path(primary_info)?,
            mount_root,
        )));
    }
    let primary_path = primary_info.join(format!("{name}.trashinfo"));
    if primary_path.is_file() {
        let path_value = read_trash_path(&primary_path)?;
        return Ok(Some((primary_path, path_value, None)));
    }
    for mount in mount_table() {
        for location in candidate_secondary_locations(&mount) {
            let info_path = location.info.join(format!("{name}.trashinfo"));
            if info_path.is_file() {
                return Ok(Some((
                    info_path.clone(),
                    read_trash_path(&info_path)?,
                    location.mount_root,
                )));
            }
        }
    }
    Ok(None)
}

fn read_trash_path(path: &Path) -> Result<String, String> {
    read_trash_metadata(path).map(|(path, _)| path)
}

fn read_trash_metadata(path: &Path) -> Result<(String, String), String> {
    let content = fs::read_to_string(path)
        .map_err(|error| format!("read trash metadata {}: {error}", path.display()))?;
    let path_value = content
        .lines()
        .find_map(|line| line.strip_prefix("Path=").map(str::to_string))
        .filter(|value| !value.is_empty())
        .ok_or_else(|| format!("trash metadata has no Path: {}", path.display()))?;
    let deletion_date = content
        .lines()
        .find_map(|line| line.strip_prefix("DeletionDate=").map(str::to_string))
        .unwrap_or_default();
    Ok((path_value, deletion_date))
}

fn encode_trash_path(path: &Path, mount_root: Option<&Path>) -> String {
    let value = mount_root
        .and_then(|root| path.strip_prefix(root).ok())
        .unwrap_or(path);
    encode_path_bytes(value)
}

fn decode_trash_path(value: &str, mount_root: Option<&Path>) -> Option<PathBuf> {
    let bytes = decode_path_bytes(value)?;
    #[cfg(unix)]
    let path = PathBuf::from(std::ffi::OsString::from_vec(bytes));
    #[cfg(not(unix))]
    let path = PathBuf::from(String::from_utf8(bytes).ok()?);
    if path.is_absolute() {
        Some(path)
    } else if path
        .components()
        .any(|component| component == Component::ParentDir)
    {
        None
    } else {
        mount_root.map(|root| root.join(path))
    }
}

#[cfg(unix)]
fn encode_path_bytes(path: &Path) -> String {
    path.as_os_str()
        .as_bytes()
        .iter()
        .map(|byte| {
            if byte.is_ascii_alphanumeric() || matches!(byte, b'/' | b'-' | b'_' | b'.' | b'~') {
                (*byte as char).to_string()
            } else {
                format!("%{byte:02X}")
            }
        })
        .collect()
}

#[cfg(not(unix))]
fn encode_path_bytes(path: &Path) -> String {
    path.to_string_lossy()
        .as_bytes()
        .iter()
        .map(|byte| {
            if byte.is_ascii_alphanumeric() || matches!(byte, b'/' | b'-' | b'_' | b'.' | b'~') {
                (*byte as char).to_string()
            } else {
                format!("%{byte:02X}")
            }
        })
        .collect()
}

fn decode_path_bytes(value: &str) -> Option<Vec<u8>> {
    let mut output = Vec::with_capacity(value.len());
    let bytes = value.as_bytes();
    let mut index = 0;
    while index < bytes.len() {
        if bytes[index] == b'%' && index + 2 < bytes.len() {
            output.push(u8::from_str_radix(&value[index + 1..index + 3], 16).ok()?);
            index += 3;
        } else {
            output.push(bytes[index]);
            index += 1;
        }
    }
    Some(output)
}

fn remove_path(path: &Path) -> Result<(), String> {
    if path.is_dir() && !path.is_symlink() {
        fs::remove_dir_all(path).map_err(|error| format!("empty trash: {error}"))
    } else {
        fs::remove_file(path).map_err(|error| format!("empty trash: {error}"))
    }
}

fn secondary_location(mount: &MountLocation) -> Result<TrashLocation, String> {
    candidate_secondary_locations(mount)
        .into_iter()
        .next()
        .ok_or_else(|| {
            format!(
                "no usable trash location on {}",
                mount.mount_point.display()
            )
        })
}

fn candidate_secondary_locations(mount: &MountLocation) -> Vec<TrashLocation> {
    let uid = current_uid();
    let mut locations = Vec::new();
    let shared = mount.mount_point.join(".Trash");
    if shared.is_dir() && !shared.is_symlink() && is_sticky_directory(&shared) {
        let root = shared.join(uid.to_string());
        locations.push(TrashLocation {
            files: root.join("files"),
            info: root.join("info"),
            mount_root: Some(mount.mount_point.clone()),
        });
    }
    let private_root = mount.mount_point.join(format!(".Trash-{uid}"));
    let private = TrashLocation {
        files: private_root.join("files"),
        info: private_root.join("info"),
        mount_root: Some(mount.mount_point.clone()),
    };
    if !locations
        .iter()
        .any(|location| location.files == private.files)
    {
        locations.push(private);
    }
    locations
}

fn home_trash_location() -> TrashLocation {
    let data_home = env::var("XDG_DATA_HOME")
        .ok()
        .filter(|path| path.starts_with('/'))
        .map(PathBuf::from)
        .unwrap_or_else(|| expand_home(Path::new("~/.local/share")));
    TrashLocation {
        files: data_home.join("Trash/files"),
        info: data_home.join("Trash/info"),
        mount_root: None,
    }
}

#[cfg(unix)]
fn is_sticky_directory(path: &Path) -> bool {
    fs::symlink_metadata(path)
        .map(|metadata| metadata.permissions().mode() & 0o1000 != 0)
        .unwrap_or(false)
}

#[cfg(not(unix))]
fn is_sticky_directory(_path: &Path) -> bool {
    true
}

#[cfg(unix)]
fn current_uid() -> u32 {
    unsafe extern "C" {
        fn getuid() -> u32;
    }
    unsafe { getuid() }
}

#[cfg(not(unix))]
fn current_uid() -> u32 {
    0
}

fn owning_secondary_mount(path: &Path) -> Option<MountLocation> {
    mount_table()
        .into_iter()
        .filter(|mount| mount.mount_point != Path::new("/") && path.starts_with(&mount.mount_point))
        .max_by_key(|mount| mount.mount_point.components().count())
}

fn is_remote_filesystem(fs_type: &str) -> bool {
    matches!(
        fs_type,
        "9p" | "afp" | "cifs" | "fuse.sshfs" | "ncp" | "nfs" | "nfs4" | "smbfs" | "sshfs"
    )
}

fn mount_table() -> Vec<MountLocation> {
    #[cfg(unix)]
    {
        let text = env::var("ASTREA_MOUNTINFO")
            .ok()
            .or_else(|| fs::read_to_string("/proc/self/mountinfo").ok())
            .unwrap_or_default();
        return text.lines().filter_map(parse_mountinfo_line).collect();
    }
    #[cfg(not(unix))]
    {
        Vec::new()
    }
}

#[cfg(unix)]
fn parse_mountinfo_line(line: &str) -> Option<MountLocation> {
    let mut fields = line.split(" - ");
    let left: Vec<&str> = fields.next()?.split_whitespace().collect();
    let right: Vec<&str> = fields.next()?.split_whitespace().collect();
    Some(MountLocation {
        mount_point: PathBuf::from(unescape_mountinfo(left.get(4)?)),
        fs_type: right.first()?.to_string(),
    })
}

#[cfg(unix)]
fn unescape_mountinfo(value: &str) -> String {
    value
        .replace("\\040", " ")
        .replace("\\011", "\t")
        .replace("\\012", "\n")
        .replace("\\134", "\\")
}

#[cfg(unix)]
fn unix_date() -> String {
    #[repr(C)]
    struct LocalTm {
        second: i32,
        minute: i32,
        hour: i32,
        day: i32,
        month: i32,
        year_since_1900: i32,
        _weekday: i32,
        _yearday: i32,
        _isdst: i32,
        _gmtoff: i64,
        _zone: *const std::ffi::c_char,
    }

    unsafe extern "C" {
        fn localtime_r(time: *const i64, result: *mut LocalTm) -> *mut LocalTm;
    }

    let seconds = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_secs() as i64)
        .unwrap_or_default();
    let mut local = LocalTm {
        second: 0,
        minute: 0,
        hour: 0,
        day: 1,
        month: 0,
        year_since_1900: 70,
        _weekday: 0,
        _yearday: 0,
        _isdst: 0,
        _gmtoff: 0,
        _zone: std::ptr::null(),
    };
    if unsafe { localtime_r(&seconds, &mut local) }.is_null() {
        return String::from("1970-01-01T00:00:00");
    }
    format!(
        "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}",
        local.year_since_1900 + 1900,
        local.month + 1,
        local.day,
        local.hour,
        local.minute,
        local.second
    )
}

#[cfg(not(unix))]
fn unix_date() -> String {
    String::from("1970-01-01T00:00:00")
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
    use std::sync::Mutex;

    static TRASH_TEST_LOCK: Mutex<()> = Mutex::new(());

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

    #[test]
    fn trash_metadata_is_freedesktop_path_bytes_and_restore_is_explicit() {
        let _guard = TRASH_TEST_LOCK.lock().unwrap();
        let root = tempfile_path("trash-metadata");
        let files = root.join("Trash/files");
        let info = root.join("Trash/info");
        let source_dir = root.join("source");
        fs::create_dir_all(&source_dir).unwrap();
        let source = source_dir.join("space é.txt");
        fs::write(&source, "payload").unwrap();

        trash(&vec![
            "trash".into(),
            files.to_string_lossy().into_owned(),
            info.to_string_lossy().into_owned(),
            source.to_string_lossy().into_owned(),
        ])
        .unwrap();

        let trashed = files.join("space é.txt");
        let metadata = fs::read_to_string(info.join("space é.txt.trashinfo")).unwrap();
        assert!(trashed.is_file());
        assert!(metadata.contains("Path=/"));
        assert!(!metadata.contains("Path=file://"));
        assert!(metadata.contains("%20"));
        assert!(metadata.contains("%C3%A9"));

        let conflict = source.clone();
        fs::write(&conflict, "new occupant").unwrap();
        let restore = restore_trash(&vec![
            "restore-trash".into(),
            info.to_string_lossy().into_owned(),
            source_dir.to_string_lossy().into_owned(),
            trashed.to_string_lossy().into_owned(),
        ]);
        let restore = restore.unwrap();
        assert!(restore.contains("\"ok\":false"));
        assert!(restore.contains("restore conflict"));
        assert!(trashed.is_file());
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn virtual_trash_lists_home_items_with_owning_metadata() {
        let _guard = TRASH_TEST_LOCK.lock().unwrap();
        let root = tempfile_path("trash-index");
        let data_home = root.join("data");
        let files = data_home.join("Trash/files");
        let info = data_home.join("Trash/info");
        let original = root.join("original/item.txt");
        fs::create_dir_all(&files).unwrap();
        fs::create_dir_all(&info).unwrap();
        fs::create_dir_all(original.parent().unwrap()).unwrap();
        fs::write(files.join("item.txt"), "payload").unwrap();
        fs::write(
            info.join("item.txt.trashinfo"),
            format!(
                "[Trash Info]\nPath={}\nDeletionDate=2026-08-17T12:00:00\n",
                encode_path_bytes(&original)
            ),
        )
        .unwrap();
        unsafe { env::set_var("XDG_DATA_HOME", &data_home) };

        let result = list_trash_entries_json().unwrap();

        assert!(result.contains("\"fileName\":\"item.txt\""));
        assert!(result.contains("\"trashOriginalPath\":\""));
        assert!(result.contains("\"trashInfoPath\":\""));
        assert!(result.contains("\"trashOrphanState\":\"none\""));
        unsafe { env::remove_var("XDG_DATA_HOME") };
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn empty_trash_reports_logical_items_not_physical_entries() {
        let _guard = TRASH_TEST_LOCK.lock().unwrap();
        let root = tempfile_path("trash-count");
        let files = root.join("files");
        let info = root.join("info");
        fs::create_dir_all(&files).unwrap();
        fs::create_dir_all(&info).unwrap();
        fs::write(files.join("one"), "1").unwrap();
        fs::write(files.join("orphan"), "2").unwrap();
        fs::write(info.join("one.trashinfo"), "[Trash Info]\n").unwrap();

        let result = empty_trash(&vec![
            "empty-trash".into(),
            files.to_string_lossy().into_owned(),
            info.to_string_lossy().into_owned(),
        ])
        .unwrap();
        assert!(result.contains("\"count\":1"));
        assert!(result.contains("\"items\":["));
        assert_eq!(fs::read_dir(&files).unwrap().count(), 0);
        assert_eq!(fs::read_dir(&info).unwrap().count(), 0);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn restore_rejects_relative_metadata_that_escapes_mount_root() {
        let root = tempfile_path("trash-path-escape");
        assert!(decode_trash_path("../../outside", Some(&root)).is_none());
        let _ = fs::remove_dir_all(root);
    }

    #[cfg(unix)]
    #[test]
    fn trash_uses_a_secondary_filesystem_trash_location() {
        let _guard = TRASH_TEST_LOCK.lock().unwrap();
        let root = tempfile_path("trash-secondary");
        let volume = root.join("volume");
        let primary = root.join("primary/Trash");
        let source = volume.join("item.txt");
        fs::create_dir_all(&volume).unwrap();
        fs::create_dir_all(&primary).unwrap();
        fs::write(&source, "payload").unwrap();
        let mountinfo = format!(
            "42 1 0:42 / {} rw,relatime - ext4 /dev/test rw\n",
            volume.display()
        );
        unsafe { env::set_var("ASTREA_MOUNTINFO", mountinfo) };

        trash(&vec![
            "trash".into(),
            primary.join("files").to_string_lossy().into_owned(),
            primary.join("info").to_string_lossy().into_owned(),
            source.to_string_lossy().into_owned(),
        ])
        .unwrap();

        let secondary = volume.join(format!(".Trash-{}/files/item.txt", current_uid()));
        assert!(secondary.is_file());
        assert!(!source.exists());
        unsafe { env::remove_var("ASTREA_MOUNTINFO") };
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn permanent_delete_removes_trash_items_without_metadata_lookup() {
        let root = tempfile_path("permanent-delete");
        fs::create_dir_all(&root).unwrap();
        let item = root.join("folder");
        fs::create_dir_all(&item).unwrap();
        fs::write(item.join("child"), "payload").unwrap();

        let result = delete_permanently(&vec![
            "delete-permanently".into(),
            item.to_string_lossy().into_owned(),
        ])
        .unwrap();
        assert!(result.contains("\"count\":1"));
        assert!(!item.exists());
        let _ = fs::remove_dir_all(root);
    }

    fn tempfile_path(name: &str) -> PathBuf {
        env::temp_dir().join(format!("astrea-utility-{name}-{}", std::process::id()))
    }
}
