use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fmt;
use std::path::{Path, PathBuf};
use std::process::Stdio;
use std::time::Duration;
use tokio::fs as async_fs;
use tokio::io::{AsyncRead, AsyncReadExt};
use tokio::process::Command;
use tokio::sync::watch;
use tokio::time::sleep;
use zvariant::OwnedValue;

pub const RESULT_PREFIX: &str = "__ASTREA_FILE_DIALOG__";
const MAX_DIALOG_OUTPUT_BYTES: usize = 1024 * 1024;
const MAX_RESULT_FILE_BYTES: u64 = 1024 * 1024;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PortalError {
    message: String,
}

impl PortalError {
    fn new(message: impl Into<String>) -> Self {
        Self {
            message: message.into(),
        }
    }
}

impl fmt::Display for PortalError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str(&self.message)
    }
}

impl std::error::Error for PortalError {}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum DialogMode {
    OpenFile,
    SaveFile,
    SelectFolder,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FilterPatternKind {
    Glob,
    Mime,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FilterPattern {
    pub kind: FilterPatternKind,
    pub value: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SerializedFilter {
    pub label: String,
    pub patterns: Vec<FilterPattern>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct DialogOptions {
    pub mode: DialogMode,
    pub title: String,
    pub start_folder: String,
    pub accept_label: String,
    pub current_name: String,
    pub filters: Vec<String>,
    pub multiple: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DialogRunConfig {
    pub explorer_bin: PathBuf,
    pub timeout: Duration,
}

pub type PortalOptions = HashMap<String, OwnedValue>;

#[derive(Debug, Clone, Default, PartialEq, Eq, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct PortalSelection {
    #[serde(default)]
    pub accepted: bool,
    #[serde(default)]
    pub file_path: Option<String>,
    #[serde(default)]
    pub file_url: Option<String>,
    #[serde(default)]
    pub files: Vec<SelectionFile>,
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct SelectionFile {
    pub file_path: String,
    #[serde(default)]
    pub file_url: Option<String>,
    #[serde(default)]
    pub file_name: Option<String>,
    #[serde(default)]
    pub file_is_dir: Option<bool>,
}

pub fn decode_null_terminated_bytes(value: &[u8]) -> String {
    let end = value
        .iter()
        .position(|byte| *byte == 0)
        .unwrap_or(value.len());
    String::from_utf8_lossy(&value[..end]).into_owned()
}

pub fn open_mode(directory: bool, filters: &[SerializedFilter]) -> DialogMode {
    if directory && !filters_include_file_types(filters) {
        DialogMode::SelectFolder
    } else {
        DialogMode::OpenFile
    }
}

pub fn filters_include_file_types(filters: &[SerializedFilter]) -> bool {
    filters.iter().any(|filter| {
        filter.patterns.iter().any(|pattern| {
            let value = pattern.value.trim();
            if value.is_empty() {
                return false;
            }
            matches!(value, "*" | "*.*")
                || value.starts_with("*.")
                || (value.contains('/')
                    && value != "inode/directory"
                    && value != "application/x-directory")
        })
    })
}

pub fn format_filters(filters: &[SerializedFilter]) -> Vec<String> {
    filters
        .iter()
        .filter_map(|filter| {
            let mut patterns = Vec::new();
            for pattern in &filter.patterns {
                match pattern.kind {
                    FilterPatternKind::Glob => push_unique(&mut patterns, pattern.value.clone()),
                    FilterPatternKind::Mime => {
                        for extension in extensions_for_mime(&pattern.value) {
                            push_unique(&mut patterns, format!("*{extension}"));
                        }
                    }
                }
            }

            if patterns.is_empty() {
                None
            } else {
                Some(format!("{} ({})", filter.label, patterns.join(" ")))
            }
        })
        .collect()
}

fn push_unique(values: &mut Vec<String>, value: String) {
    if !values.iter().any(|existing| existing == &value) {
        values.push(value);
    }
}

fn extensions_for_mime(mime: &str) -> &'static [&'static str] {
    match mime {
        "image/png" => &[".png"],
        "image/jpeg" => &[".jpg", ".jpeg", ".jpe", ".jfif"],
        "image/webp" => &[".webp"],
        "image/gif" => &[".gif"],
        "image/bmp" => &[".bmp"],
        "image/tiff" => &[".tif", ".tiff"],
        "image/svg+xml" => &[".svg", ".svgz"],
        "text/plain" => &[".txt", ".text"],
        "application/pdf" => &[".pdf"],
        "application/zip" => &[".zip"],
        "application/gzip" => &[".gz"],
        "application/x-tar" => &[".tar"],
        _ => &[],
    }
}

pub fn sanitize_save_file_name(value: &str) -> Result<String, PortalError> {
    let name = value.trim();
    if name.is_empty() {
        return Ok(String::new());
    }
    if name == "." || name == ".." {
        return Err(PortalError::new("invalid save file name"));
    }
    if name.starts_with('/') || name.contains('/') || name.contains('\\') {
        return Err(PortalError::new("invalid save file name"));
    }
    if name
        .chars()
        .any(|character| character == '\0' || character.is_control())
    {
        return Err(PortalError::new("invalid save file name"));
    }
    Ok(name.to_string())
}

pub fn file_uri(path: &str) -> String {
    let encoded = path
        .split('/')
        .map(urlencoding::encode)
        .collect::<Vec<_>>()
        .join("/");
    format!("file://{encoded}")
}

pub fn uris_from_selection(selection: &PortalSelection) -> Result<Vec<String>, PortalError> {
    let mut uris = Vec::new();
    for file in &selection.files {
        let path = file.file_path.trim();
        if !path.is_empty() {
            uris.push(file_uri(path));
        }
    }

    if uris.is_empty() {
        let path = selection
            .file_path
            .as_deref()
            .unwrap_or_default()
            .trim()
            .to_string();
        if path.is_empty() {
            return Err(PortalError::new("selection did not include a file path"));
        }
        uris.push(file_uri(&path));
    }

    Ok(uris)
}

pub fn uris_for_save_files(
    folder_selection: &PortalSelection,
    files: &[String],
) -> Result<Vec<String>, PortalError> {
    let folder = folder_selection
        .file_path
        .as_deref()
        .unwrap_or_default()
        .trim();
    if folder.is_empty() {
        return Err(PortalError::new(
            "folder selection did not include a file path",
        ));
    }

    let mut uris = Vec::new();
    for raw_name in files {
        let name = sanitize_save_file_name(raw_name)?;
        if !name.is_empty() {
            uris.push(file_uri(&format!("{folder}/{name}")));
        }
    }
    Ok(uris)
}

pub fn parse_result_from_text(text: &str) -> Result<Option<PortalSelection>, PortalError> {
    for line in text.lines().rev() {
        if let Some(index) = line.find(RESULT_PREFIX) {
            let payload = &line[index + RESULT_PREFIX.len()..];
            let selection = serde_json::from_str(payload.trim())
                .map_err(|err| PortalError::new(format!("parse dialog result: {err}")))?;
            return Ok(Some(selection));
        }
    }
    Ok(None)
}

pub fn default_run_config() -> DialogRunConfig {
    DialogRunConfig {
        explorer_bin: std::env::var_os("ASTREA_EXPLORER_BIN")
            .map(PathBuf::from)
            .unwrap_or_else(|| PathBuf::from("astrea-explorer")),
        timeout: Duration::from_secs(300),
    }
}

pub async fn run_dialog(
    mode: DialogMode,
    title: &str,
    options: &PortalOptions,
    cancellation: watch::Receiver<bool>,
) -> Result<PortalSelection, PortalError> {
    run_dialog_with_config(mode, title, options, &default_run_config(), cancellation).await
}

pub async fn run_dialog_with_config(
    mode: DialogMode,
    title: &str,
    options: &PortalOptions,
    config: &DialogRunConfig,
    mut cancellation: watch::Receiver<bool>,
) -> Result<PortalSelection, PortalError> {
    let result_file = tempfile::Builder::new()
        .prefix("astrea_file_dialog_result_")
        .suffix(".json")
        .tempfile_in("/tmp")
        .map_err(|err| PortalError::new(format!("create dialog result file: {err}")))?;
    let result_path = result_file.path().to_path_buf();
    let dialog_options = dialog_options_from_request(mode, title, options);
    let dialog_options_json = serde_json::to_string(&dialog_options)
        .map_err(|err| PortalError::new(format!("serialize dialog options: {err}")))?;

    let mut child = Command::new(&config.explorer_bin)
        .kill_on_drop(true)
        .arg("--portal")
        .env("ASTREA_FILE_DIALOG_OPTIONS", &dialog_options_json)
        .env("ASTREA_FILE_DIALOG_RESULT_FILE", &result_path)
        .env("BENCH_FILE_DIALOG_OPTIONS", &dialog_options_json)
        .env("BENCH_FILE_DIALOG_RESULT_FILE", &result_path)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|err| PortalError::new(format!("start portal dialog: {err}")))?;

    if *cancellation.borrow() {
        return Err(PortalError::new("dialog cancelled"));
    }

    let stdout_reader = tokio::spawn(read_bounded_output(child.stdout.take(), "stdout"));
    let stderr_reader = tokio::spawn(read_bounded_output(child.stderr.take(), "stderr"));
    let mut wait_task = tokio::spawn(async move {
        child
            .wait()
            .await
            .map_err(|err| PortalError::new(format!("wait for portal dialog: {err}")))
    });
    let status = tokio::select! {
        result = &mut wait_task => result
            .map_err(|err| PortalError::new(format!("join dialog process waiter: {err}")))??,
        _ = sleep(config.timeout) => {
            wait_task.abort();
            stdout_reader.abort();
            stderr_reader.abort();
            return Err(PortalError::new("dialog timed out"));
        }
        changed = cancellation.changed() => {
            if changed.is_err() || *cancellation.borrow() {
                wait_task.abort();
                stdout_reader.abort();
                stderr_reader.abort();
                return Err(PortalError::new("dialog cancelled"));
            }
            return Err(PortalError::new("dialog cancellation state failed"));
        }
    };

    let stdout = stdout_reader
        .await
        .map_err(|err| PortalError::new(format!("join dialog stdout reader: {err}")))??;
    let stderr = stderr_reader
        .await
        .map_err(|err| PortalError::new(format!("join dialog stderr reader: {err}")))??;

    if !status.success() {
        return Err(PortalError::new(format!(
            "portal dialog exited with status {status}"
        )));
    }

    if let Some(selection) = read_result_file(&result_path).await? {
        return Ok(selection);
    }
    let output = format!("{stdout}\n{stderr}");
    Ok(parse_result_from_text(&output)?.unwrap_or_default())
}

async fn read_bounded_output<R>(
    reader: Option<R>,
    stream_name: &'static str,
) -> Result<String, PortalError>
where
    R: AsyncRead + Send + Unpin + 'static,
{
    let Some(mut reader) = reader else {
        return Ok(String::new());
    };
    let mut output = Vec::new();
    let mut buffer = [0_u8; 8192];
    let mut total_bytes = 0_usize;
    loop {
        let count = reader
            .read(&mut buffer)
            .await
            .map_err(|err| PortalError::new(format!("read dialog {stream_name}: {err}")))?;
        if count == 0 {
            break;
        }
        total_bytes = total_bytes.saturating_add(count);
        if output.len() < MAX_DIALOG_OUTPUT_BYTES {
            let remaining = MAX_DIALOG_OUTPUT_BYTES - output.len();
            output.extend_from_slice(&buffer[..count.min(remaining)]);
        }
    }
    if total_bytes > MAX_DIALOG_OUTPUT_BYTES {
        return Err(PortalError::new(format!(
            "dialog {stream_name} output exceeded limit"
        )));
    }
    Ok(String::from_utf8_lossy(&output).into_owned())
}

async fn read_result_file(path: &Path) -> Result<Option<PortalSelection>, PortalError> {
    let metadata = match async_fs::metadata(path).await {
        Ok(metadata) => metadata,
        Err(err) if err.kind() == std::io::ErrorKind::NotFound => return Ok(None),
        Err(err) => {
            return Err(PortalError::new(format!(
                "read dialog result metadata: {err}"
            )));
        }
    };
    if metadata.len() == 0 {
        return Ok(None);
    }
    if metadata.len() > MAX_RESULT_FILE_BYTES {
        return Err(PortalError::new("dialog result file exceeded limit"));
    }
    let text = async_fs::read_to_string(path)
        .await
        .map_err(|err| PortalError::new(format!("read dialog result file: {err}")))?;
    let selection = serde_json::from_str(&text)
        .map_err(|err| PortalError::new(format!("parse dialog result file: {err}")))?;
    Ok(Some(selection))
}

pub fn dialog_options_from_request(
    requested_mode: DialogMode,
    title: &str,
    options: &PortalOptions,
) -> DialogOptions {
    let filters = filters_from_options(options);
    let mode = if requested_mode == DialogMode::OpenFile {
        open_mode(option_bool(options, "directory"), &filters)
    } else {
        requested_mode
    };

    let mut current_name = option_string(options, "current_name").unwrap_or_default();
    let mut start_folder = option_bytes(options, "current_folder")
        .map(|bytes| decode_null_terminated_bytes(&bytes))
        .filter(|folder| !folder.is_empty())
        .unwrap_or_else(home_dir);

    if let Some(current_file) = option_bytes(options, "current_file")
        .map(|bytes| decode_null_terminated_bytes(&bytes))
        .filter(|file| !file.is_empty())
    {
        let current_path = Path::new(&current_file);
        if current_path.is_dir() {
            start_folder = current_file;
        } else {
            if let Some(parent) = current_path.parent().and_then(Path::to_str) {
                start_folder = parent.to_string();
            }
            if current_name.is_empty() {
                if let Some(name) = current_path.file_name().and_then(|name| name.to_str()) {
                    current_name = name.to_string();
                }
            }
        }
    }

    DialogOptions {
        mode,
        title: title.to_string(),
        start_folder,
        accept_label: option_string(options, "accept_label").unwrap_or_default(),
        current_name,
        filters: format_filters(&filters),
        multiple: option_bool(options, "multiple"),
    }
}

pub fn save_file_names_from_options(options: &PortalOptions) -> Vec<String> {
    option_file_name_bytes(options, "files")
        .unwrap_or_default()
        .into_iter()
        .map(|bytes| decode_null_terminated_bytes(&bytes))
        .collect()
}

fn filters_from_options(options: &PortalOptions) -> Vec<SerializedFilter> {
    option_filters(options, "filters")
        .unwrap_or_default()
        .into_iter()
        .map(|(label, items)| SerializedFilter {
            label,
            patterns: items
                .into_iter()
                .filter_map(|(kind, value)| {
                    let kind = match kind {
                        0 => FilterPatternKind::Glob,
                        1 => FilterPatternKind::Mime,
                        _ => return None,
                    };
                    Some(FilterPattern { kind, value })
                })
                .collect(),
        })
        .collect()
}

fn option_bool(options: &PortalOptions, key: &str) -> bool {
    options
        .get(key)
        .and_then(|value| bool::try_from(value).ok())
        .unwrap_or(false)
}

fn option_string(options: &PortalOptions, key: &str) -> Option<String> {
    options
        .get(key)
        .and_then(|value| <&str>::try_from(value).ok())
        .map(ToOwned::to_owned)
}

fn option_bytes(options: &PortalOptions, key: &str) -> Option<Vec<u8>> {
    options
        .get(key)
        .cloned()
        .and_then(|value| Vec::<u8>::try_from(value).ok())
}

fn option_filters(options: &PortalOptions, key: &str) -> Option<Vec<(String, Vec<(u32, String)>)>> {
    options
        .get(key)
        .cloned()
        .and_then(|value| Vec::<(String, Vec<(u32, String)>)>::try_from(value).ok())
}

fn option_file_name_bytes(options: &PortalOptions, key: &str) -> Option<Vec<Vec<u8>>> {
    options
        .get(key)
        .cloned()
        .and_then(|value| Vec::<Vec<u8>>::try_from(value).ok())
}

fn home_dir() -> String {
    std::env::var("HOME").unwrap_or_else(|_| "/".to_string())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;
    use std::fs;
    use std::os::unix::fs::PermissionsExt;
    use std::time::Duration;
    use zvariant::{OwnedValue, Value};

    fn owned<T>(value: T) -> OwnedValue
    where
        Value<'static>: From<T>,
    {
        Value::from(value).try_into().unwrap()
    }

    #[test]
    fn null_terminated_bytes_decode_to_path_text() {
        assert_eq!(
            decode_null_terminated_bytes(&[b'/', b't', b'm', b'p', 0, b'x']),
            "/tmp"
        );
    }

    #[test]
    fn directory_option_with_file_filters_stays_open_file() {
        let filters = vec![SerializedFilter {
            label: "Images".to_string(),
            patterns: vec![FilterPattern {
                kind: FilterPatternKind::Glob,
                value: "*.[pP][nN][gG]".to_string(),
            }],
        }];

        assert_eq!(open_mode(true, &filters), DialogMode::OpenFile);
    }

    #[test]
    fn save_files_reject_path_traversal_names() {
        assert!(sanitize_save_file_name("../escape.txt").is_err());
    }

    #[test]
    fn selection_result_encodes_file_uris() {
        let result = PortalSelection {
            accepted: true,
            file_path: Some("/home/astrea/Desktop/a file.txt".to_string()),
            file_url: None,
            files: Vec::new(),
        };

        assert_eq!(
            uris_from_selection(&result).unwrap(),
            vec!["file:///home/astrea/Desktop/a%20file.txt"]
        );
    }

    #[test]
    fn dialog_options_from_dbus_options_preserve_current_file_and_filters() {
        let mut options: HashMap<String, OwnedValue> = HashMap::new();
        options.insert("directory".to_string(), owned(true));
        options.insert(
            "current_file".to_string(),
            owned(b"/home/astrea/Pictures/in.png\0".to_vec()),
        );
        options.insert(
            "filters".to_string(),
            owned(vec![(
                "Images".to_string(),
                vec![(0u32, "*.[pP][nN][gG]".to_string())],
            )]),
        );

        let dialog = dialog_options_from_request(DialogMode::OpenFile, "Select Image", &options);

        assert_eq!(dialog.mode, DialogMode::OpenFile);
        assert_eq!(dialog.start_folder, "/home/astrea/Pictures");
        assert_eq!(dialog.current_name, "in.png");
        assert_eq!(dialog.filters, vec!["Images (*.[pP][nN][gG])"]);
    }

    #[test]
    fn result_parser_finds_last_prefixed_json_payload() {
        let output = "noise\n__ASTREA_FILE_DIALOG__{\"accepted\":false}\nmore\n__ASTREA_FILE_DIALOG__{\"accepted\":true,\"filePath\":\"/tmp/a.txt\"}\n";

        let selection = parse_result_from_text(output).unwrap().unwrap();

        assert!(selection.accepted);
        assert_eq!(selection.file_path.as_deref(), Some("/tmp/a.txt"));
    }

    #[tokio::test]
    async fn run_dialog_reads_json_result_file_written_by_child() {
        let temp = tempfile::tempdir().unwrap();
        let fake_explorer = temp.path().join("fake-explorer");
        fs::write(
            &fake_explorer,
            "#!/usr/bin/env sh\nprintf '{\"accepted\":true,\"filePath\":\"/tmp/from-child.txt\"}' > \"$ASTREA_FILE_DIALOG_RESULT_FILE\"\n",
        )
        .unwrap();
        let mut permissions = fs::metadata(&fake_explorer).unwrap().permissions();
        permissions.set_mode(0o755);
        fs::set_permissions(&fake_explorer, permissions).unwrap();

        let config = DialogRunConfig {
            explorer_bin: fake_explorer,
            timeout: Duration::from_secs(2),
        };
        let selection = run_dialog_with_config(
            DialogMode::OpenFile,
            "Open",
            &HashMap::new(),
            &config,
            tokio::sync::watch::channel(false).1,
        )
        .await
        .unwrap();

        assert!(selection.accepted);
        assert_eq!(selection.file_path.as_deref(), Some("/tmp/from-child.txt"));
    }

    #[tokio::test]
    async fn run_dialog_cancellation_stops_child_without_blocking() {
        let temp = tempfile::tempdir().unwrap();
        let fake_explorer = temp.path().join("fake-explorer");
        fs::write(&fake_explorer, "#!/usr/bin/env sh\nsleep 10\n").unwrap();
        let mut permissions = fs::metadata(&fake_explorer).unwrap().permissions();
        permissions.set_mode(0o755);
        fs::set_permissions(&fake_explorer, permissions).unwrap();

        let config = DialogRunConfig {
            explorer_bin: fake_explorer,
            timeout: Duration::from_secs(5),
        };
        let (cancel, cancellation) = tokio::sync::watch::channel(false);
        let task = tokio::spawn(async move {
            run_dialog_with_config(
                DialogMode::OpenFile,
                "Open",
                &HashMap::new(),
                &config,
                cancellation,
            )
            .await
        });
        tokio::time::sleep(Duration::from_millis(50)).await;
        cancel.send(true).unwrap();

        let result = tokio::time::timeout(Duration::from_secs(1), task)
            .await
            .unwrap()
            .unwrap();
        assert!(result.unwrap_err().to_string().contains("cancelled"));
    }
}
