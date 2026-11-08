#!/usr/bin/env python3
"""Inspect upstream history or prepare an isolated source integration for review.

No command commits, pushes, or changes the caller's current development branch.
`prepare` creates a new worktree/branch; `record` only updates review metadata in
an already-prepared merge. Porting extracted code remains an explicit review.
"""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import json
from pathlib import Path
import subprocess
import sys

CONFIG = '.upstream-sync.json'


def git(repo, *args, check=True):
    result = subprocess.run(['git', '-C', str(repo), *args], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check and result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip())
    return result


def config(repo):
    value = json.loads((repo / CONFIG).read_text())
    if value.get('schema') != 1:
        raise RuntimeError('Unsupported upstream configuration schema')
    for key in ('remote', 'branch', 'integrated_commit'):
        if not value[key] or value[key].startswith('-'):
            raise RuntimeError(f'Invalid configuration field: {key}')
    actual = git(repo, 'remote', 'get-url', value['remote']).stdout.strip()
    if actual != value['url']:
        raise RuntimeError('Upstream URL differs from the tracked configuration; inspect before fetching')
    return value


def inspect(repo, fetch=False):
    value = config(repo)
    if fetch:
        git(repo, 'fetch', '--no-tags', value['remote'], value['branch'])
    ref = f"refs/remotes/{value['remote']}/{value['branch']}"
    tip = git(repo, 'rev-parse', '--verify', ref + '^{commit}').stdout.strip()
    base = git(repo, 'rev-parse', '--verify', value['integrated_commit'] + '^{commit}').stdout.strip()
    if git(repo, 'merge-base', '--is-ancestor', base, tip, check=False).returncode:
        raise RuntimeError('Tracked baseline is not an ancestor of upstream. History needs manual review.')
    return value, base, tip


def report(repo, value, base, tip):
    span = f'{base}..{tip}'
    commits = git(repo, 'log', '--reverse', '--format=%h %s', span).stdout.strip()
    files = git(repo, 'diff', '--name-only', base, tip).stdout.splitlines()
    changed = set(files)
    result = [
        '# Upstream source review', '',
        f"Repository: {value['url']} ({value['branch']})", '',
        f'Previously integrated source commit: `{base}`',
        f'Upstream source tip: `{tip}`', '',
        'No new upstream commits.' if base == tip else f'{len(files)} paths changed. Source integration does not establish Linux feature parity.', '',
        '## Commits', '', '```text', commits or '(none)', '```', '',
        '## Changed paths', '', '```text', '\n'.join(files) or '(none)', '```', '',
        '## Extracted code requiring manual review', ''
    ]
    flagged = False
    for item in value.get('extractions', []):
        if item['source'] in changed:
            flagged = True
            result.append(f"- [ ] Review `{item['source']}` against `{item['destination']}`: {item['note']}")
    if not flagged:
        result.append('No tracked extracted source changed in this range. This is not a complete dependency analysis.')
    result += ['', '## Linux adaptation checklist', '',
               '- [ ] Classify bug fixes, new features, dependencies, resources, docs and Windows-only changes.',
               '- [ ] Update the parity tracker and create Linux tasks for applicable improvements.',
               '- [ ] Review tracked extractions and any other affected Qt/core code.',
               '- [ ] Build and test the Linux application; verify affected UI workflows.',
               '- [ ] Record independent Linux changes that must remain intact.',
               '- [ ] Review the full merge diff before committing and opening a PR.', '',
               'The baseline records integrated original source history, not completed Linux adaptations.', '']
    return '\n'.join(result)


def record(repo):
    value, base, tip = inspect(repo)
    merge = git(repo, 'rev-parse', '--verify', 'MERGE_HEAD', check=False)
    if merge.returncode or merge.stdout.strip() != tip:
        raise RuntimeError('record requires the prepared merge of the currently tracked upstream tip')
    conflicts = git(repo, 'diff', '--name-only', '--diff-filter=U').stdout.strip()
    if conflicts:
        raise RuntimeError('Resolve and stage all merge conflicts before recording the source baseline')
    text = report(repo, value, base, tip)
    value['integrated_commit'] = tip
    (repo / CONFIG).write_text(json.dumps(value, indent=2) + '\n')
    relative = Path('doc/porting/upstream-reviews') / (tip[:12] + '.md')
    target = repo / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text)
    git(repo, 'add', '--', CONFIG, relative.as_posix())
    print(f'Review recorded at {target}. The merge is not committed or pushed.')


def prepare(repo, destination, fetch=False):
    value, base, tip = inspect(repo, fetch)
    if base == tip:
        print('No new upstream commits; no worktree created.')
        return 0
    if git(repo, 'status', '--porcelain').stdout.strip():
        raise RuntimeError('Commit current development changes before preparing an integration')
    if destination.exists():
        raise RuntimeError('Destination already exists; refusing to reuse it')
    branch = 'sync/upstream-' + tip[:12]
    git(repo, 'worktree', 'add', '-b', branch, str(destination), 'HEAD')
    merged = git(destination, 'merge', '--no-ff', '--no-commit', tip, check=False)
    if merged.returncode:
        print(merged.stdout + merged.stderr, file=sys.stderr)
        print(f'Integration needs review in {destination}. The original worktree is unchanged.', file=sys.stderr)
        print('Resolve and stage conflicts there, then run this tool with --repo THAT_WORKTREE record.', file=sys.stderr)
        return 2
    record(destination)
    print(f'Candidate branch: {branch}\nWorktree: {destination}')
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[1])
    sub = parser.add_subparsers(dest='command', required=True)
    check = sub.add_parser('check', help='Report commits and changes, without modifying source')
    check.add_argument('--fetch', action='store_true', help='Fetch the configured upstream before inspecting')
    check.add_argument('--output', type=Path, help='Write Markdown report instead of stdout')
    candidate = sub.add_parser('prepare', help='Merge upstream in a new isolated worktree, without committing')
    candidate.add_argument('--fetch', action='store_true')
    candidate.add_argument('--worktree', required=True, type=Path)
    sub.add_parser('record', help='Record metadata after resolving a prepared merge conflict')
    args = parser.parse_args()
    repo = args.repo.resolve()
    try:
        if args.command == 'check':
            value, base, tip = inspect(repo, args.fetch)
            text = report(repo, value, base, tip)
            if args.output:
                args.output.parent.mkdir(parents=True, exist_ok=True)
                args.output.write_text(text)
            else:
                print(text)
        elif args.command == 'prepare':
            return prepare(repo, args.worktree.resolve(), args.fetch)
        else:
            record(repo)
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        print(f'Upstream sync: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
