# Contributing to the Qt port

The target experience is Altap Salamander 4.0 x64 on Windows and Linux. Read `AGENTS.md`, the architecture README and `PARITY.md` before changing behavior.

## Development cycle

1. Work on a focused branch. The initial integration branch is `improvement/salamander_for_linux`.
2. Implement a concrete feature and add regression fixtures for behavior or filesystem safety.
3. Run CMake build and CTest. Check `git diff --check` and review the exact staged diff.
4. Write a focused commit explaining the resulting behavior. Keep generated files, local paths, credentials and build directories out of Git.
5. Push the branch to your fork and open a PR for review before merging into main. Do not force-push a shared branch.
6. Check Qt CI on Windows and Linux; record manual GUI checks separately. Preserve the existing upstream Windows workflow.

A push is not a release. Publish downloads only after runtime packaging, licensing/dependency review and clean-machine testing. No releases or repository-visibility changes are part of this milestone.

## Tests

Use temporary directories for filesystem tests. Dot files are hidden by convention on Linux; Windows tests must set the actual hidden attribute. Do not depend on a particular locale, current user directory or installed desktop theme.

`ctest --test-dir build/qt --output-on-failure` runs headless Qt tests. Real desktop tests must also cover focus, shortcuts, sizing, X11/Wayland and Windows behavior.
