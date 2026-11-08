# SPDX-License-Identifier: GPL-2.0-or-later
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).resolve().parents[1] / 'upstream_sync.py'


class UpstreamTests(unittest.TestCase):
    def git(self, repo, *args):
        return subprocess.check_output(['git', '-C', str(repo), *args], stderr=subprocess.PIPE, text=True).strip()

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.upstream = self.root / 'upstream'
        self.upstream.mkdir()
        self.git(self.upstream, 'init', '-b', 'main')
        self.identity(self.upstream)
        (self.upstream / 'source.txt').write_text('original\n')
        self.commit(self.upstream, 'initial')
        self.base = self.git(self.upstream, 'rev-parse', 'HEAD')
        self.fork = self.root / 'fork'
        subprocess.run(['git', 'clone', str(self.upstream), str(self.fork)], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.identity(self.fork)
        self.git(self.fork, 'remote', 'rename', 'origin', 'upstream')
        self.git(self.fork, 'switch', '-c', 'linux')
        self.config = {'schema': 1, 'remote': 'upstream', 'url': str(self.upstream), 'branch': 'main', 'integrated_commit': self.base,
                       'extractions': [{'source': 'source.txt', 'destination': 'linux.txt', 'note': 'Review manually'}]}
        (self.fork / '.upstream-sync.json').write_text(json.dumps(self.config))
        (self.fork / 'linux.txt').write_text('independent Linux feature\n')
        self.commit(self.fork, 'linux work')

    def identity(self, repo):
        self.git(repo, 'config', 'user.name', 'Test Fixture')
        self.git(repo, 'config', 'user.email', 'test@example.invalid')
        self.git(repo, 'config', 'commit.gpgsign', 'false')
        self.git(repo, 'config', 'core.hooksPath', str(self.root / 'no-hooks'))

    def commit(self, repo, message):
        self.git(repo, 'add', '.')
        self.git(repo, 'commit', '-m', message)

    def run_tool(self, *args, repo=None):
        return subprocess.run([sys.executable, str(TOOL), '--repo', str(repo or self.fork), *args], capture_output=True, text=True)

    def advance(self):
        (self.upstream / 'source.txt').write_text('upstream improvement\n')
        self.commit(self.upstream, 'improve original')
        return self.git(self.upstream, 'rev-parse', 'HEAD')

    def test_no_changes_creates_no_candidate(self):
        destination = self.root / 'candidate'
        result = self.run_tool('prepare', '--worktree', str(destination))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(destination.exists())

    def test_report_and_isolated_clean_merge(self):
        tip = self.advance()
        original = self.git(self.fork, 'rev-parse', 'HEAD')
        report = self.run_tool('check', '--fetch')
        self.assertEqual(report.returncode, 0, report.stderr)
        self.assertIn('source.txt', report.stdout)
        self.assertIn('linux.txt', report.stdout)
        destination = self.root / 'candidate'
        result = self.run_tool('prepare', '--worktree', str(destination))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.git(self.fork, 'rev-parse', 'HEAD'), original)
        self.assertEqual((self.fork / 'source.txt').read_text(), 'original\n')
        self.assertEqual((destination / 'linux.txt').read_text(), 'independent Linux feature\n')
        self.assertEqual((destination / 'source.txt').read_text(), 'upstream improvement\n')
        self.assertEqual(json.loads((destination / '.upstream-sync.json').read_text())['integrated_commit'], tip)
        self.assertEqual(self.git(destination, 'rev-parse', 'HEAD'), original)  # No auto-commit.
        self.assertEqual(self.git(destination, 'rev-parse', 'MERGE_HEAD'), tip)
        self.assertEqual(self.git(self.fork, 'status', '--porcelain'), '')

    def test_conflicts_stay_isolated_and_can_be_recorded_after_resolution(self):
        (self.fork / 'source.txt').write_text('Linux-side change\n')
        self.commit(self.fork, 'independent source change')
        tip = self.advance()
        destination = self.root / 'candidate'
        result = self.run_tool('prepare', '--fetch', '--worktree', str(destination))
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual((self.fork / 'source.txt').read_text(), 'Linux-side change\n')
        self.assertEqual(json.loads((destination / '.upstream-sync.json').read_text())['integrated_commit'], self.base)
        self.assertNotEqual(self.run_tool('record', repo=destination).returncode, 0)
        (destination / 'source.txt').write_text('reviewed resolution\n')
        self.git(destination, 'add', 'source.txt')
        finished = self.run_tool('record', repo=destination)
        self.assertEqual(finished.returncode, 0, finished.stderr)
        self.assertEqual(json.loads((destination / '.upstream-sync.json').read_text())['integrated_commit'], tip)
        self.assertEqual(self.git(self.fork, 'status', '--porcelain'), '')

    def test_dirty_source_refuses_candidate(self):
        self.advance()
        (self.fork / 'linux.txt').write_text('uncommitted work\n')
        destination = self.root / 'candidate'
        result = self.run_tool('prepare', '--fetch', '--worktree', str(destination))
        self.assertEqual(result.returncode, 1)
        self.assertFalse(destination.exists())

    def test_remote_url_mismatch_refuses_fetch(self):
        self.config['url'] = 'https://example.invalid/unexpected.git'
        (self.fork / '.upstream-sync.json').write_text(json.dumps(self.config))
        result = self.run_tool('check', '--fetch')
        self.assertEqual(result.returncode, 1)
        self.assertIn('URL differs', result.stderr)


if __name__ == '__main__':
    unittest.main()
