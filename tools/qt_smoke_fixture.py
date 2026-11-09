#!/usr/bin/env python3
"""Create disposable files for manually testing the Linux Qt application."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--launch', action='store_true', help='Open the built Qt application with both fixture panels')
    parser.add_argument('--binary', type=Path, default=Path(__file__).resolve().parents[1] / 'build/qt/salamander-qt')
    args = parser.parse_args()
    binary = args.binary.resolve()
    if args.launch and not binary.is_file():
        parser.error(f'Build the application first; binary not found: {binary}')
    root = Path(tempfile.mkdtemp(prefix='salamander-linux-test-'))
    left, right = root / 'left', root / 'right'
    (left / 'folder-a' / 'child').mkdir(parents=True)
    (left / 'folder-b').mkdir()
    (left / '.hidden-folder').mkdir()
    (right / 'destination').mkdir(parents=True)
    for name, content in {
        'alpha.txt': 'First test file\n',
        'alphabet.txt': 'Second prefix match\n',
        'beta.md': '# Salamander Linux\n',
        'archive.tar.gz': 'Sample filename only; this is not a real archive.\n',
        'relatório.txt': 'Unicode filename fixture\n',
        'name with spaces.txt': 'Spaces in a filename\n',
        '.hidden': 'Hidden file fixture\n',
        '.config.json': '{}\n',
        'run-me.sh': '#!/bin/sh\nprintf "fixture\\n"\n',
    }.items():
        (left / name).write_text(content, encoding='utf-8')
    (left / 'run-me.sh').chmod(0o755)
    (left / 'alpha-link.txt').symlink_to('alpha.txt')
    (right / 'README.txt').write_text('Independent right panel\n')
    command = [str(binary), str(left), str(right)]
    print(f'Test directory: {root}', flush=True)
    print('Launch: ' + shlex.join(command), flush=True)
    print('Fixtures are kept after exit so you can inspect them. No existing files were changed.', flush=True)
    if args.launch:
        return subprocess.run(command, check=False).returncode
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
