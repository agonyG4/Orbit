use gio::prelude::*;
use std::fs::{self, File};
use std::io::BufWriter;
use std::path::Path;
use std::process::Command;

fn write_standard_thumbnail(path: &Path, uri: &str, modified_secs: i64, size: u64) {
    let file = File::create(path).unwrap();
    let mut encoder = png::Encoder::new(BufWriter::new(file), 1, 1);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    encoder
        .add_text_chunk("Thumb::URI".to_string(), uri.to_string())
        .unwrap();
    encoder
        .add_text_chunk("Thumb::MTime".to_string(), modified_secs.to_string())
        .unwrap();
    encoder
        .add_text_chunk("Thumb::Size".to_string(), size.to_string())
        .unwrap();
    encoder
        .add_text_chunk("Software".to_string(), "Orbit Explorer".to_string())
        .unwrap();
    let mut writer = encoder.write_header().unwrap();
    writer.write_image_data(&[255, 0, 0, 255]).unwrap();
}

#[test]
fn gio_reads_orbit_standard_cache_without_attribute_injection() {
    let root = std::env::temp_dir().join(format!(
        "orbit-gio-thumbnail-{}-{}",
        std::process::id(),
        std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos()
    ));
    let cache_home = root.join("cache-home");
    let source = root.join("photo.png");
    fs::create_dir_all(&cache_home).unwrap();
    fs::write(&source, b"source").unwrap();

    let source_metadata = fs::metadata(&source).unwrap();
    let modified_secs = source_metadata
        .modified()
        .unwrap()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs() as i64;
    let uri = gio::File::for_path(&source).uri().to_string();
    let cache_path = cache_home
        .join("thumbnails")
        .join("normal")
        .join(format!("{:x}.png", md5::compute(uri.as_bytes())));
    fs::create_dir_all(cache_path.parent().unwrap()).unwrap();
    write_standard_thumbnail(&cache_path, &uri, modified_secs, source_metadata.len());

    let probe = Command::new(env!("CARGO_BIN_EXE_gio_thumbnail_probe"))
        .arg(&source)
        .env("XDG_CACHE_HOME", &cache_home)
        .env("HOME", &root)
        .env("GIO_USE_VFS", "local")
        .output()
        .unwrap();
    assert!(
        probe.status.success(),
        "GIO probe failed: {}",
        String::from_utf8_lossy(&probe.stderr)
    );
    assert_eq!(
        String::from_utf8_lossy(&probe.stdout).trim(),
        cache_path.to_string_lossy()
    );

    let _ = fs::remove_dir_all(root);
}
