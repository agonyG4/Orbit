#!/usr/bin/env python3
from pathlib import Path
import re, sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else 'source/AstreaOS/src/Apps/Explorer')
files = list(root.rglob('*.qml')) + list(root.rglob('*.js'))
processes = 0
imports = []
for path in files:
    text = path.read_text(encoding='utf-8', errors='replace')
    count = text.count('Process {')
    processes += count
    for line_no, line in enumerate(text.splitlines(), 1):
        if 'import Quickshell' in line:
            imports.append((path, line_no, line.strip()))
print(f'QML/JS files: {len(files)}')
print(f'Process nodes: {processes}')
print(f'Quickshell import lines: {len(imports)}')
for path, line_no, line in imports:
    print(f'{path}:{line_no}: {line}')
