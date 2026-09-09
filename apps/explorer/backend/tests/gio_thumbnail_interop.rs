use gio::prelude::*;
use std::fs::{self, File};
use std::io::BufWriter;
use std::path::Path;
use std::process::Command;

fn write_source_png(path: &Path) {
    let file = File::create(path).unwrap();
    let mut encoder = png::Encoder::new(BufWriter::new(file), 1, 1);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
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
    write_source_png(&source);
    if Command::new("magick").arg("-version").output().is_err() {
        eprintln!("SKIPPED: ImageMagick is unavailable");
        let _ = fs::remove_dir_all(root);
        return;
    }
    assert!(
        Command::new("touch")
            .args(["-d", "@1"])
            .arg(&source)
            .status()
            .unwrap()
            .success()
    );

    let backend = Command::new(env!("CARGO_BIN_EXE_explorer_backend"))
        .args(["thumbnail-batch", "128"])
        .arg(&source)
        .env("XDG_CACHE_HOME", &cache_home)
        .env("HOME", &root)
        .env("GIO_USE_VFS", "local")
        .output()
        .unwrap();
    assert!(
        backend.status.success(),
        "Orbit backend failed: {}",
        String::from_utf8_lossy(&backend.stderr)
    );
    let batch: serde_json::Value = serde_json::from_slice(&backend.stdout).unwrap();
    assert_eq!(batch["items"][0]["status"], "ready");
    let preview_url = batch["items"][0]["previewUrl"].as_str().unwrap();
    let produced_path = gio::File::for_uri(preview_url.split('?').next().unwrap())
        .path()
        .unwrap();

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
        produced_path.to_string_lossy()
    );

    let _ = fs::remove_dir_all(root);
}
