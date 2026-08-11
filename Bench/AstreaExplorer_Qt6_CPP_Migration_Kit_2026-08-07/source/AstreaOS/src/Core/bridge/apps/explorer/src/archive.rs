use std::fs;
use std::path::{Component, Path, PathBuf};
use std::process::Command;

use crate::json;

pub fn run_extract(args: &[String]) -> Result<(), String> {
    println!("{}", extract(args)?);
    Ok(())
}

pub fn run_compress(args: &[String]) -> Result<(), String> {
    println!("{}", compress(args)?);
    Ok(())
}

pub fn extract(args: &[String]) -> Result<String, String> {
    let archive = args.first().ok_or_else(|| {
        "usage: archive-extract <archive> <destination> [password] [policy]".to_string()
    })?;
    let destination = args
        .get(1)
        .ok_or_else(|| "missing extraction destination".to_string())?;
    let password = args.get(2).map(String::as_str).unwrap_or_default();
    let policy = args.get(3).map(String::as_str).unwrap_or("keep-both");
    let archive = Path::new(archive);
    let destination = Path::new(destination);
    if !archive.is_file() {
        return Err(format!("archive not found: {}", archive.display()));
    }
    if let Some(parent) = destination.parent() {
        fs::create_dir_all(parent).map_err(|error| format!("create extraction parent: {error}"))?;
    }

    let listing = Command::new("bsdtar")
        .args(["-tf"])
        .arg(archive)
        .output()
        .map_err(|error| format!("list archive: {error}"))?;
    if !listing.status.success() {
        return Err(String::from_utf8_lossy(&listing.stderr).trim().to_string());
    }
    for raw in String::from_utf8_lossy(&listing.stdout).lines() {
        validate_member(raw)?;
    }

    // Never publish an archive containing links. A staged extraction prevents
    // path traversal, while rejecting links also prevents an archive member
    // from redirecting later writes outside the staged tree.
    let verbose_listing = Command::new("bsdtar")
        .args(["-tvf"])
        .arg(archive)
        .output()
        .map_err(|error| format!("inspect archive links: {error}"))?;
    if !verbose_listing.status.success() {
        return Err(String::from_utf8_lossy(&verbose_listing.stderr)
            .trim()
            .to_string());
    }
    for line in String::from_utf8_lossy(&verbose_listing.stdout).lines() {
        if line.starts_with('l') || line.starts_with('h') || line.contains(" -> ") {
            return Err("archives containing symbolic or hard links are not supported".into());
        }
    }

    let target = choose_target(destination, policy)?;
    let stage = target
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .join(format!(".astrea-extract-{}", std::process::id()));
    let _ = fs::remove_dir_all(&stage);
    fs::create_dir_all(&stage)
        .map_err(|error| format!("create extraction staging directory: {error}"))?;

    let result = if password.is_empty() {
        Command::new("bsdtar")
            .args(["-xf"])
            .arg(archive)
            .args(["-C"])
            .arg(&stage)
            .output()
    } else {
        Command::new("7z")
            .args(["x", "-y"])
            .arg(format!("-p{password}"))
            .arg(archive)
            .arg(format!("-o{}", stage.display()))
            .output()
    };
    let output = result.map_err(|error| format!("extract archive: {error}"))?;
    if !output.status.success() {
        let _ = fs::remove_dir_all(&stage);
        return Err(String::from_utf8_lossy(&output.stderr).trim().to_string());
    }
    if target.exists() {
        if policy == "overwrite" {
            remove_path(&target)?;
        } else {
            return Err(format!(
                "extraction target already exists: {}",
                target.display()
            ));
        }
    }
    fs::rename(&stage, &target).map_err(|error| format!("publish extracted archive: {error}"))?;
    Ok(format!(
        "{{\"ok\":true,\"operation\":\"archive-extract\",\"destination\":\"{}\"}}",
        json::escape(&target.to_string_lossy())
    ))
}

pub fn compress(args: &[String]) -> Result<String, String> {
    let source = args
        .first()
        .ok_or_else(|| "usage: archive-compress <source> <archive> [format]".to_string())?;
    let output = args
        .get(1)
        .ok_or_else(|| "missing archive output".to_string())?;
    let source = Path::new(source);
    let output = Path::new(output);
    if !source.exists() {
        return Err(format!("source not found: {}", source.display()));
    }
    if let Some(parent) = output.parent() {
        fs::create_dir_all(parent).map_err(|error| format!("create archive parent: {error}"))?;
    }
    let temp = output.with_extension(format!(
        "{}.tmp-{}",
        output
            .extension()
            .and_then(|v| v.to_str())
            .unwrap_or("archive"),
        std::process::id()
    ));
    let parent = source.parent().unwrap_or_else(|| Path::new("."));
    let name = source
        .file_name()
        .ok_or_else(|| "source has no file name".to_string())?;
    let result = Command::new("bsdtar")
        .args(["-caf"])
        .arg(&temp)
        .args(["-C"])
        .arg(parent)
        .arg(name)
        .output()
        .map_err(|error| format!("compress archive: {error}"))?;
    if !result.status.success() {
        let _ = fs::remove_file(&temp);
        return Err(String::from_utf8_lossy(&result.stderr).trim().to_string());
    }
    fs::rename(&temp, output).map_err(|error| format!("publish archive: {error}"))?;
    Ok(format!(
        "{{\"ok\":true,\"operation\":\"archive-compress\",\"path\":\"{}\"}}",
        json::escape(&output.to_string_lossy())
    ))
}

fn validate_member(value: &str) -> Result<(), String> {
    let path = Path::new(value);
    if value.is_empty() || path.is_absolute() {
        return Err(format!("unsafe archive member: {value:?}"));
    }
    for component in path.components() {
        if matches!(component, Component::ParentDir) {
            return Err(format!("archive member escapes destination: {value}"));
        }
    }
    Ok(())
}

fn choose_target(destination: &Path, policy: &str) -> Result<PathBuf, String> {
    if !destination.exists() || policy == "overwrite" {
        return Ok(destination.to_path_buf());
    }
    if policy != "keep-both" && policy != "rename" {
        return Err(format!(
            "extraction target already exists: {}",
            destination.display()
        ));
    }
    for index in 2..10000 {
        let name = destination
            .file_name()
            .and_then(|v| v.to_str())
            .unwrap_or("extracted");
        let candidate = destination.with_file_name(format!("{name} ({index})"));
        if !candidate.exists() {
            return Ok(candidate);
        }
    }
    Err("could not choose a unique extraction target".into())
}

fn remove_path(path: &Path) -> Result<(), String> {
    if path.is_dir() && !path.is_symlink() {
        fs::remove_dir_all(path).map_err(|error| format!("remove existing target: {error}"))
    } else {
        fs::remove_file(path).map_err(|error| format!("remove existing target: {error}"))
    }
}

#[cfg(test)]
mod tests {
    use super::validate_member;

    #[test]
    fn rejects_absolute_and_parent_archive_members() {
        assert!(validate_member("/tmp/escape").is_err());
        assert!(validate_member("../escape").is_err());
        assert!(validate_member("nested/../../escape").is_err());
    }

    #[test]
    fn accepts_normal_archive_members() {
        assert!(validate_member("folder/file.txt").is_ok());
    }
}
