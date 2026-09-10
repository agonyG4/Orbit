use gio::prelude::*;
use std::collections::HashMap;
use std::fs::{self, File, OpenOptions};
use std::io::{BufReader, BufWriter};
use std::path::{Component, Path, PathBuf};
use std::time::UNIX_EPOCH;

#[cfg(test)]
use std::sync::atomic::{AtomicUsize, Ordering};

const FAILURE_CACHE_APPLICATION: &str = "orbit-explorer";
const FAILURE_CACHE_VERSION: &str = env!("CARGO_PKG_VERSION");
const MAX_VALID_THUMBNAIL_DIMENSION: u32 = 1024;

#[cfg(test)]
static FULL_VALIDATION_COUNT: AtomicUsize = AtomicUsize::new(0);

#[cfg(test)]
pub(crate) fn reset_full_validation_count() {
    FULL_VALIDATION_COUNT.store(0, Ordering::Relaxed);
}

#[cfg(test)]
pub(crate) fn full_validation_count() -> usize {
    FULL_VALIDATION_COUNT.load(Ordering::Relaxed)
}

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub(crate) enum ThumbnailTier {
    Normal,
    Large,
    XLarge,
    XXLarge,
    Fail,
}

impl ThumbnailTier {
    pub(crate) fn directory_name(self) -> &'static str {
        match self {
            Self::Normal => "normal",
            Self::Large => "large",
            Self::XLarge => "x-large",
            Self::XXLarge => "xx-large",
            Self::Fail => "fail",
        }
    }

    pub(crate) fn max_pixels(self) -> u32 {
        match self {
            Self::Normal => 128,
            Self::Large => 256,
            Self::XLarge => 512,
            Self::XXLarge => 1024,
            Self::Fail => 0,
        }
    }
}

pub(crate) fn tier_for_target(target: u32) -> ThumbnailTier {
    match target {
        0..=128 => ThumbnailTier::Normal,
        129..=256 => ThumbnailTier::Large,
        257..=512 => ThumbnailTier::XLarge,
        _ => ThumbnailTier::XXLarge,
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub(crate) struct SourceVersion {
    pub(crate) modified_secs: i64,
    pub(crate) size: u64,
}

impl SourceVersion {
    pub(crate) fn from_metadata(metadata: &fs::Metadata) -> Self {
        let modified_secs = metadata
            .modified()
            .ok()
            .and_then(|value| value.duration_since(UNIX_EPOCH).ok())
            .map(|value| value.as_secs() as i64)
            .unwrap_or_default();
        Self {
            modified_secs,
            size: metadata.len(),
        }
    }

    pub(crate) fn identity(self) -> String {
        format!("{}:{}", self.modified_secs, self.size)
    }
}

pub(crate) fn cache_root(xdg_cache_home: Option<&Path>, home: &Path) -> Result<PathBuf, String> {
    let base = match xdg_cache_home.filter(|path| path.is_absolute()) {
        Some(path) => path.to_path_buf(),
        None if home.is_absolute() => home.join(".cache"),
        None => return Err("home path must be absolute".to_string()),
    };
    Ok(base.join("thumbnails"))
}

pub(crate) fn thumbnail_root() -> Result<PathBuf, String> {
    let xdg = std::env::var_os("XDG_CACHE_HOME").map(PathBuf::from);
    let home = std::env::var_os("HOME").map(PathBuf::from);
    thumbnail_root_from_environment(xdg.as_deref(), home.as_deref())
}

fn thumbnail_root_from_environment(
    xdg_cache_home: Option<&Path>,
    home: Option<&Path>,
) -> Result<PathBuf, String> {
    if let Some(path) = xdg_cache_home.filter(|path| path.is_absolute()) {
        return cache_root(Some(path), Path::new("/"));
    }
    let home = home.ok_or_else(|| "HOME not set".to_string())?;
    cache_root(xdg_cache_home, home)
}

pub(crate) fn canonical_uri(path: &Path) -> Result<String, String> {
    let absolute = if path.is_absolute() {
        path.to_path_buf()
    } else {
        std::env::current_dir()
            .map_err(|error| format!("current directory: {error}"))?
            .join(path)
    };
    let mut normalized = PathBuf::new();
    for component in absolute.components() {
        match component {
            Component::CurDir => {}
            Component::ParentDir => {
                if !normalized.pop() {
                    return Err(format!("path escapes its root: {}", absolute.display()));
                }
            }
            other => normalized.push(other.as_os_str()),
        }
    }
    Ok(gio::File::for_path(normalized).uri().to_string())
}

pub(crate) fn cache_filename(uri: &str) -> String {
    format!("{:x}.png", md5::compute(uri.as_bytes()))
}

pub(crate) fn tier_path(root: &Path, tier: ThumbnailTier, uri: &str) -> PathBuf {
    if tier == ThumbnailTier::Fail {
        return failure_tier_path(root, uri);
    }
    root.join(tier.directory_name()).join(cache_filename(uri))
}

pub(crate) fn failure_application_dir(root: &Path) -> PathBuf {
    root.join("fail").join(format!(
        "{FAILURE_CACHE_APPLICATION}-{FAILURE_CACHE_VERSION}"
    ))
}

pub(crate) fn failure_tier_path(root: &Path, uri: &str) -> PathBuf {
    failure_application_dir(root).join(cache_filename(uri))
}

pub(crate) fn cache_candidates(
    root: &Path,
    uri: &str,
    requested: ThumbnailTier,
) -> Vec<(ThumbnailTier, PathBuf)> {
    let tiers = match requested {
        ThumbnailTier::Normal => vec![
            ThumbnailTier::Normal,
            ThumbnailTier::Large,
            ThumbnailTier::XLarge,
            ThumbnailTier::XXLarge,
        ],
        ThumbnailTier::Large => vec![
            ThumbnailTier::Large,
            ThumbnailTier::XLarge,
            ThumbnailTier::XXLarge,
        ],
        ThumbnailTier::XLarge => vec![ThumbnailTier::XLarge, ThumbnailTier::XXLarge],
        ThumbnailTier::XXLarge => vec![ThumbnailTier::XXLarge],
        ThumbnailTier::Fail => vec![ThumbnailTier::Fail],
    };
    tiers
        .into_iter()
        .map(|tier| (tier, tier_path(root, tier, uri)))
        .collect()
}

fn png_text(path: &Path) -> Result<HashMap<String, String>, String> {
    #[cfg(test)]
    FULL_VALIDATION_COUNT.fetch_add(1, Ordering::Relaxed);

    let file = File::open(path).map_err(|error| format!("open {}: {error}", path.display()))?;
    let decoder = png::Decoder::new(BufReader::new(file));
    let mut reader = decoder
        .read_info()
        .map_err(|error| format!("read PNG {}: {error}", path.display()))?;
    if reader.info().width > MAX_VALID_THUMBNAIL_DIMENSION
        || reader.info().height > MAX_VALID_THUMBNAIL_DIMENSION
    {
        return Err(format!(
            "PNG {} exceeds thumbnail dimensions",
            path.display()
        ));
    }
    let mut values = HashMap::new();
    {
        for chunk in &reader.info().uncompressed_latin1_text {
            values.insert(chunk.keyword.clone(), chunk.text.clone());
        }
        for chunk in &reader.info().compressed_latin1_text {
            if let Ok(text) = chunk.get_text() {
                values.insert(chunk.keyword.clone(), text);
            }
        }
        for chunk in &reader.info().utf8_text {
            if let Ok(text) = chunk.get_text() {
                values.insert(chunk.keyword.clone(), text);
            }
        }
    }
    let output_size = reader
        .output_buffer_size()
        .ok_or_else(|| format!("PNG {} has no bounded output", path.display()))?;
    let mut buffer = vec![0; output_size];
    reader
        .next_frame(&mut buffer)
        .map_err(|error| format!("decode PNG {}: {error}", path.display()))?;
    Ok(values)
}

pub(crate) fn read_valid_thumbnail(
    path: &Path,
    uri: &str,
    version: SourceVersion,
    _expected_mime: Option<&str>,
) -> Result<(), String> {
    if !path.is_file() {
        return Err("thumbnail is not a file".to_string());
    }
    let values = png_text(path)?;
    if values.get("Thumb::URI").map(String::as_str) != Some(uri) {
        return Err("thumbnail URI does not match source".to_string());
    }
    if values
        .get("Thumb::MTime")
        .and_then(|value| value.parse::<i64>().ok())
        != Some(version.modified_secs)
    {
        return Err("thumbnail mtime does not match source".to_string());
    }
    if let Some(size) = values.get("Thumb::Size") {
        if size.parse::<u64>().ok() != Some(version.size) {
            return Err("thumbnail size does not match source".to_string());
        }
    }
    Ok(())
}

pub(crate) fn read_failure_entry(
    path: &Path,
    uri: &str,
    version: SourceVersion,
) -> Result<(), String> {
    read_valid_thumbnail(path, uri, version, None)
}

pub(crate) fn private_directory(path: &Path) -> Result<(), String> {
    #[cfg(unix)]
    {
        use std::os::unix::fs::DirBuilderExt;
        if !path.exists() {
            let mut builder = fs::DirBuilder::new();
            builder.recursive(true).mode(0o700);
            if let Err(error) = builder.create(path) {
                if error.kind() != std::io::ErrorKind::AlreadyExists {
                    return Err(format!("create {}: {error}", path.display()));
                }
            }
        }
        if !path.is_dir() {
            return Err(format!("{} is not a directory", path.display()));
        }
        fs::set_permissions(path, std::os::unix::fs::PermissionsExt::from_mode(0o700))
            .map_err(|error| format!("permissions {}: {error}", path.display()))?;
    }
    #[cfg(not(unix))]
    fs::create_dir_all(path).map_err(|error| format!("create {}: {error}", path.display()))?;
    Ok(())
}

pub(crate) fn private_thumbnail_directories(
    root: &Path,
    tier: ThumbnailTier,
) -> Result<(), String> {
    private_directory(root)?;
    let tier_directory = root.join(tier.directory_name());
    private_directory(&tier_directory)?;
    if tier == ThumbnailTier::Fail {
        private_directory(&failure_application_dir(root))?;
    }
    Ok(())
}

pub(crate) fn create_private_staging_file(path: &Path) -> Result<File, String> {
    let mut options = OpenOptions::new();
    options.write(true).create_new(true);
    #[cfg(unix)]
    {
        use std::os::unix::fs::OpenOptionsExt;
        options.mode(0o600);
    }
    let file = options
        .open(path)
        .map_err(|error| format!("create {}: {error}", path.display()))?;
    #[cfg(unix)]
    file.set_permissions(std::os::unix::fs::PermissionsExt::from_mode(0o600))
        .map_err(|error| format!("permissions {}: {error}", path.display()))?;
    Ok(file)
}

fn encode_png_with_metadata(
    input: &Path,
    output: &Path,
    uri: &str,
    version: SourceVersion,
    mimetype: Option<&str>,
    software: &str,
) -> Result<(), String> {
    let input_file =
        File::open(input).map_err(|error| format!("open {}: {error}", input.display()))?;
    let mut decoder = png::Decoder::new(BufReader::new(input_file));
    decoder.set_transformations(
        png::Transformations::EXPAND | png::Transformations::ALPHA | png::Transformations::STRIP_16,
    );
    let mut reader = decoder
        .read_info()
        .map_err(|error| format!("read PNG {}: {error}", input.display()))?;
    let output_size = reader
        .output_buffer_size()
        .ok_or_else(|| "PNG output is too large".to_string())?;
    let mut buffer = vec![0; output_size];
    let frame = reader
        .next_frame(&mut buffer)
        .map_err(|error| format!("decode PNG {}: {error}", input.display()))?;
    let image_data = rgba8_image_data(
        frame.color_type,
        frame.bit_depth,
        &buffer[..frame.buffer_size()],
    )?;
    let width = frame.width;
    let height = frame.height;
    drop(reader);

    let output_file = create_private_staging_file(output)?;
    let mut encoder = png::Encoder::new(BufWriter::new(output_file), width, height);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    encoder
        .add_text_chunk("Thumb::URI".to_string(), uri.to_string())
        .map_err(|error| format!("PNG URI metadata: {error}"))?;
    encoder
        .add_text_chunk(
            "Thumb::MTime".to_string(),
            version.modified_secs.to_string(),
        )
        .map_err(|error| format!("PNG mtime metadata: {error}"))?;
    encoder
        .add_text_chunk("Thumb::Size".to_string(), version.size.to_string())
        .map_err(|error| format!("PNG size metadata: {error}"))?;
    if let Some(mime) = mimetype {
        encoder
            .add_text_chunk("Thumb::Mimetype".to_string(), mime.to_string())
            .map_err(|error| format!("PNG MIME metadata: {error}"))?;
    }
    encoder
        .add_text_chunk("Software".to_string(), software.to_string())
        .map_err(|error| format!("PNG software metadata: {error}"))?;
    let mut writer = encoder
        .write_header()
        .map_err(|error| format!("PNG header: {error}"))?;
    writer
        .write_image_data(&image_data)
        .map_err(|error| format!("PNG image data: {error}"))?;
    Ok(())
}

fn rgba8_image_data(
    color_type: png::ColorType,
    bit_depth: png::BitDepth,
    data: &[u8],
) -> Result<Vec<u8>, String> {
    if bit_depth != png::BitDepth::Eight {
        return Err("PNG decoder did not produce 8-bit samples".to_string());
    }
    match color_type {
        png::ColorType::Rgba => Ok(data.to_vec()),
        png::ColorType::Rgb => Ok(data
            .chunks_exact(3)
            .flat_map(|pixel| [pixel[0], pixel[1], pixel[2], 255])
            .collect()),
        png::ColorType::Grayscale => Ok(data
            .iter()
            .flat_map(|gray| [*gray, *gray, *gray, 255])
            .collect()),
        png::ColorType::GrayscaleAlpha => Ok(data
            .chunks_exact(2)
            .flat_map(|pixel| [pixel[0], pixel[0], pixel[0], pixel[1]])
            .collect()),
        png::ColorType::Indexed => Err("PNG decoder left an indexed image".to_string()),
    }
}

pub(crate) fn write_standard_thumbnail(
    input: &Path,
    destination: &Path,
    uri: &str,
    version: SourceVersion,
    mimetype: Option<&str>,
) -> Result<(), String> {
    let parent = destination
        .parent()
        .ok_or_else(|| "thumbnail has no parent directory".to_string())?;
    private_directory(parent)?;
    let temporary = parent.join(format!(
        ".{}.tmp-{}",
        cache_filename(uri),
        std::process::id()
    ));
    let _ = fs::remove_file(&temporary);
    let result =
        encode_png_with_metadata(input, &temporary, uri, version, mimetype, "Orbit Explorer");
    if result.is_err() {
        let _ = fs::remove_file(&temporary);
        return result;
    }
    fs::rename(&temporary, destination)
        .map_err(|error| format!("install thumbnail {}: {error}", destination.display()))
}

pub(crate) fn write_failure_entry(
    root: &Path,
    uri: &str,
    version: SourceVersion,
) -> Result<PathBuf, String> {
    private_thumbnail_directories(root, ThumbnailTier::Fail)?;
    let destination = failure_tier_path(root, uri);
    let parent = destination
        .parent()
        .ok_or_else(|| "failure entry has no parent directory".to_string())?;
    private_directory(parent)?;
    let fixture = parent.join(format!(
        ".failure-input-{}-{}",
        std::process::id(),
        cache_filename(uri)
    ));
    let result = (|| {
        create_fixture_png(&fixture)?;
        write_standard_thumbnail(
            &fixture,
            &destination,
            uri,
            version,
            Some("application/x-orbit-thumbnail-failure"),
        )
    })();
    let _ = fs::remove_file(fixture);
    result.map(|()| destination)
}

fn create_fixture_png(path: &Path) -> Result<(), String> {
    let file = create_private_staging_file(path)?;
    let mut encoder = png::Encoder::new(BufWriter::new(file), 1, 1);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    let mut writer = encoder
        .write_header()
        .map_err(|error| format!("fixture header: {error}"))?;
    writer
        .write_image_data(&[0, 0, 0, 0])
        .map_err(|error| format!("fixture data: {error}"))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::Path;

    #[test]
    fn tier_boundaries_follow_freedesktop_limits() {
        assert_eq!(tier_for_target(128), ThumbnailTier::Normal);
        assert_eq!(tier_for_target(129), ThumbnailTier::Large);
        assert_eq!(tier_for_target(256), ThumbnailTier::Large);
        assert_eq!(tier_for_target(257), ThumbnailTier::XLarge);
        assert_eq!(tier_for_target(512), ThumbnailTier::XLarge);
        assert_eq!(tier_for_target(513), ThumbnailTier::XXLarge);
        assert_eq!(tier_for_target(1024), ThumbnailTier::XXLarge);
        assert_eq!(tier_for_target(1025), ThumbnailTier::XXLarge);
    }

    #[test]
    fn canonical_uri_and_filename_use_standard_identity() {
        let root =
            std::env::temp_dir().join(format!("astrea-thumbnail-cache-{}", std::process::id()));
        std::fs::create_dir_all(&root).unwrap();
        let path = root.join("photo # é.png");
        std::fs::write(&path, b"fixture").unwrap();
        let equivalent = root.join(".").join("photo # é.png");

        let uri = canonical_uri(&path).unwrap();
        assert_eq!(uri, canonical_uri(&equivalent).unwrap());
        assert!(uri.starts_with("file:///"));
        assert!(uri.contains("%20"));
        assert!(uri.contains("%23"));
        assert!(uri.contains("%C3%A9"));

        let filename = cache_filename(&uri);
        assert_eq!(filename.len(), 36);
        assert!(filename.ends_with(".png"));
        assert!(
            filename[..32]
                .chars()
                .all(|c| c.is_ascii_hexdigit() && !c.is_ascii_uppercase())
        );

        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn xdg_cache_home_overrides_home_cache_directory() {
        let home = Path::new("/fixture/home");
        assert_eq!(
            cache_root(Some(Path::new("/fixture/cache")), home).unwrap(),
            Path::new("/fixture/cache/thumbnails")
        );
        assert_eq!(
            cache_root(None, home).unwrap(),
            Path::new("/fixture/home/.cache/thumbnails")
        );
    }

    #[test]
    fn absolute_xdg_cache_home_does_not_require_home() {
        assert_eq!(
            thumbnail_root_from_environment(Some(Path::new("/fixture/cache")), None,).unwrap(),
            Path::new("/fixture/cache/thumbnails")
        );
    }

    #[test]
    fn unusable_xdg_cache_home_falls_back_to_home_or_errors() {
        assert_eq!(
            thumbnail_root_from_environment(
                Some(Path::new("relative/cache")),
                Some(Path::new("/fixture/home")),
            )
            .unwrap(),
            Path::new("/fixture/home/.cache/thumbnails")
        );
        assert!(thumbnail_root_from_environment(Some(Path::new("relative/cache")), None).is_err());
        assert!(thumbnail_root_from_environment(None, None).is_err());
    }

    #[test]
    fn standard_png_metadata_and_freshness_are_validated() {
        let root =
            std::env::temp_dir().join(format!("astrea-thumbnail-metadata-{}", std::process::id()));
        std::fs::create_dir_all(&root).unwrap();
        let input = root.join("input.png");
        let output = root.join("large").join("thumb.png");
        create_fixture_png(&input).unwrap();
        let uri = "file:///fixture/input.png";
        let version = SourceVersion {
            modified_secs: 42,
            size: 7,
        };

        write_standard_thumbnail(&input, &output, uri, version, Some("image/png")).unwrap();
        assert!(read_valid_thumbnail(&output, uri, version, Some("image/png")).is_ok());
        assert!(read_valid_thumbnail(&output, uri, version, Some("image/jpeg")).is_ok());
        assert!(
            read_valid_thumbnail(
                &output,
                uri,
                SourceVersion {
                    modified_secs: 43,
                    ..version
                },
                Some("image/png"),
            )
            .is_err()
        );
        assert!(
            read_valid_thumbnail(
                &output,
                uri,
                SourceVersion { size: 8, ..version },
                Some("image/png"),
            )
            .is_err()
        );
        assert!(!root.join("large").join(".thumb.png.tmp").exists());
        let values = png_text(&output).unwrap();
        assert_eq!(values.get("Thumb::URI").map(String::as_str), Some(uri));
        assert_eq!(values.get("Thumb::MTime").map(String::as_str), Some("42"));
        assert_eq!(values.get("Thumb::Size").map(String::as_str), Some("7"));
        assert_eq!(
            values.get("Thumb::Mimetype").map(String::as_str),
            Some("image/png")
        );
        assert_eq!(
            values.get("Software").map(String::as_str),
            Some("Orbit Explorer")
        );

        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn rewrite_uses_post_transform_png_layout_for_common_inputs() {
        let root = std::env::temp_dir().join(format!(
            "astrea-thumbnail-png-layout-{}",
            std::process::id()
        ));
        std::fs::create_dir_all(&root).unwrap();
        let uri = "file:///fixture/layout.png";
        let version = SourceVersion {
            modified_secs: 42,
            size: 7,
        };
        let variants = [
            (
                "rgba",
                png::ColorType::Rgba,
                png::BitDepth::Eight,
                vec![
                    255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255,
                ],
                None,
            ),
            (
                "rgb",
                png::ColorType::Rgb,
                png::BitDepth::Eight,
                vec![255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 0],
                None,
            ),
            (
                "gray",
                png::ColorType::Grayscale,
                png::BitDepth::Eight,
                vec![0, 85, 170, 255],
                None,
            ),
            (
                "gray-alpha",
                png::ColorType::GrayscaleAlpha,
                png::BitDepth::Eight,
                vec![0, 255, 85, 192, 170, 128, 255, 0],
                None,
            ),
            (
                "indexed",
                png::ColorType::Indexed,
                png::BitDepth::Eight,
                vec![0, 1, 2, 3],
                Some(vec![255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 0]),
            ),
            (
                "indexed-low-bit",
                png::ColorType::Indexed,
                png::BitDepth::Two,
                vec![0x1b, 0xe4],
                Some(vec![255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 0]),
            ),
        ];

        for (name, color_type, bit_depth, data, palette) in variants {
            let input = root.join(format!("{name}.png"));
            let output = root.join(format!("{name}-out.png"));
            write_variant_png(&input, color_type, bit_depth, &data, palette.as_deref());
            write_standard_thumbnail(&input, &output, uri, version, Some("image/png")).unwrap();
            assert!(read_valid_thumbnail(&output, uri, version, Some("image/jpeg")).is_ok());

            let file = File::open(&output).unwrap();
            let mut reader = png::Decoder::new(BufReader::new(file)).read_info().unwrap();
            assert_eq!((reader.info().width, reader.info().height), (2, 2));
            let output_size = reader.output_buffer_size().unwrap();
            let mut buffer = vec![0; output_size];
            let frame = reader.next_frame(&mut buffer).unwrap();
            assert_eq!((frame.width, frame.height), (2, 2));
            assert_eq!(frame.buffer_size(), output_size);
            assert_eq!(frame.color_type, png::ColorType::Rgba);
            assert_eq!(frame.bit_depth, png::BitDepth::Eight);
        }

        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn truncated_png_payload_is_not_a_valid_thumbnail() {
        let root =
            std::env::temp_dir().join(format!("astrea-thumbnail-truncated-{}", std::process::id()));
        std::fs::create_dir_all(&root).unwrap();
        let input = root.join("input.png");
        let output = root.join("normal").join("thumb.png");
        create_fixture_png(&input).unwrap();
        let uri = "file:///fixture/input.png";
        let version = SourceVersion {
            modified_secs: 42,
            size: 7,
        };
        write_standard_thumbnail(&input, &output, uri, version, None).unwrap();
        let mut bytes = std::fs::read(&output).unwrap();
        let idat = bytes.windows(4).position(|chunk| chunk == b"IDAT").unwrap();
        let data_length = u32::from_be_bytes(bytes[idat - 4..idat].try_into().unwrap()) as usize;
        bytes.truncate(idat + 4 + data_length / 2);
        std::fs::write(&output, bytes).unwrap();

        assert!(read_valid_thumbnail(&output, uri, version, None).is_err());
        let _ = std::fs::remove_dir_all(root);
    }

    #[cfg(unix)]
    #[test]
    fn cache_directories_and_files_are_private_before_generation() {
        use std::os::unix::fs::PermissionsExt;

        let root = std::env::temp_dir().join(format!(
            "astrea-thumbnail-permissions-{}",
            std::process::id()
        ));
        let cache = root.join("cache");
        std::fs::create_dir_all(&root).unwrap();
        let input = root.join("input.png");
        let output = cache.join("normal").join("thumb.png");
        create_fixture_png(&input).unwrap();
        let version = SourceVersion {
            modified_secs: 42,
            size: 7,
        };
        write_standard_thumbnail(&input, &output, "file:///fixture/input.png", version, None)
            .unwrap();

        assert_eq!(
            std::fs::metadata(&cache).unwrap().permissions().mode() & 0o777,
            0o700
        );
        assert_eq!(
            std::fs::metadata(output.parent().unwrap())
                .unwrap()
                .permissions()
                .mode()
                & 0o777,
            0o700
        );
        assert_eq!(
            std::fs::metadata(&output).unwrap().permissions().mode() & 0o777,
            0o600
        );
        let _ = std::fs::remove_dir_all(root);
    }

    #[cfg(unix)]
    #[test]
    fn canonical_uri_preserves_symlink_identity_used_by_gio() {
        let root =
            std::env::temp_dir().join(format!("astrea-thumbnail-symlink-{}", std::process::id()));
        let real = root.join("real");
        let link = root.join("link");
        std::fs::create_dir_all(&real).unwrap();
        let source = real.join("photo # é.png");
        std::fs::write(&source, b"fixture").unwrap();
        std::os::unix::fs::symlink(&real, &link).unwrap();
        let user_path = link.join("photo # é.png");

        let orbit_uri = canonical_uri(&user_path).unwrap();
        let gio_uri = gio::File::for_path(&user_path).uri().to_string();
        assert_eq!(orbit_uri, gio_uri);

        let _ = std::fs::remove_dir_all(root);
    }

    fn write_variant_png(
        path: &Path,
        color_type: png::ColorType,
        bit_depth: png::BitDepth,
        data: &[u8],
        palette: Option<&[u8]>,
    ) {
        let file = File::create(path).unwrap();
        let mut encoder = png::Encoder::new(BufWriter::new(file), 2, 2);
        encoder.set_color(color_type);
        encoder.set_depth(bit_depth);
        if let Some(palette) = palette {
            encoder.set_palette(palette);
        }
        let mut writer = encoder.write_header().unwrap();
        writer.write_image_data(data).unwrap();
    }
}
