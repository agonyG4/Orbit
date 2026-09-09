use gio::prelude::*;
use rayon::prelude::*;
use serde::Serialize;
use std::collections::HashSet;
use std::fs::{self, File};
use std::io::Read;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::OnceLock;
use std::time::{Duration, SystemTime};

use crate::json;
use crate::thumbnail_cache::{
    SourceVersion, ThumbnailTier, cache_candidates, canonical_uri, create_private_staging_file,
    private_directory, read_failure_entry, read_valid_thumbnail, thumbnail_root, tier_for_target,
    tier_path, write_failure_entry, write_standard_thumbnail,
};

pub(crate) const MAX_BATCH_ITEMS: usize = 32;
pub(crate) const MAX_BATCH_ARGUMENT_BYTES: usize = 262_144;
const THUMBNAIL_THREADS: usize = 4;
const RECENT_FILE_COOLDOWN: Duration = Duration::from_secs(3);

static THUMBNAIL_POOL: OnceLock<Result<rayon::ThreadPool, String>> = OnceLock::new();

pub(crate) trait ThumbnailGenerator: Sync {
    fn generate(&self, input: &Path, output: &Path, target: u32) -> Result<(), String>;
}

struct ProcessThumbnailGenerator;

impl ThumbnailGenerator for ProcessThumbnailGenerator {
    fn generate(&self, input: &Path, output: &Path, target: u32) -> Result<(), String> {
        let extension = input
            .extension()
            .and_then(|value| value.to_str())
            .map(|value| value.to_ascii_lowercase())
            .unwrap_or_default();
        let status = match file_media_type(input) {
            Some("video") => {
                let filter =
                    format!("scale={target}:{target}:force_original_aspect_ratio=decrease");
                Command::new("ffmpeg")
                    .args(["-y", "-ss", "00:00:00", "-i"])
                    .arg(input)
                    .args(["-vframes", "1", "-vf", &filter])
                    .arg(output)
                    .status()
            }
            Some("image") if extension == "svg" => {
                let density = target.to_string();
                let size = format!("{target}x{target}");
                Command::new("magick")
                    .args(["-background", "none", "-density"])
                    .arg(density)
                    .arg(input)
                    .args([
                        "-filter", "Lanczos", "-resize", &size, "-alpha", "Set", "-strip",
                    ])
                    .arg(output)
                    .status()
            }
            Some("image") => {
                let size = format!("{target}x{target}>");
                Command::new("magick")
                    .arg(input)
                    .args([
                        "-auto-orient",
                        "-strip",
                        "-filter",
                        "Lanczos",
                        "-define",
                        "filter:blur=0.92",
                        "-thumbnail",
                        &size,
                    ])
                    .arg(output)
                    .status()
            }
            _ => return Err("unsupported thumbnail input".to_string()),
        }
        .map_err(|error| format!("start thumbnail generator: {error}"))?;

        if status.success() {
            Ok(())
        } else {
            Err(format!("thumbnail generator failed: {}", input.display()))
        }
    }
}

#[derive(Serialize)]
struct ThumbnailItem {
    #[serde(rename = "filePath")]
    file_path: String,
    status: &'static str,
    #[serde(rename = "previewUrl")]
    preview_url: String,
    #[serde(rename = "cacheTier")]
    cache_tier: &'static str,
    #[serde(rename = "sourceVersion")]
    source_version: String,
}

pub fn run_batch(args: &[String]) -> Result<(), String> {
    println!(
        "{}",
        run_batch_at(args, &ProcessThumbnailGenerator, None, SystemTime::now())?
    );
    Ok(())
}

pub(crate) fn batch(args: &[String]) -> Result<String, String> {
    run_batch_at(args, &ProcessThumbnailGenerator, None, SystemTime::now())
}

pub(crate) fn run_batch_with_generator(
    args: &[String],
    generator: &dyn ThumbnailGenerator,
    cache_override: Option<&Path>,
) -> Result<String, String> {
    run_batch_at(args, generator, cache_override, SystemTime::now())
}

pub(crate) fn run_batch_with_generator_at(
    args: &[String],
    generator: &dyn ThumbnailGenerator,
    cache_override: Option<&Path>,
    now: SystemTime,
) -> Result<String, String> {
    run_batch_at(args, generator, cache_override, now)
}

fn run_batch_at(
    args: &[String],
    generator: &dyn ThumbnailGenerator,
    cache_override: Option<&Path>,
    now: SystemTime,
) -> Result<String, String> {
    let target = args
        .first()
        .and_then(|value| value.parse::<u32>().ok())
        .filter(|value| *value > 0)
        .ok_or_else(|| "thumbnail target must be a positive integer".to_string())?;

    let mut paths = Vec::new();
    let mut seen = HashSet::new();
    let mut argument_bytes = args[0].len() + 1;
    for raw_path in args.iter().skip(1) {
        let path = PathBuf::from(raw_path);
        if !seen.insert(path.clone()) {
            continue;
        }
        if paths.len() >= MAX_BATCH_ITEMS {
            return Err(format!(
                "thumbnail batch exceeds {MAX_BATCH_ITEMS} unique paths"
            ));
        }
        argument_bytes = argument_bytes
            .saturating_add(raw_path.len())
            .saturating_add(1);
        if argument_bytes > MAX_BATCH_ARGUMENT_BYTES {
            return Err(format!(
                "thumbnail batch exceeds {MAX_BATCH_ARGUMENT_BYTES} argument bytes"
            ));
        }
        paths.push(path);
    }
    if paths.is_empty() {
        return Ok(batch_result_json(target, &[]));
    }

    let cache = match cache_override {
        Some(path) => path.to_path_buf(),
        None => thumbnail_root()?,
    };
    private_directory(&cache)?;
    let pool = THUMBNAIL_POOL
        .get_or_init(|| {
            rayon::ThreadPoolBuilder::new()
                .num_threads(THUMBNAIL_THREADS)
                .build()
                .map_err(|error| format!("thumbnail worker pool: {error}"))
        })
        .as_ref()
        .map_err(Clone::clone)?;
    let items = pool.install(|| {
        paths
            .par_iter()
            .map(|path| process_item(path, target, &cache, generator, now))
            .collect::<Vec<_>>()
    });
    Ok(batch_result_json(target, &items))
}

fn process_item(
    path: &Path,
    target: u32,
    cache: &Path,
    generator: &dyn ThumbnailGenerator,
    now: SystemTime,
) -> ThumbnailItem {
    let file_path = path.to_string_lossy().into_owned();
    let unsupported = |source_version: String| ThumbnailItem {
        file_path: file_path.clone(),
        status: "unsupported",
        preview_url: String::new(),
        cache_tier: "fail",
        source_version,
    };
    if is_remote_path(path) {
        return unsupported(String::new());
    }
    let metadata = match fs::metadata(path) {
        Ok(metadata) => metadata,
        Err(_) => return unsupported(String::new()),
    };
    if !metadata.is_file() || !is_previewable(path) {
        return unsupported(SourceVersion::from_metadata(&metadata).identity());
    }
    let version = SourceVersion::from_metadata(&metadata);
    let source_version = version.identity();
    let uri = match canonical_uri(path) {
        Ok(uri) => uri,
        Err(_) => {
            return ThumbnailItem {
                file_path,
                status: "failed",
                preview_url: String::new(),
                cache_tier: "fail",
                source_version,
            };
        }
    };
    let requested_tier = tier_for_target(target);
    let mime = mime_for_path(path);
    if !source_is_readable(path) {
        return ThumbnailItem {
            file_path,
            status: "deferred",
            preview_url: String::new(),
            cache_tier: requested_tier.directory_name(),
            source_version,
        };
    }
    for (tier, candidate) in cache_candidates(cache, &uri, requested_tier) {
        if read_valid_thumbnail(&candidate, &uri, version, mime).is_ok() {
            return ready_item(&file_path, &candidate, tier, source_version);
        }
        if let Some(gio_candidate) = gio_thumbnail_path(path, tier) {
            if read_valid_thumbnail(&gio_candidate, &uri, version, mime).is_ok() {
                return ready_item(&file_path, &gio_candidate, tier, source_version);
            }
        }
    }

    let failure_path = tier_path(cache, ThumbnailTier::Fail, &uri);
    if read_failure_entry(&failure_path, &uri, version).is_ok()
        || gio_failure_is_valid(path, requested_tier)
    {
        return ThumbnailItem {
            file_path,
            status: "failed",
            preview_url: String::new(),
            cache_tier: "fail",
            source_version,
        };
    }
    if is_recently_modified(&metadata, now) {
        return ThumbnailItem {
            file_path,
            status: "deferred",
            preview_url: String::new(),
            cache_tier: requested_tier.directory_name(),
            source_version,
        };
    }

    let destination = tier_path(cache, requested_tier, &uri);
    let raw = destination.parent().unwrap_or(cache).join(format!(
        ".{}.raw-{}.png",
        crate::thumbnail_cache::cache_filename(&uri),
        std::process::id()
    ));
    let destination_parent = destination.parent().unwrap_or(cache);
    if private_directory(destination_parent).is_err() {
        return failed_item(&file_path, source_version);
    }
    let _ = fs::remove_file(&raw);
    if create_private_staging_file(&raw).is_err() {
        return failed_item(&file_path, source_version);
    }
    let generated = generator.generate(path, &raw, requested_tier.max_pixels());
    let result = match generated {
        Ok(()) => write_standard_thumbnail(&raw, &destination, &uri, version, mime)
            .map(|()| {
                ready_item(
                    &file_path,
                    &destination,
                    requested_tier,
                    source_version.clone(),
                )
            })
            .unwrap_or_else(|_| failed_item(&file_path, source_version.clone())),
        Err(_) => failed_item(&file_path, source_version.clone()),
    };
    let _ = fs::remove_file(&raw);
    if result.status == "failed" {
        let _ = write_failure_entry(cache, &uri, version);
    }
    result
}

fn ready_item(
    file_path: &str,
    thumbnail: &Path,
    tier: ThumbnailTier,
    source_version: String,
) -> ThumbnailItem {
    ThumbnailItem {
        file_path: file_path.to_string(),
        status: "ready",
        preview_url: preview_url_identity(thumbnail, &source_version, tier.directory_name()),
        cache_tier: tier.directory_name(),
        source_version,
    }
}

fn failed_item(file_path: &str, source_version: String) -> ThumbnailItem {
    ThumbnailItem {
        file_path: file_path.to_string(),
        status: "failed",
        preview_url: String::new(),
        cache_tier: "fail",
        source_version,
    }
}

fn source_is_readable(path: &Path) -> bool {
    let Ok(mut file) = File::open(path) else {
        return false;
    };
    let mut probe = [0_u8; 1];
    file.read(&mut probe).is_ok()
}

fn batch_result_json(target: u32, items: &[ThumbnailItem]) -> String {
    serde_json::json!({
        "ok": true,
        "operation": "thumbnail-batch",
        "targetPx": target,
        "items": items,
    })
    .to_string()
}

fn preview_url_identity(path: &Path, source_version: &str, tier: &str) -> String {
    format!(
        "{}?sourceVersion={}&cacheTier={}",
        json::file_url(path),
        source_version.replace(':', "%3A"),
        tier
    )
}

fn gio_thumbnail_path(path: &Path, tier: ThumbnailTier) -> Option<PathBuf> {
    let (path_attribute, valid_attribute) = gio_thumbnail_attributes(tier)?;
    let info = gio::File::for_path(path)
        .query_info(
            &format!("{path_attribute},{valid_attribute}"),
            gio::FileQueryInfoFlags::NONE,
            None::<&gio::Cancellable>,
        )
        .ok()?;
    if !info.boolean(valid_attribute) {
        return None;
    }
    info.attribute_byte_string(path_attribute)
        .map(|value| PathBuf::from(value.as_str()))
}

fn gio_failure_is_valid(path: &Path, tier: ThumbnailTier) -> bool {
    let Some((path_attribute, valid_attribute)) = gio_thumbnail_attributes(tier) else {
        return false;
    };
    let failure_attribute = match tier {
        ThumbnailTier::Normal => "thumbnail::failed-normal",
        ThumbnailTier::Large => "thumbnail::failed-large",
        ThumbnailTier::XLarge => "thumbnail::failed-xlarge",
        ThumbnailTier::XXLarge => "thumbnail::failed-xxlarge",
        ThumbnailTier::Fail => return false,
    };
    gio::File::for_path(path)
        .query_info(
            &format!("{path_attribute},{valid_attribute},{failure_attribute}"),
            gio::FileQueryInfoFlags::NONE,
            None::<&gio::Cancellable>,
        )
        .map(|info| info.boolean(valid_attribute) && info.boolean(failure_attribute))
        .unwrap_or(false)
}

fn gio_thumbnail_attributes(tier: ThumbnailTier) -> Option<(&'static str, &'static str)> {
    match tier {
        ThumbnailTier::Normal => Some(("thumbnail::path-normal", "thumbnail::is-valid-normal")),
        ThumbnailTier::Large => Some(("thumbnail::path-large", "thumbnail::is-valid-large")),
        ThumbnailTier::XLarge => Some(("thumbnail::path-xlarge", "thumbnail::is-valid-xlarge")),
        ThumbnailTier::XXLarge => Some(("thumbnail::path-xxlarge", "thumbnail::is-valid-xxlarge")),
        ThumbnailTier::Fail => None,
    }
}

fn is_recently_modified(metadata: &fs::Metadata, now: SystemTime) -> bool {
    let Ok(modified) = metadata.modified() else {
        return false;
    };
    now.duration_since(modified)
        .map(|age| age < RECENT_FILE_COOLDOWN)
        .unwrap_or(false)
}

pub fn preview_url(path: &Path, is_dir: bool, modified_ms: i64, size: u64) -> String {
    let cached = cached_preview_url(path, is_dir, modified_ms, size);
    if !cached.is_empty() {
        return cached;
    }
    if is_svg(path) || file_media_type(path) == Some("image") {
        return json::file_url(path);
    }
    String::new()
}

pub fn cached_preview_url(path: &Path, is_dir: bool, modified_ms: i64, size: u64) -> String {
    if is_dir {
        return String::new();
    }
    let version = SourceVersion {
        modified_secs: modified_ms.div_euclid(1000),
        size,
    };
    if let Ok(uri) = canonical_uri(path) {
        if let Ok(root) = thumbnail_root() {
            for (tier, candidate) in cache_candidates(&root, &uri, tier_for_target(256)) {
                if read_valid_thumbnail(&candidate, &uri, version, mime_for_path(path)).is_ok() {
                    return preview_url_identity(
                        &candidate,
                        &version.identity(),
                        tier.directory_name(),
                    );
                }
            }
        }
    }
    String::new()
}

pub fn is_previewable(path: &Path) -> bool {
    file_media_type(path).is_some()
}

pub fn is_svg(path: &Path) -> bool {
    path.extension()
        .and_then(|value| value.to_str())
        .map(|value| value.eq_ignore_ascii_case("svg"))
        .unwrap_or(false)
}

fn is_remote_path(path: &Path) -> bool {
    let value = path.to_string_lossy();
    value.contains("://") && !value.starts_with("file://")
}

fn mime_for_path(path: &Path) -> Option<&'static str> {
    match path.extension()?.to_str()?.to_ascii_lowercase().as_str() {
        "jpg" | "jpeg" => Some("image/jpeg"),
        "png" => Some("image/png"),
        "gif" => Some("image/gif"),
        "bmp" => Some("image/bmp"),
        "webp" => Some("image/webp"),
        "svg" => Some("image/svg+xml"),
        "avif" => Some("image/avif"),
        "heic" | "heif" => Some("image/heif"),
        "tiff" | "tif" => Some("image/tiff"),
        "mp4" => Some("video/mp4"),
        "mkv" => Some("video/x-matroska"),
        "avi" => Some("video/x-msvideo"),
        "mov" => Some("video/quicktime"),
        "webm" => Some("video/webm"),
        _ => None,
    }
}

fn file_media_type(path: &Path) -> Option<&'static str> {
    match path.extension()?.to_str()?.to_ascii_lowercase().as_str() {
        "jpg" | "jpeg" | "png" | "gif" | "bmp" | "webp" | "svg" | "avif" | "heic" | "heif"
        | "tiff" | "tif" | "tga" | "ico" | "psd" | "jxl" | "exr" | "dds" | "ppm" | "pbm"
        | "pgm" => Some("image"),
        "mp4" | "mkv" | "avi" | "mov" | "webm" | "flv" | "wmv" | "m4v" | "ts" | "3gp" | "ogv"
        | "rm" | "rmvb" | "vob" | "divx" | "f4v" | "m2ts" | "mts" | "mpg" | "mpeg" | "asf"
        | "m2v" | "h264" | "h265" | "hevc" => Some("video"),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::thumbnail_cache::cache_filename;
    use std::fs;
    #[cfg(unix)]
    use std::os::unix::fs::PermissionsExt;
    use std::sync::{Arc, Mutex};

    struct CountingGenerator {
        calls: Arc<Mutex<Vec<String>>>,
        targets: Arc<Mutex<Vec<u32>>>,
    }

    impl ThumbnailGenerator for CountingGenerator {
        fn generate(&self, input: &Path, output: &Path, _target: u32) -> Result<(), String> {
            self.calls
                .lock()
                .unwrap()
                .push(input.to_string_lossy().into_owned());
            self.targets.lock().unwrap().push(_target);
            let file = std::fs::File::create(output).map_err(|error| error.to_string())?;
            let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), 1, 1);
            encoder.set_color(png::ColorType::Rgba);
            encoder.set_depth(png::BitDepth::Eight);
            let mut writer = encoder.write_header().map_err(|error| error.to_string())?;
            writer
                .write_image_data(&[255, 0, 0, 255])
                .map_err(|error| error.to_string())
        }
    }

    struct ConditionalGenerator {
        calls: Arc<Mutex<Vec<String>>>,
    }

    #[cfg(unix)]
    struct PermissionCheckingGenerator {
        staging_mode: Arc<Mutex<Option<u32>>>,
    }

    #[cfg(unix)]
    impl ThumbnailGenerator for PermissionCheckingGenerator {
        fn generate(&self, _input: &Path, output: &Path, _target: u32) -> Result<(), String> {
            use std::os::unix::fs::PermissionsExt;

            let mode = fs::metadata(output)
                .map_err(|error| error.to_string())?
                .permissions()
                .mode()
                & 0o777;
            *self.staging_mode.lock().unwrap() = Some(mode);
            let file = File::create(output).map_err(|error| error.to_string())?;
            let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), 1, 1);
            encoder.set_color(png::ColorType::Rgba);
            encoder.set_depth(png::BitDepth::Eight);
            let mut writer = encoder.write_header().map_err(|error| error.to_string())?;
            writer
                .write_image_data(&[255, 0, 0, 255])
                .map_err(|error| error.to_string())
        }
    }

    impl ThumbnailGenerator for ConditionalGenerator {
        fn generate(&self, input: &Path, output: &Path, target: u32) -> Result<(), String> {
            self.calls
                .lock()
                .unwrap()
                .push(input.to_string_lossy().into_owned());
            if input.file_stem().and_then(|value| value.to_str()) == Some("broken") {
                return Err("deterministic generator failure".to_string());
            }
            let file = std::fs::File::create(output).map_err(|error| error.to_string())?;
            let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), target, target);
            encoder.set_color(png::ColorType::Rgba);
            encoder.set_depth(png::BitDepth::Eight);
            let mut writer = encoder.write_header().map_err(|error| error.to_string())?;
            let pixel_count = target as usize * target as usize;
            let pixels = vec![255; pixel_count * 4];
            writer
                .write_image_data(&pixels)
                .map_err(|error| error.to_string())
        }
    }

    #[test]
    fn batch_uses_exact_paths_in_input_order_and_deduplicates() {
        let root = std::env::temp_dir().join(format!(
            "astrea-thumbnail-batch-order-{}",
            std::process::id()
        ));
        let cache = root.join("cache");
        fs::create_dir_all(&root).unwrap();
        let first = root.join("first.jpg");
        let second = root.join("second.mp4");
        fs::write(&first, b"first").unwrap();
        fs::write(&second, b"second").unwrap();
        let calls = Arc::new(Mutex::new(Vec::new()));
        let targets = Arc::new(Mutex::new(Vec::new()));
        let generator = CountingGenerator {
            calls: calls.clone(),
            targets,
        };
        let result = run_batch_with_generator_at(
            &[
                "256".to_string(),
                second.to_string_lossy().into_owned(),
                first.to_string_lossy().into_owned(),
                second.to_string_lossy().into_owned(),
            ],
            &generator,
            Some(&cache),
            std::time::SystemTime::now() + std::time::Duration::from_secs(4),
        )
        .unwrap();
        let items = serde_json::from_str::<serde_json::Value>(&result)
            .unwrap()
            .get("items")
            .and_then(serde_json::Value::as_array)
            .unwrap()
            .clone();

        assert_eq!(items.len(), 2);
        assert_eq!(items[0]["filePath"], second.to_string_lossy().as_ref());
        assert_eq!(items[1]["filePath"], first.to_string_lossy().as_ref());
        assert_eq!(calls.lock().unwrap().len(), 2);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn batch_rejects_more_than_32_paths_before_generation() {
        let root = std::env::temp_dir().join(format!(
            "astrea-thumbnail-batch-limit-{}",
            std::process::id()
        ));
        fs::create_dir_all(&root).unwrap();
        let mut args = vec!["256".to_string()];
        for index in 0..33 {
            let path = root.join(format!("file-{index}.jpg"));
            fs::write(&path, b"fixture").unwrap();
            args.push(path.to_string_lossy().into_owned());
        }
        let calls = Arc::new(Mutex::new(Vec::new()));
        let targets = Arc::new(Mutex::new(Vec::new()));
        let generator = CountingGenerator {
            calls: calls.clone(),
            targets,
        };

        let error = run_batch_with_generator_at(
            &args,
            &generator,
            Some(&root.join("cache")),
            std::time::SystemTime::now() + std::time::Duration::from_secs(4),
        )
        .unwrap_err();
        assert!(error.contains("32"));
        assert!(calls.lock().unwrap().is_empty());
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn valid_shared_cache_hit_skips_generation_and_reuses_tier() {
        let root =
            std::env::temp_dir().join(format!("astrea-thumbnail-cache-hit-{}", std::process::id()));
        let cache = root.join("cache");
        fs::create_dir_all(&root).unwrap();
        let source = root.join("photo.png");
        write_fixture_png(&source);
        let metadata = fs::metadata(&source).unwrap();
        let version = SourceVersion::from_metadata(&metadata);
        let uri = canonical_uri(&source).unwrap();
        let cached = tier_path(&cache, ThumbnailTier::Large, &uri);
        write_standard_thumbnail(&source, &cached, &uri, version, Some("image/png")).unwrap();

        let calls = Arc::new(Mutex::new(Vec::new()));
        let generator = CountingGenerator {
            calls: calls.clone(),
            targets: Arc::new(Mutex::new(Vec::new())),
        };
        let result = run_batch_with_generator_at(
            &["200".to_string(), source.to_string_lossy().into_owned()],
            &generator,
            Some(&cache),
            SystemTime::now() + Duration::from_secs(4),
        )
        .unwrap();
        let item = serde_json::from_str::<serde_json::Value>(&result).unwrap()["items"][0].clone();

        assert_eq!(item["status"], "ready");
        assert_eq!(item["cacheTier"], "large");
        assert!(item["previewUrl"].as_str().unwrap().contains("/large/"));
        assert!(calls.lock().unwrap().is_empty());
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn requested_target_selects_standard_tier_generation_size() {
        let root =
            std::env::temp_dir().join(format!("astrea-thumbnail-tier-{}", std::process::id()));
        fs::create_dir_all(&root).unwrap();
        let source = root.join("clip.mp4");
        fs::write(&source, b"video").unwrap();
        let targets = Arc::new(Mutex::new(Vec::new()));
        let generator = CountingGenerator {
            calls: Arc::new(Mutex::new(Vec::new())),
            targets: targets.clone(),
        };

        let result = run_batch_with_generator_at(
            &["640".to_string(), source.to_string_lossy().into_owned()],
            &generator,
            Some(&root.join("cache")),
            SystemTime::now() + Duration::from_secs(4),
        )
        .unwrap();
        let item = serde_json::from_str::<serde_json::Value>(&result).unwrap()["items"][0].clone();

        assert_eq!(item["status"], "ready");
        assert_eq!(item["cacheTier"], "xx-large");
        assert_eq!(targets.lock().unwrap().as_slice(), &[1024]);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn recent_file_is_deferred_without_starting_generator() {
        let root =
            std::env::temp_dir().join(format!("astrea-thumbnail-recent-{}", std::process::id()));
        fs::create_dir_all(&root).unwrap();
        let source = root.join("photo.png");
        fs::write(&source, b"still changing").unwrap();
        let calls = Arc::new(Mutex::new(Vec::new()));
        let generator = CountingGenerator {
            calls: calls.clone(),
            targets: Arc::new(Mutex::new(Vec::new())),
        };

        let result = run_batch_with_generator_at(
            &["128".to_string(), source.to_string_lossy().into_owned()],
            &generator,
            Some(&root.join("cache")),
            SystemTime::now(),
        )
        .unwrap();
        let item = serde_json::from_str::<serde_json::Value>(&result).unwrap()["items"][0].clone();

        assert_eq!(item["status"], "deferred");
        assert!(calls.lock().unwrap().is_empty());
        let _ = fs::remove_dir_all(root);
    }

    #[cfg(unix)]
    #[test]
    fn cache_paths_are_private_before_generator_runs() {
        use std::os::unix::fs::PermissionsExt;

        let root = std::env::temp_dir().join(format!(
            "astrea-thumbnail-private-generation-{}",
            std::process::id()
        ));
        let cache = root.join("cache");
        fs::create_dir_all(&root).unwrap();
        let source = root.join("photo.png");
        write_fixture_png(&source);
        let staging_mode = Arc::new(Mutex::new(None));
        let generator = PermissionCheckingGenerator {
            staging_mode: staging_mode.clone(),
        };

        let result = run_batch_with_generator_at(
            &["128".to_string(), source.to_string_lossy().into_owned()],
            &generator,
            Some(&cache),
            SystemTime::now() + Duration::from_secs(4),
        )
        .unwrap();
        let item = serde_json::from_str::<serde_json::Value>(&result).unwrap()["items"][0].clone();
        assert_eq!(item["status"], "ready");
        assert_eq!(
            fs::metadata(&cache).unwrap().permissions().mode() & 0o777,
            0o700
        );
        assert_eq!(
            fs::metadata(cache.join("normal"))
                .unwrap()
                .permissions()
                .mode()
                & 0o777,
            0o700
        );
        assert_eq!(*staging_mode.lock().unwrap(), Some(0o600));
        let uri = canonical_uri(&source).unwrap();
        let output = tier_path(&cache, ThumbnailTier::Normal, &uri);
        assert_eq!(
            fs::metadata(output).unwrap().permissions().mode() & 0o777,
            0o600
        );

        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn failure_entries_suppress_same_source_and_retry_after_size_change() {
        let root =
            std::env::temp_dir().join(format!("astrea-thumbnail-failure-{}", std::process::id()));
        let cache = root.join("cache");
        fs::create_dir_all(&root).unwrap();
        let healthy = root.join("healthy.png");
        let broken = root.join("broken.mp4");
        write_fixture_png(&healthy);
        fs::write(&broken, b"broken").unwrap();
        let first_calls = Arc::new(Mutex::new(Vec::new()));
        let first_generator = ConditionalGenerator {
            calls: first_calls.clone(),
        };

        let result = run_batch_with_generator_at(
            &[
                "128".to_string(),
                healthy.to_string_lossy().into_owned(),
                broken.to_string_lossy().into_owned(),
            ],
            &first_generator,
            Some(&cache),
            SystemTime::now() + Duration::from_secs(4),
        )
        .unwrap();
        let items = serde_json::from_str::<serde_json::Value>(&result).unwrap()["items"]
            .as_array()
            .unwrap()
            .clone();
        assert_eq!(items[0]["status"], "ready");
        assert_eq!(items[1]["status"], "failed");
        assert_eq!(first_calls.lock().unwrap().len(), 2);
        let failure_uri = canonical_uri(&broken).unwrap();
        let failure_path = cache
            .join("fail")
            .join(format!("orbit-explorer-{}", env!("CARGO_PKG_VERSION")))
            .join(cache_filename(&failure_uri));
        assert!(failure_path.is_file());
        assert_eq!(
            fs::metadata(failure_path.parent().unwrap())
                .unwrap()
                .permissions()
                .mode()
                & 0o777,
            0o700
        );
        assert_eq!(
            fs::metadata(&failure_path).unwrap().permissions().mode() & 0o777,
            0o600
        );

        let second_calls = Arc::new(Mutex::new(Vec::new()));
        let second_generator = ConditionalGenerator {
            calls: second_calls.clone(),
        };
        let suppressed = run_batch_with_generator_at(
            &[
                "128".to_string(),
                healthy.to_string_lossy().into_owned(),
                broken.to_string_lossy().into_owned(),
            ],
            &second_generator,
            Some(&cache),
            SystemTime::now() + Duration::from_secs(4),
        )
        .unwrap();
        let suppressed_items =
            serde_json::from_str::<serde_json::Value>(&suppressed).unwrap()["items"]
                .as_array()
                .unwrap()
                .clone();
        assert_eq!(suppressed_items[1]["status"], "failed");
        assert!(second_calls.lock().unwrap().is_empty());

        fs::write(&broken, b"broken source changed").unwrap();
        let third_calls = Arc::new(Mutex::new(Vec::new()));
        let third_generator = ConditionalGenerator {
            calls: third_calls.clone(),
        };
        let retried = run_batch_with_generator_at(
            &["128".to_string(), broken.to_string_lossy().into_owned()],
            &third_generator,
            Some(&cache),
            SystemTime::now() + Duration::from_secs(4),
        )
        .unwrap();
        let retried_item =
            serde_json::from_str::<serde_json::Value>(&retried).unwrap()["items"][0].clone();
        assert_eq!(retried_item["status"], "failed");
        assert_eq!(
            third_calls.lock().unwrap().as_slice(),
            &[broken.to_string_lossy().as_ref()]
        );
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn real_ffmpeg_generator_produces_a_readable_standard_thumbnail() {
        if Command::new("ffmpeg").arg("-version").output().is_err() {
            eprintln!("SKIPPED: ffmpeg is unavailable");
            return;
        }

        let root =
            std::env::temp_dir().join(format!("astrea-thumbnail-ffmpeg-{}", std::process::id()));
        let cache = root.join("cache");
        fs::create_dir_all(&root).unwrap();
        let source = root.join("fixture.mp4");
        let fixture = Command::new("ffmpeg")
            .args([
                "-v",
                "error",
                "-f",
                "lavfi",
                "-i",
                "color=c=red:s=16x16:d=2",
                "-frames:v",
                "2",
                "-c:v",
                "mpeg4",
            ])
            .arg(&source)
            .status()
            .unwrap();
        assert!(fixture.success());

        let result = run_batch_with_generator_at(
            &["128".to_string(), source.to_string_lossy().into_owned()],
            &ProcessThumbnailGenerator,
            Some(&cache),
            SystemTime::now() + Duration::from_secs(4),
        )
        .unwrap();
        let item = serde_json::from_str::<serde_json::Value>(&result).unwrap()["items"][0].clone();
        assert_eq!(item["status"], "ready");

        let uri = canonical_uri(&source).unwrap();
        let output = tier_path(&cache, ThumbnailTier::Normal, &uri);
        let file = std::fs::File::open(&output).unwrap();
        let mut reader = png::Decoder::new(std::io::BufReader::new(file))
            .read_info()
            .unwrap();
        let mut buffer = vec![0; reader.output_buffer_size().unwrap()];
        reader.next_frame(&mut buffer).unwrap();
        let version = SourceVersion::from_metadata(&fs::metadata(&source).unwrap());
        assert!(read_valid_thumbnail(&output, &uri, version, Some("video/mp4")).is_ok());
        let _ = fs::remove_dir_all(root);
    }

    #[cfg(unix)]
    #[test]
    fn unreadable_source_does_not_spawn_or_persist_failure() {
        use std::os::unix::fs::PermissionsExt;

        let root = std::env::temp_dir().join(format!(
            "astrea-thumbnail-unreadable-{}",
            std::process::id()
        ));
        let cache = root.join("cache");
        fs::create_dir_all(&root).unwrap();
        let source = root.join("secret.png");
        write_fixture_png(&source);
        fs::set_permissions(&source, fs::Permissions::from_mode(0o000)).unwrap();
        if std::fs::File::open(&source).is_ok() {
            eprintln!("SKIPPED: test user can read mode-000 files");
            fs::set_permissions(&source, fs::Permissions::from_mode(0o600)).unwrap();
            let _ = fs::remove_dir_all(root);
            return;
        }

        let calls = Arc::new(Mutex::new(Vec::new()));
        let generator = CountingGenerator {
            calls: calls.clone(),
            targets: Arc::new(Mutex::new(Vec::new())),
        };
        let result = run_batch_with_generator_at(
            &["128".to_string(), source.to_string_lossy().into_owned()],
            &generator,
            Some(&cache),
            SystemTime::now() + Duration::from_secs(4),
        )
        .unwrap();
        let item = serde_json::from_str::<serde_json::Value>(&result).unwrap()["items"][0].clone();
        assert_eq!(item["status"], "deferred");
        assert!(calls.lock().unwrap().is_empty());
        assert!(!cache.join("fail").exists());
        fs::set_permissions(&source, fs::Permissions::from_mode(0o600)).unwrap();
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn unsupported_item_isolated_from_other_batch_results() {
        let root = std::env::temp_dir().join(format!(
            "astrea-thumbnail-item-status-{}",
            std::process::id()
        ));
        fs::create_dir_all(&root).unwrap();
        let supported = root.join("photo.png");
        let unsupported = root.join("notes.txt");
        write_fixture_png(&supported);
        fs::write(&unsupported, b"notes").unwrap();
        let generator = CountingGenerator {
            calls: Arc::new(Mutex::new(Vec::new())),
            targets: Arc::new(Mutex::new(Vec::new())),
        };

        let result = run_batch_with_generator_at(
            &[
                "128".to_string(),
                supported.to_string_lossy().into_owned(),
                unsupported.to_string_lossy().into_owned(),
            ],
            &generator,
            Some(&root.join("cache")),
            SystemTime::now() + Duration::from_secs(4),
        )
        .unwrap();
        let items = serde_json::from_str::<serde_json::Value>(&result).unwrap()["items"]
            .as_array()
            .unwrap()
            .clone();

        assert_eq!(items[0]["status"], "ready");
        assert_eq!(items[1]["status"], "unsupported");
        let _ = fs::remove_dir_all(root);
    }

    fn write_fixture_png(path: &Path) {
        let file = std::fs::File::create(path).unwrap();
        let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), 1, 1);
        encoder.set_color(png::ColorType::Rgba);
        encoder.set_depth(png::BitDepth::Eight);
        let mut writer = encoder.write_header().unwrap();
        writer.write_image_data(&[255, 0, 0, 255]).unwrap();
    }

    #[test]
    fn preview_url_percent_encodes_local_image_paths() {
        let root =
            std::env::temp_dir().join(format!("astrea-preview-url-test-{}", std::process::id()));
        let _ = fs::remove_dir_all(&root);
        fs::create_dir_all(&root).unwrap();
        let path = root.join("img # one.png");
        fs::write(&path, "x").unwrap();

        let url = json::file_url(&path);

        assert!(url.ends_with("img%20%23%20one.png"));
        assert!(!url.ends_with("img # one.png"));
        let _ = fs::remove_dir_all(root);
    }
}
