#!/usr/bin/env python3
from pathlib import Path
import hashlib, os, sys

root = Path(__file__).resolve().parents[1]
manifest = root / 'manifests' / 'SOURCE_SHA256.txt'
failed = 0
for raw in manifest.read_text(encoding='utf-8').splitlines():
    if not raw.strip():
        continue
    expected, rel = raw.split('  ', 1)
    path = root / rel
    if expected.startswith('SYMLINK->'):
        target = expected[len('SYMLINK->'):]
        if not path.is_symlink() or os.readlink(path) != target:
            print(f'FAIL symlink {rel}')
            failed += 1
        continue
    if not path.is_file():
        print(f'MISSING {rel}')
        failed += 1
        continue
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != expected:
        print(f'FAIL {rel}: {actual} != {expected}')
        failed += 1
print('OK' if failed == 0 else f'FAILED: {failed}')
raise SystemExit(1 if failed else 0)
