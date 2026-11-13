#!/usr/bin/env python3
"""Create disposable files for manually testing the Linux Qt application."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import zlib


def write_test_image(path):
    """Write an RGB test pattern with no third-party Python dependencies."""
    width, height = 960, 540
    colors = [(58, 99, 179), (35, 155, 125), (231, 173, 55), (159, 88, 176)]
    rows = []
    for y in range(height):
        row = bytearray([0])  # PNG scanline filter: None
        for x in range(width):
            color = colors[(x // 240 + y // 135) % len(colors)]
            row.extend(color)
        rows.append(row)

    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))

    header = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header)
                     + chunk(b'IDAT', zlib.compress(b''.join(rows))) + chunk(b'IEND', b''))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--launch', action='store_true', help='Open the built Qt application with both fixture panels')
    parser.add_argument('--binary', type=Path, default=Path(__file__).resolve().parents[1] / 'build/qt/salamander-qt')
    parser.add_argument('--large-mib', type=int, default=0, help='Add a sparse file for copy/cancellation tests (0..4096 MiB)')
    args = parser.parse_args()
    if not 0 <= args.large_mib <= 4096:
        parser.error('--large-mib must be between 0 and 4096')
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
    write_test_image(left / 'imagem-teste.png')
    if args.large_mib:
        with (left / 'large-copy-test.bin').open('wb') as large:
            large.truncate(args.large_mib * 1024 * 1024)
    command = [str(binary), str(left), str(right)]
    print(f'Test directory: {root}', flush=True)
    print('Launch: ' + shlex.join(command), flush=True)
    print('Fixtures are kept after exit so you can inspect them. No existing files were changed.', flush=True)
    if args.launch:
        return subprocess.run(command, check=False).returncode
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
