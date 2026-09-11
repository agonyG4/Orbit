use serde_json::Value;
use std::fs;
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};

fn unique_root() -> std::path::PathBuf {
    let stamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("system clock before epoch")
        .as_nanos();
    std::env::temp_dir().join(format!("astrea-file-op-drag-{stamp}"))
}

fn run_move(
    destination: &std::path::Path,
    policy: &str,
    sources: &[&std::path::Path],
) -> Vec<Value> {
    let mut args = vec![
        "file-op".to_string(),
        "--json-events".to_string(),
        "move".to_string(),
        destination.to_str().unwrap().to_string(),
        policy.to_string(),
    ];
    args.extend(
        sources
            .iter()
            .map(|source| source.to_str().unwrap().to_string()),
    );
    let output = Command::new(env!("CARGO_BIN_EXE_explorer_backend"))
        .args(&args)
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "backend failed: {}",
        String::from_utf8_lossy(&output.stderr)
    );
    String::from_utf8(output.stdout)
        .unwrap()
        .lines()
        .map(|line| serde_json::from_str(line).unwrap())
        .collect()
}

#[test]
fn moves_two_sibling_directories_as_one_successful_batch() {
    let root = unique_root();
    let source_a = root.join("folder A");
    let source_b = root.join("folder-测试");
    let destination = root.join("destination");
    fs::create_dir_all(&source_a).unwrap();
    fs::create_dir_all(&source_b).unwrap();
    fs::create_dir_all(&destination).unwrap();
    fs::write(source_a.join("a.txt"), "a").unwrap();
    fs::write(source_b.join("b.txt"), "b").unwrap();

    let events = run_move(&destination, "keep-both", &[&source_a, &source_b]);
    let items: Vec<&Value> = events
        .iter()
        .filter(|event| event["event"] == "item")
        .collect();
    assert_eq!(items.len(), 2);
    assert!(items.iter().all(|event| event["status"] == "moved"));
    let done = events
        .iter()
        .find(|event| event["event"] == "done")
        .expect("missing done event");
    assert_eq!(done["done"], 2);
    assert_eq!(done["total"], 2);
    assert_eq!(done["state"], "success");

    assert_eq!(
        fs::read_to_string(destination.join("folder A/a.txt")).unwrap(),
        "a"
    );
    assert_eq!(
        fs::read_to_string(destination.join("folder-测试/b.txt")).unwrap(),
        "b"
    );
    assert!(!source_a.exists());
    assert!(!source_b.exists());
    let _ = fs::remove_dir_all(root);
}

#[test]
fn multi_source_conflicts_are_reported_and_keep_the_batch_together() {
    let skip_root = unique_root();
    let skip_a = skip_root.join("folder-a");
    let skip_b = skip_root.join("folder-b");
    let skip_destination = skip_root.join("destination");
    fs::create_dir_all(&skip_a).unwrap();
    fs::create_dir_all(&skip_b).unwrap();
    fs::create_dir_all(skip_destination.join("folder-a")).unwrap();
    fs::create_dir_all(skip_destination.join("folder-b")).unwrap();
    fs::write(skip_a.join("a.txt"), "a").unwrap();
    fs::write(skip_b.join("b.txt"), "b").unwrap();

    let skip_events = run_move(&skip_destination, "skip", &[&skip_a, &skip_b]);
    let skip_items: Vec<&Value> = skip_events
        .iter()
        .filter(|event| event["event"] == "item")
        .collect();
    assert_eq!(skip_items.len(), 2);
    assert!(skip_items.iter().all(|event| event["status"] == "skipped"));
    assert!(skip_a.exists());
    assert!(skip_b.exists());
    let _ = fs::remove_dir_all(skip_root);

    let keep_root = unique_root();
    let keep_a = keep_root.join("folder-a");
    let keep_b = keep_root.join("folder-b");
    let keep_destination = keep_root.join("destination");
    fs::create_dir_all(&keep_a).unwrap();
    fs::create_dir_all(&keep_b).unwrap();
    fs::create_dir_all(keep_destination.join("folder-a")).unwrap();
    fs::create_dir_all(keep_destination.join("folder-b")).unwrap();
    fs::write(keep_a.join("a.txt"), "a").unwrap();
    fs::write(keep_b.join("b.txt"), "b").unwrap();

    let keep_events = run_move(&keep_destination, "keep-both", &[&keep_a, &keep_b]);
    let keep_items: Vec<&Value> = keep_events
        .iter()
        .filter(|event| event["event"] == "item")
        .collect();
    assert_eq!(keep_items.len(), 2);
    assert!(keep_items.iter().all(|event| event["status"] == "moved"));
    for event in &keep_items {
        let target = event["target"].as_str().unwrap();
        assert!(std::path::Path::new(target).exists());
        assert!(!target.ends_with("/folder-a") && !target.ends_with("/folder-b"));
    }
    assert!(!keep_a.exists());
    assert!(!keep_b.exists());
    let _ = fs::remove_dir_all(keep_root);

    let merge_root = unique_root();
    let merge_a = merge_root.join("folder-a");
    let merge_b = merge_root.join("folder-b");
    let merge_destination = merge_root.join("destination");
    fs::create_dir_all(&merge_a).unwrap();
    fs::create_dir_all(&merge_b).unwrap();
    fs::create_dir_all(merge_destination.join("folder-a")).unwrap();
    fs::write(merge_destination.join("folder-a/existing.txt"), "existing").unwrap();
    fs::write(merge_a.join("a.txt"), "a").unwrap();
    fs::write(merge_b.join("b.txt"), "b").unwrap();

    let merge_events = run_move(&merge_destination, "merge", &[&merge_a, &merge_b]);
    let merge_items: Vec<&Value> = merge_events
        .iter()
        .filter(|event| event["event"] == "item")
        .collect();
    assert_eq!(merge_items.len(), 2);
    assert!(merge_items.iter().all(|event| event["status"] == "moved"));
    assert_eq!(
        fs::read_to_string(merge_destination.join("folder-a/existing.txt")).unwrap(),
        "existing"
    );
    assert_eq!(
        fs::read_to_string(merge_destination.join("folder-a/a.txt")).unwrap(),
        "a"
    );
    assert_eq!(
        fs::read_to_string(merge_destination.join("folder-b/b.txt")).unwrap(),
        "b"
    );
    assert!(!merge_a.exists());
    assert!(!merge_b.exists());
    let _ = fs::remove_dir_all(merge_root);
}
