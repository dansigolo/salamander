# Upstream relationship and synchronization

This fork develops a native Linux Salamander. The official [OpenSalamander/salamander](https://github.com/OpenSalamander/salamander) project maintains Windows. Altap Salamander 4.0 x64 is our initial workflow/layout reference. Upstream parity is a baseline; Linux-specific improvements are welcome.

## Separate source integration from feature adaptation

- `origin`: `https://github.com/dansigolo/salamander.git`, our Linux development history.
- `upstream`: `https://github.com/OpenSalamander/salamander.git`, official `main` branch.
- `.upstream-sync.json`: last integrated original-source commit and known extracted-code mappings.
- `doc/porting/PARITY.md`: applicable features and Linux adaptation status.
- `doc/porting/upstream-reviews/`: per-integration review checklist, generated in candidate branches.

Bringing in upstream commits preserves source history and makes improvements available for review. It does not automatically update copied algorithms, translate WinAPI code to Qt, or complete the corresponding Linux features. Track these adaptations explicitly. Keep the upstream `src/` tree close to its origin and place Linux-specific code under `port/` to reduce conflicts.

## Initial setup in a new clone

```sh
git remote add upstream https://github.com/OpenSalamander/salamander.git
git fetch --no-tags upstream main
```

Do not add the remote again if it already exists. The synchronization tool checks that its URL matches the tracked configuration before fetching. It requires Python 3 and Git, with no additional Python packages.

## Inspect new changes

From the repository root:

```sh
python3 tools/upstream_sync.py check --fetch
# Optional Markdown report:
python3 tools/upstream_sync.py check --output build/upstream-report.md
```

`check` does not change source files or the current branch. `--fetch` updates the upstream remote references. The report lists commits and changed paths and flags known extracted source that may need changes in `port/`. These mappings are deliberately incomplete and must be expanded as code is extracted.

## Prepare an integration without disturbing current Linux work

Commit the current development work first, then choose a new worktree path:

```sh
python3 tools/upstream_sync.py prepare --fetch --worktree ../salamander-upstream-review
```

If no new upstream commits exist, no worktree is created. Otherwise, the tool creates `sync/upstream-<commit>` from the current HEAD and merges the upstream tip there using `--no-ff --no-commit`. It never commits, pushes or merges into the original development branch.

For a clean merge, it stages an updated source baseline and a review report in the candidate. For conflicts, it leaves them in that isolated worktree and retains the previous baseline. Resolve and stage conflicts there, then run:

```sh
python3 tools/upstream_sync.py --repo ../salamander-upstream-review record
```

Then, in the candidate worktree:

1. Review original changes, affected extractions and the generated adaptation checklist.
2. Update `PARITY.md`; record which improvements are ported, pending or not applicable. Implement Linux adaptations in focused commits, independently of the source merge.
3. Configure/build the Qt port and run CTest and the upstream-tool tests.
4. Inspect `git diff --cached`, finish the source merge commit and push the candidate branch.
5. Open a PR targeting the Linux development branch. Merge only after reviewing tests and changes. No automatic merge is configured.

A source-baseline marker means original history was integrated, not that all features were ported. Candidate branches and worktrees are never forcibly overwritten or automatically deleted. Inspect them before cleanup; do not use force to discard unresolved work.

If upstream history has been rewritten or the baseline cannot be found, the tool stops for review rather than choosing a new baseline.

## Monitoring workflow

`.github/workflows/upstream-watch.yml` runs a read-only report on relevant pushes, by manual dispatch, and weekly (Monday at 09:00 UTC). It uploads the report and writes the job summary. It does not push source, open issues, send messages or automatically merge.

GitHub schedules run from the repository's default branch. Until this workflow is reviewed and merged into this fork's default branch, its weekly schedule is not active. The local `check --fetch` command works immediately; the push-triggered report validates the mechanism on the development branch. No default-branch change is part of this milestone.

## Tests

```sh
python3 -m unittest discover -s tools/tests -p test_upstream_sync.py -v
```

Fixtures create temporary upstream/fork repositories and verify unchanged history, reports, clean merges, conflict isolation/resolution, dirty-worktree rejection and remote URL validation. They never access real upstream or modify this repository.
