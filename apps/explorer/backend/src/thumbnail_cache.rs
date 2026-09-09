use gio::prelude::*;
use std::collections::HashMap;
use std::fs::{self, File, OpenOptions};
use std::io::{BufReader, BufWriter};
use std::path::{Path, PathBuf};
use std::time::UNIX_EPOCH;

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
    let home = std::env::var_os("HOME")
        .map(PathBuf::from)
        .ok_or_else(|| "HOME not set".to_string())?;
    cache_root(xdg.as_deref(), &home)
}

pub(crate) fn canonical_uri(path: &Path) -> Result<String, String> {
    let absolute = if path.is_absolute() {
        path.to_path_buf()
    } else {
        std::env::current_dir()
            .map_err(|error| format!("current directory: {error}"))?
            .join(path)
    };
    let canonical = fs::canonicalize(&absolute)
        .map_err(|error| format!("canonicalize {}: {error}", absolute.display()))?;
    Ok(gio::File::for_path(canonical).uri().to_string())
}

pub(crate) fn cache_filename(uri: &str) -> String {
    format!("{:x}.png", md5::compute(uri.as_bytes()))
}

pub(crate) fn tier_path(root: &Path, tier: ThumbnailTier, uri: &str) -> PathBuf {
    root.join(tier.directory_name()).join(cache_filename(uri))
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
    let file = File::open(path).map_err(|error| format!("open {}: {error}", path.display()))?;
    let decoder = png::Decoder::new(BufReader::new(file));
    let reader = decoder
        .read_info()
        .map_err(|error| format!("read PNG {}: {error}", path.display()))?;
    let mut values = HashMap::new();
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
    Ok(values)
}

pub(crate) fn read_valid_thumbnail(
    path: &Path,
    uri: &str,
    version: SourceVersion,
    expected_mime: Option<&str>,
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
    if let Some(mime) = expected_mime {
        if let Some(actual) = values.get("Thumb::Mimetype") {
            if actual != mime {
                return Err("thumbnail MIME type does not match source".to_string());
            }
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

fn private_directory(path: &Path) -> Result<(), String> {
    fs::create_dir_all(path).map_err(|error| format!("create {}: {error}", path.display()))?;
    #[cfg(unix)]
    fs::set_permissions(path, std::os::unix::fs::PermissionsExt::from_mode(0o700))
        .map_err(|error| format!("permissions {}: {error}", path.display()))?;
    Ok(())
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
    decoder.set_transformations(png::Transformations::normalize_to_color8());
    let mut reader = decoder
        .read_info()
        .map_err(|error| format!("read PNG {}: {error}", input.display()))?;
    let info = reader.info();
    let width = info.width;
    let height = info.height;
    let color_type = info.color_type;
    let bit_depth = info.bit_depth;
    let output_size = reader
        .output_buffer_size()
        .ok_or_else(|| "PNG output is too large".to_string())?;
    let mut buffer = vec![0; output_size];
    let frame = reader
        .next_frame(&mut buffer)
        .map_err(|error| format!("decode PNG {}: {error}", input.display()))?;
    let image_data = buffer[..frame.buffer_size()].to_vec();
    drop(reader);

    let output_file = OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(output)
        .map_err(|error| format!("create {}: {error}", output.display()))?;
    #[cfg(unix)]
    output_file
        .set_permissions(std::os::unix::fs::PermissionsExt::from_mode(0o600))
        .map_err(|error| format!("permissions {}: {error}", output.display()))?;
    let mut encoder = png::Encoder::new(BufWriter::new(output_file), width, height);
    encoder.set_color(color_type);
    encoder.set_depth(bit_depth);
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
    let destination = tier_path(root, ThumbnailTier::Fail, uri);
    let parent = destination
        .parent()
        .ok_or_else(|| "failure entry has no parent directory".to_string())?;
    private_directory(parent)?;
    let fixture = parent.join(format!(".failure-input-{}", std::process::id()));
    create_fixture_png(&fixture)?;
    let result = write_standard_thumbnail(
        &fixture,
        &destination,
        uri,
        version,
        Some("application/x-orbit-thumbnail-failure"),
    );
    let _ = fs::remove_file(fixture);
    result.map(|()| destination)
}

fn create_fixture_png(path: &Path) -> Result<(), String> {
    let file = File::create(path).map_err(|error| format!("create fixture: {error}"))?;
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
}
