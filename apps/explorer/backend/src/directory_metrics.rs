//! On-demand recursive metrics for Explorer Properties.
//!
//! The traversal is incremental and intentionally independent from ordinary
//! directory listing so Properties can request deep counts on demand.

use serde::Deserialize;
use serde_json::json;
use std::collections::HashSet;
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

const DEFAULT_PROGRESS_EVERY: u64 = 256;
const DEFAULT_PROGRESS_INTERVAL: Duration = Duration::from_millis(50);
const MAX_NATIVE_METRIC: u64 = i64::MAX as u64;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum MetricsState {
    Running,
    Success,
    Partial,
    Cancelled,
    Failed,
}

impl MetricsState {
    fn as_str(self) -> &'static str {
        match self {
            Self::Running => "running",
            Self::Success => "success",
            Self::Partial => "partial",
            Self::Cancelled => "cancelled",
            Self::Failed => "failed",
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MetricsSnapshot {
    pub state: MetricsState,
    pub bytes: u64,
    pub file_count: u64,
    pub directory_count: u64,
    pub unreadable_count: u64,
    pub scanned_entry_count: u64,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct MetricsResult {
    pub state: MetricsState,
    pub bytes: u64,
    pub file_count: u64,
    pub directory_count: u64,
    pub unreadable_count: u64,
    pub scanned_entry_count: u64,
    pub error_code: String,
    pub error_message: String,
}

#[derive(Clone, Debug)]
pub struct ScanOptions {
    pub cancellation_marker: Option<PathBuf>,
    pub unreadable_paths: HashSet<PathBuf>,
    pub cancel_after_entries: Option<u64>,
    pub progress_every: u64,
    pub progress_interval: Duration,
    pub(crate) mount_profile: Option<crate::entries::ListingProfileSnapshot>,
}

impl Default for ScanOptions {
    fn default() -> Self {
        Self {
            cancellation_marker: std::env::var_os("ASTREA_CANCEL_FILE").map(PathBuf::from),
            unreadable_paths: HashSet::new(),
            cancel_after_entries: None,
            progress_every: DEFAULT_PROGRESS_EVERY,
            progress_interval: DEFAULT_PROGRESS_INTERVAL,
            mount_profile: None,
        }
    }
}

impl ScanOptions {
    fn is_cancelled(&self, scanned_entry_count: u64) -> bool {
        self.cancellation_marker
            .as_ref()
            .is_some_and(|path| path.exists())
            || self
                .cancel_after_entries
                .is_some_and(|limit| scanned_entry_count >= limit)
    }
}

#[cfg(unix)]
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
struct DirectoryIdentity {
    device: u64,
    inode: u64,
}

#[cfg(not(unix))]
#[derive(Clone, Debug, Eq, Hash, PartialEq)]
struct DirectoryIdentity(PathBuf);

#[cfg(unix)]
fn directory_identity(metadata: &fs::Metadata) -> DirectoryIdentity {
    use std::os::unix::fs::MetadataExt;

    DirectoryIdentity {
        device: metadata.dev(),
        inode: metadata.ino(),
    }
}

#[cfg(not(unix))]
fn directory_identity(path: &Path) -> DirectoryIdentity {
    DirectoryIdentity(path.to_path_buf())
}

fn checked_add_bytes(current: u64, additional: u64) -> Result<u64, String> {
    let total = current
        .checked_add(additional)
        .ok_or_else(|| "logical size exceeds the supported range".to_string())?;
    if total > MAX_NATIVE_METRIC {
        return Err("logical size exceeds the supported range".to_string());
    }
    Ok(total)
}

fn empty_result(state: MetricsState) -> MetricsResult {
    MetricsResult {
        state,
        bytes: 0,
        file_count: 0,
        directory_count: 0,
        unreadable_count: 0,
        scanned_entry_count: 0,
        error_code: String::new(),
        error_message: String::new(),
    }
}

fn failed_result(code: &str, message: impl Into<String>) -> MetricsResult {
    let mut result = empty_result(MetricsState::Failed);
    result.error_code = code.to_string();
    result.error_message = message.into();
    result
}

fn snapshot(result: &MetricsResult, state: MetricsState) -> MetricsSnapshot {
    MetricsSnapshot {
        state,
        bytes: result.bytes,
        file_count: result.file_count,
        directory_count: result.directory_count,
        unreadable_count: result.unreadable_count,
        scanned_entry_count: result.scanned_entry_count,
    }
}

fn add_entry(result: &mut MetricsResult, size: u64, is_directory: bool) -> Result<(), String> {
    if is_directory {
        result.directory_count = result
            .directory_count
            .checked_add(1)
            .ok_or_else(|| "directory count exceeds the supported range".to_string())?;
    } else {
        result.file_count = result
            .file_count
            .checked_add(1)
            .ok_or_else(|| "file count exceeds the supported range".to_string())?;
        result.bytes = checked_add_bytes(result.bytes, size)?;
    }
    Ok(())
}

fn increment_scanned(result: &mut MetricsResult) -> Result<(), String> {
    result.scanned_entry_count = result
        .scanned_entry_count
        .checked_add(1)
        .ok_or_else(|| "scanned entry count exceeds the supported range".to_string())?;
    Ok(())
}

fn emit_progress_if_due<F>(
    result: &MetricsResult,
    options: &ScanOptions,
    last_emit: &mut Instant,
    last_count: &mut u64,
    emit: &mut F,
) where
    F: FnMut(MetricsSnapshot),
{
    let count_due = options.progress_every > 0
        && result.scanned_entry_count.saturating_sub(*last_count) >= options.progress_every;
    let time_due = last_emit.elapsed() >= options.progress_interval;
    if count_due || time_due {
        emit(snapshot(result, MetricsState::Running));
        *last_emit = Instant::now();
        *last_count = result.scanned_entry_count;
    }
}

fn root_path_metadata(path: &Path, options: &ScanOptions) -> Result<fs::Metadata, MetricsResult> {
    if options.unreadable_paths.contains(path) {
        return Err(failed_result(
            "root_unavailable",
            format!("could not inspect root {}", path.display()),
        ));
    }
    fs::symlink_metadata(path).map_err(|error| {
        failed_result(
            "root_unavailable",
            format!("could not inspect root {}: {error}", path.display()),
        )
    })
}

pub fn scan_paths<F>(paths: &[PathBuf], options: ScanOptions, mut emit: F) -> MetricsResult
where
    F: FnMut(MetricsSnapshot),
{
    if paths.is_empty() {
        return failed_result(
            "invalid_request",
            "directory metrics requires at least one path",
        );
    }

    let mut result = empty_result(MetricsState::Running);
    let mut visited_directories = HashSet::new();
    let mut directories = Vec::new();
    let mut root_directories = HashSet::new();
    let mut counted_file_roots = HashSet::new();
    let mut root_metadata = Vec::new();
    let mount_profile = options
        .mount_profile
        .as_ref()
        .cloned()
        .unwrap_or_else(crate::entries::capture_listing_profile);
    let mut last_emit = Instant::now() - options.progress_interval;
    let mut last_count = 0;

    for path in paths {
        if options.is_cancelled(result.scanned_entry_count) {
            result.state = MetricsState::Cancelled;
            result.error_code = "cancelled".to_string();
            result.error_message = "directory metrics cancelled".to_string();
            return result;
        }
        if mount_profile.path_uses_remote_listing(path) {
            result.unreadable_count = result.unreadable_count.saturating_add(1);
            result.error_code = "unsupported_remote".to_string();
            result.error_message =
                "recursive metrics are not supported for remote or virtual paths".to_string();
            continue;
        }
        let metadata = match root_path_metadata(path, &options) {
            Ok(metadata) => metadata,
            Err(error) => return error,
        };
        if let Err(message) = increment_scanned(&mut result) {
            return failed_result("count_overflow", message);
        }
        root_metadata.push((path.clone(), metadata));
    }

    let root_directory_paths = root_metadata
        .iter()
        .filter(|(_, metadata)| metadata.is_dir() && !metadata.file_type().is_symlink())
        .map(|(path, _)| path)
        .collect::<Vec<_>>();

    let effective_root_metadata = root_metadata
        .iter()
        .filter(|(path, metadata)| {
            !metadata.is_dir()
                || metadata.file_type().is_symlink()
                || !root_directory_paths.iter().any(|directory| {
                    path.as_path() != directory.as_path() && path.starts_with(directory)
                })
        })
        .collect::<Vec<_>>();

    for (path, metadata) in effective_root_metadata {
        if options.is_cancelled(result.scanned_entry_count) {
            result.state = MetricsState::Cancelled;
            result.error_code = "cancelled".to_string();
            result.error_message = "directory metrics cancelled".to_string();
            return result;
        }

        if metadata.file_type().is_symlink() || !metadata.is_dir() {
            if !counted_file_roots.insert(path.clone()) {
                continue;
            }
            if root_directory_paths.iter().any(|directory| {
                path.as_path() != directory.as_path() && path.starts_with(directory)
            }) {
                continue;
            }
            if let Err(message) = add_entry(&mut result, metadata.len(), false) {
                return failed_result("size_overflow", message);
            }
            emit_progress_if_due(
                &result,
                &options,
                &mut last_emit,
                &mut last_count,
                &mut emit,
            );
            continue;
        }

        #[cfg(unix)]
        let identity = directory_identity(&metadata);
        #[cfg(not(unix))]
        let identity = directory_identity(path);
        if !visited_directories.insert(identity) {
            continue;
        }
        root_directories.insert(path.clone());
        directories.push(path.clone());
    }

    while let Some(directory) = directories.pop() {
        if options.is_cancelled(result.scanned_entry_count) {
            result.state = MetricsState::Cancelled;
            result.error_code = "cancelled".to_string();
            result.error_message = "directory metrics cancelled".to_string();
            return result;
        }
        let entries = match fs::read_dir(&directory) {
            Ok(entries) => entries,
            Err(error) if root_directories.contains(&directory) => {
                return failed_result(
                    "root_unavailable",
                    format!("could not enumerate root {}: {error}", directory.display()),
                );
            }
            Err(error) => {
                result.unreadable_count = result.unreadable_count.saturating_add(1);
                result.error_code = "unreadable".to_string();
                result.error_message =
                    format!("could not enumerate {}: {error}", directory.display());
                continue;
            }
        };

        for entry in entries {
            if options.is_cancelled(result.scanned_entry_count) {
                result.state = MetricsState::Cancelled;
                result.error_code = "cancelled".to_string();
                result.error_message = "directory metrics cancelled".to_string();
                return result;
            }
            if increment_scanned(&mut result).is_err() {
                return failed_result(
                    "count_overflow",
                    "scanned entry count exceeds the supported range",
                );
            }
            let entry = match entry {
                Ok(entry) => entry,
                Err(error) => {
                    result.unreadable_count = result.unreadable_count.saturating_add(1);
                    result.error_code = "unreadable".to_string();
                    result.error_message = format!("could not read directory entry: {error}");
                    emit_progress_if_due(
                        &result,
                        &options,
                        &mut last_emit,
                        &mut last_count,
                        &mut emit,
                    );
                    continue;
                }
            };
            let path = entry.path();
            if options.unreadable_paths.contains(&path) {
                result.unreadable_count = result.unreadable_count.saturating_add(1);
                result.error_code = "unreadable".to_string();
                result.error_message = format!("could not inspect {}", path.display());
                emit_progress_if_due(
                    &result,
                    &options,
                    &mut last_emit,
                    &mut last_count,
                    &mut emit,
                );
                continue;
            }
            let metadata = match fs::symlink_metadata(&path) {
                Ok(metadata) => metadata,
                Err(error) => {
                    result.unreadable_count = result.unreadable_count.saturating_add(1);
                    result.error_code = "unreadable".to_string();
                    result.error_message = format!("could not inspect {}: {error}", path.display());
                    emit_progress_if_due(
                        &result,
                        &options,
                        &mut last_emit,
                        &mut last_count,
                        &mut emit,
                    );
                    continue;
                }
            };

            if !metadata.file_type().is_symlink()
                && metadata.is_dir()
                && mount_profile.path_uses_remote_listing(&path)
            {
                result.unreadable_count = result.unreadable_count.saturating_add(1);
                result.error_code = "unsupported_remote".to_string();
                result.error_message =
                    "recursive metrics are not supported for remote or virtual paths".to_string();
                if let Err(message) = add_entry(&mut result, 0, true) {
                    return failed_result("count_overflow", message);
                }
                emit_progress_if_due(
                    &result,
                    &options,
                    &mut last_emit,
                    &mut last_count,
                    &mut emit,
                );
                continue;
            }

            if metadata.file_type().is_symlink() || !metadata.is_dir() {
                if let Err(message) = add_entry(&mut result, metadata.len(), false) {
                    return failed_result("size_overflow", message);
                }
            } else {
                #[cfg(unix)]
                let identity = directory_identity(&metadata);
                #[cfg(not(unix))]
                let identity = directory_identity(&path);
                if visited_directories.insert(identity) {
                    if let Err(message) = add_entry(&mut result, 0, true) {
                        return failed_result("count_overflow", message);
                    }
                    directories.push(path);
                }
            }
            emit_progress_if_due(
                &result,
                &options,
                &mut last_emit,
                &mut last_count,
                &mut emit,
            );
        }
    }

    if result.error_code == "unsupported_remote" || result.unreadable_count > 0 {
        result.state = MetricsState::Partial;
    } else {
        result.state = MetricsState::Success;
        result.error_code.clear();
        result.error_message.clear();
    }
    result
}

#[derive(Debug, Deserialize)]
struct DirectoryMetricsRequest {
    paths: Vec<String>,
}

fn result_json(result: &MetricsResult) -> serde_json::Value {
    json!({
        "event": "result",
        "operation": "directory-metrics",
        "ok": result.state != MetricsState::Failed && result.state != MetricsState::Cancelled,
        "state": result.state.as_str(),
        "bytes": result.bytes,
        "fileCount": result.file_count,
        "directoryCount": result.directory_count,
        "unreadableCount": result.unreadable_count,
        "scannedEntryCount": result.scanned_entry_count,
        "errorCode": result.error_code,
        "errorMessage": result.error_message,
    })
}

pub fn run(args: &[String]) -> Result<(), String> {
    let encoded = args
        .iter()
        .position(|value| value == "--json")
        .and_then(|index| args.get(index + 1))
        .ok_or_else(|| "directory-metrics requires --json <request>".to_string())?;
    let request: DirectoryMetricsRequest = serde_json::from_str(encoded)
        .map_err(|error| format!("invalid directory metrics request: {error}"))?;
    if request.paths.is_empty() {
        return Err("directory-metrics requires at least one path".to_string());
    }
    let paths = request
        .paths
        .into_iter()
        .map(PathBuf::from)
        .collect::<Vec<_>>();
    let mut stdout = std::io::BufWriter::new(std::io::stdout().lock());
    let result = scan_paths(&paths, ScanOptions::default(), |progress| {
        let _ = writeln!(
            stdout,
            "{}",
            json!({
                "event": "progress",
                "operation": "directory-metrics",
                "state": progress.state.as_str(),
                "bytes": progress.bytes,
                "fileCount": progress.file_count,
                "directoryCount": progress.directory_count,
                "unreadableCount": progress.unreadable_count,
                "scannedEntryCount": progress.scanned_entry_count,
            })
        );
        let _ = stdout.flush();
    });
    writeln!(stdout, "{}", result_json(&result)).map_err(|error| error.to_string())?;
    stdout.flush().map_err(|error| error.to_string())?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::path::PathBuf;
    use std::time::Instant;

    fn test_root(name: &str) -> PathBuf {
        let root = std::env::temp_dir().join(format!(
            "astrea-directory-metrics-{name}-{}",
            std::process::id()
        ));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).expect("create test root");
        root
    }

    fn write_file(path: &std::path::Path, contents: &[u8]) {
        fs::write(path, contents).expect("write test file");
    }

    #[test]
    fn empty_directory_has_zero_bytes_and_no_content_directories() {
        let root = test_root("empty");
        let result = scan_paths(&[root.clone()], ScanOptions::default(), |_| {});
        assert_eq!(result.state, MetricsState::Success);
        assert_eq!(result.bytes, 0);
        assert_eq!(result.file_count, 0);
        assert_eq!(result.directory_count, 0);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn regular_file_root_uses_metadata_size_without_traversal() {
        let root = test_root("file-root");
        let file = root.join("file.bin");
        write_file(&file, b"12345");

        let result = scan_paths(&[file], ScanOptions::default(), |_| {});
        assert_eq!(result.state, MetricsState::Success);
        assert_eq!(result.bytes, 5);
        assert_eq!(result.file_count, 1);
        assert_eq!(result.directory_count, 0);
        assert_eq!(result.scanned_entry_count, 1);
        let _ = fs::remove_dir_all(root);
    }

    #[cfg(unix)]
    #[test]
    fn sparse_large_file_uses_metadata_size_without_reading_contents() {
        use std::fs::File;

        let root = test_root("sparse-file");
        let file_path = root.join("large-sparse.bin");
        let file = File::create(&file_path).expect("create sparse file");
        file.set_len(4 * 1024 * 1024 * 1024)
            .expect("grow sparse file");
        drop(file);

        let result = scan_paths(&[file_path], ScanOptions::default(), |_| {});
        assert_eq!(result.state, MetricsState::Success);
        assert_eq!(result.bytes, 4 * 1024 * 1024 * 1024);
        assert_eq!(result.file_count, 1);
        assert_eq!(result.scanned_entry_count, 1);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn nested_files_and_hidden_entries_are_counted() {
        let root = test_root("nested");
        fs::create_dir(root.join("nested")).expect("create nested directory");
        write_file(&root.join("visible"), b"1234");
        write_file(&root.join(".hidden"), b"12");
        write_file(&root.join("nested").join("child"), b"123456");

        let result = scan_paths(&[root.clone()], ScanOptions::default(), |_| {});
        assert_eq!(result.state, MetricsState::Success);
        assert_eq!(result.bytes, 12);
        assert_eq!(result.file_count, 3);
        assert_eq!(result.directory_count, 1);
        assert_eq!(result.unreadable_count, 0);
        let _ = fs::remove_dir_all(root);
    }

    #[cfg(unix)]
    #[test]
    fn symlinks_are_counted_as_links_and_never_followed() {
        use std::os::unix::fs::symlink;

        let root = test_root("symlinks");
        let outside = test_root("symlink-target");
        fs::create_dir(root.join("directory")).expect("create directory");
        write_file(&root.join("file"), b"file");
        write_file(&outside.join("outside"), b"outside");
        symlink(root.join("file"), root.join("file-link")).expect("link file");
        symlink(root.join("directory"), root.join("directory-link")).expect("link directory");
        symlink(&outside, root.join("outside-link")).expect("link outside directory");
        symlink(&root, root.join("cycle")).expect("link cycle");

        let link_size = ["file-link", "directory-link", "outside-link", "cycle"]
            .into_iter()
            .map(|name| {
                fs::symlink_metadata(root.join(name))
                    .expect("link metadata")
                    .len()
            })
            .sum::<u64>();
        let result = scan_paths(&[root.clone()], ScanOptions::default(), |_| {});
        assert_eq!(result.state, MetricsState::Success);
        assert_eq!(result.bytes, 4 + link_size);
        assert_eq!(result.file_count, 5);
        assert_eq!(result.directory_count, 1);
        let _ = fs::remove_dir_all(root);
        let _ = fs::remove_dir_all(outside);
    }

    #[test]
    fn duplicate_and_overlapping_roots_do_not_double_count_directories() {
        let root = test_root("overlap");
        let nested = root.join("nested");
        fs::create_dir(&nested).expect("create nested directory");
        write_file(&nested.join("child"), b"child");

        let result = scan_paths(&[root.clone(), nested], ScanOptions::default(), |_| {});
        assert_eq!(result.state, MetricsState::Success);
        assert_eq!(result.bytes, 5);
        assert_eq!(result.file_count, 1);
        assert_eq!(result.directory_count, 1);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn file_root_inside_directory_root_is_not_double_counted() {
        let root = test_root("overlap-file");
        let nested = root.join("nested");
        fs::create_dir(&nested).expect("create nested directory");
        let file = nested.join("child");
        write_file(&file, b"child");

        let result = scan_paths(&[root.clone(), file], ScanOptions::default(), |_| {});
        assert_eq!(result.state, MetricsState::Success);
        assert_eq!(result.bytes, 5);
        assert_eq!(result.file_count, 1);
        assert_eq!(result.directory_count, 1);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn two_nested_directories_count_only_descendant_directories() {
        let root = test_root("two-nested-directories");
        let first = root.join("first");
        let second = first.join("second");
        fs::create_dir(&first).expect("create first nested directory");
        fs::create_dir(&second).expect("create second nested directory");
        write_file(&second.join("file"), b"payload");

        let result = scan_paths(&[root.clone()], ScanOptions::default(), |_| {});

        assert_eq!(result.directory_count, 2);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn duplicate_directory_roots_are_counted_once_without_counting_roots() {
        let root = test_root("duplicate-roots");
        fs::create_dir(root.join("child")).expect("create child directory");

        let result = scan_paths(
            &[root.clone(), root.clone()],
            ScanOptions::default(),
            |_| {},
        );

        assert_eq!(result.directory_count, 1);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn nested_remote_mount_is_skipped_but_local_descendants_are_scanned() {
        let root = test_root("nested-remote-mount");
        let remote = root.join("nas");
        let local = root.join("local");
        fs::create_dir(&remote).expect("create remote mount point");
        fs::create_dir(&local).expect("create local directory");
        write_file(&remote.join("not-scanned"), b"remote");
        write_file(&local.join("kept"), b"local");
        let mountinfo = format!(
            "42 1 0:42 / {} rw,relatime - nfs server:/export rw\n",
            remote.display()
        );
        let mut options = ScanOptions::default();
        options.mount_profile = Some(crate::entries::listing_profile_from_mountinfo(&mountinfo));

        let result = scan_paths(&[root.clone()], options, |_| {});

        assert_eq!(result.state, MetricsState::Partial);
        assert_eq!(result.bytes, 5);
        assert_eq!(result.file_count, 1);
        assert_eq!(result.unreadable_count, 1);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn nested_remote_mount_filesystem_types_are_all_skipped() {
        let root = test_root("nested-remote-types");
        let local = root.join("local");
        fs::create_dir(&local).expect("create local directory");
        write_file(&local.join("kept"), b"local");
        let remote_types = [("nfs", "nfs"), ("cifs", "cifs"), ("sshfs", "sshfs")];
        let mut mountinfo = String::new();
        for (index, (name, fs_type)) in remote_types.iter().enumerate() {
            let mount = root.join(name);
            fs::create_dir(&mount).expect("create remote mount point");
            write_file(&mount.join("not-scanned"), b"remote");
            mountinfo.push_str(&format!(
                "{} 1 0:{} / {} rw,relatime - {} remote rw\n",
                42 + index,
                42 + index,
                mount.display(),
                fs_type
            ));
        }
        let mut options = ScanOptions::default();
        options.mount_profile = Some(crate::entries::listing_profile_from_mountinfo(&mountinfo));

        let result = scan_paths(&[root.clone()], options, |_| {});

        assert_eq!(result.state, MetricsState::Partial);
        assert_eq!(result.bytes, 5);
        assert_eq!(result.file_count, 1);
        assert_eq!(result.unreadable_count, 3);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn local_nested_mount_remains_scannable() {
        let root = test_root("nested-local-mount");
        let mounted = root.join("external");
        fs::create_dir(&mounted).expect("create local mount point");
        write_file(&mounted.join("kept"), b"local");
        let mountinfo = format!(
            "42 1 0:42 / {} rw,relatime - ext4 /dev/test rw\n",
            mounted.display()
        );
        let mut options = ScanOptions::default();
        options.mount_profile = Some(crate::entries::listing_profile_from_mountinfo(&mountinfo));

        let result = scan_paths(&[root.clone()], options, |_| {});

        assert_eq!(result.state, MetricsState::Success);
        assert_eq!(result.bytes, 5);
        assert_eq!(result.file_count, 1);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn direct_remote_root_is_rejected_without_traversal() {
        let root = test_root("direct-remote-root");
        write_file(&root.join("not-scanned"), b"remote");
        let mountinfo = format!(
            "42 1 0:42 / {} rw,relatime - cifs //server/share rw\n",
            root.display()
        );
        let mut options = ScanOptions::default();
        options.mount_profile = Some(crate::entries::listing_profile_from_mountinfo(&mountinfo));

        let result = scan_paths(&[root.clone()], options, |_| {});

        assert_eq!(result.state, MetricsState::Partial);
        assert_eq!(result.bytes, 0);
        assert_eq!(result.file_count, 0);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn multiple_roots_aggregate_files_and_directories() {
        let first = test_root("first");
        let second = test_root("second");
        write_file(&first.join("one"), b"one");
        fs::create_dir(second.join("nested")).expect("create nested directory");
        write_file(&second.join("nested").join("two"), b"two-two");

        let result = scan_paths(
            &[first.clone(), second.clone()],
            ScanOptions::default(),
            |_| {},
        );
        assert_eq!(result.bytes, 10);
        assert_eq!(result.file_count, 2);
        assert_eq!(result.directory_count, 1);
        let _ = fs::remove_dir_all(first);
        let _ = fs::remove_dir_all(second);
    }

    #[test]
    fn injected_unreadable_entry_returns_partial_and_continues() {
        let root = test_root("partial");
        write_file(&root.join("unreadable"), b"ignored");
        write_file(&root.join("readable"), b"kept");

        let mut options = ScanOptions::default();
        options.unreadable_paths.insert(root.join("unreadable"));
        let result = scan_paths(&[root.clone()], options, |_| {});
        assert_eq!(result.state, MetricsState::Partial);
        assert_eq!(result.bytes, 4);
        assert_eq!(result.file_count, 1);
        assert_eq!(result.unreadable_count, 1);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn missing_root_is_a_hard_failure() {
        let root = test_root("missing");
        let missing = root.join("missing");
        let result = scan_paths(&[missing], ScanOptions::default(), |_| {});
        assert_eq!(result.state, MetricsState::Failed);
        assert!(!result.error_message.is_empty());
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn cancellation_stops_incremental_scan() {
        let root = test_root("cancel");
        for index in 0..500 {
            write_file(&root.join(format!("file-{index}")), b"x");
        }
        let mut options = ScanOptions::default();
        options.cancel_after_entries = Some(10);
        let result = scan_paths(&[root.clone()], options, |_| {});
        assert_eq!(result.state, MetricsState::Cancelled);
        assert!(result.scanned_entry_count <= 11);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn checked_byte_accumulation_rejects_overflow() {
        assert_eq!(checked_add_bytes(4, 5).expect("sum"), 9);
        assert!(checked_add_bytes(i64::MAX as u64, 1).is_err());
        assert!(checked_add_bytes(u64::MAX, 1).is_err());
    }

    #[test]
    fn progress_is_coalesced_for_large_entry_count() {
        let root = test_root("progress");
        for index in 0..1000 {
            write_file(&root.join(format!("file-{index}")), b"x");
        }
        let mut options = ScanOptions::default();
        options.progress_every = 128;
        options.progress_interval = std::time::Duration::from_secs(60);
        let mut progress = Vec::new();
        let started = Instant::now();
        let result = scan_paths(&[root.clone()], options, |snapshot| progress.push(snapshot));
        assert_eq!(result.state, MetricsState::Success);
        assert!(started.elapsed().as_secs() < 5);
        assert!(progress.len() < 100);
        assert!(
            progress
                .iter()
                .all(|snapshot| snapshot.state == MetricsState::Running)
        );
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn ten_thousand_small_files_use_incremental_metrics() {
        let root = test_root("ten-thousand");
        for index in 0..10_000 {
            write_file(&root.join(format!("file-{index}")), b"x");
        }
        let mut progress_count = 0;
        let started = Instant::now();
        let result = scan_paths(&[root.clone()], ScanOptions::default(), |_| {
            progress_count += 1
        });
        eprintln!(
            "directory metrics fixture: elapsed_ms={} progress_events={} bytes={} files={}",
            started.elapsed().as_millis(),
            progress_count,
            result.bytes,
            result.file_count
        );
        assert_eq!(result.state, MetricsState::Success);
        assert_eq!(result.file_count, 10_000);
        assert_eq!(result.bytes, 10_000);
        assert!(progress_count < 10_000);
        let _ = fs::remove_dir_all(root);
    }
}
