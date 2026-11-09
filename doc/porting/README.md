# Native Qt port: initial architecture and roadmap

Status: experimental native Linux Qt port. Altap Salamander 4.0 x64 is the functional and visual target, confirmed by the user on 2026-11-07. The Open Salamander source is the technical reference. Full parity is not yet implemented; see [PARITY.md](PARITY.md).

## Decisions

- C++17, Qt 6 Widgets (minimum 6.4), CMake 3.21 and Ninja.
- Target Linux x64 first, developing on Linux Mint 22.3 / Ubuntu 24.04. Windows is maintained by the official upstream. Keep platform boundaries clean without promising a Windows release from this fork. Validate X11 and Wayland separately.
- Track and integrate upstream source history with `.upstream-sync.json` and `tools/upstream_sync.py`; review extracted algorithms and port behavior independently. Linux improvements may advance beyond upstream features.
- Incrementally extract reusable algorithms rather than replacing all code or emulating WinAPI.
- Keep the Visual Studio build untouched. The root CMake project builds only the experimental port.
- Core algorithms in `port/core/`, Widgets presentation in `port/qt/`, tests in `port/tests/`.
- Introduce an operation service and filesystem-provider interface when implementing file operations and archive navigation. They must not depend on panel widgets.
- QFileSystemModel is the local-directory bootstrap model; it is not the final archive/plugin abstraction.
- Use a dedicated native plugin contract in a later milestone. Existing Windows DLLs are not supported by this prototype.

## Implemented so far

Two independent directory panels, path entry, marked items separate from focus, Insert/Space toggle-and-advance, Shift navigation selection, Ctrl+A/Ctrl+D, prefix quick search, directories-first sorting, directory activation, parent navigation, hidden-file toggle and focused-panel switching. Each panel remembers focus and selection for previously visited directories during the session. Directory enumeration uses QFileSystemModel. Editing and drag/drop are disabled. No file-operation commands exist.

`port/core/mask.cpp` extracts `PrepareMask` from `src/masks.cpp` at upstream commit `a201fa3cc708c74dc1f4eea7032e0758840447a1`, removing the diagnostic macro and changing only the function name/namespace. The algorithm is tested independently of Qt. It is not yet wired into the UI: complete wildcard matching and Unicode policy remain open.

The UI is newly implemented. The first bootstrap was manually accepted on Linux Mint by the user. Subsequent behavior is checked against upstream source/help and automated fixtures; it still needs manual comparison against Altap 4.0. Directory size calculation on Space, full wildcard quick search, advanced sorting and exact mouse-selection behavior remain pending. One extracted function does not imply that the original engine has been ported.

## Milestones and acceptance

1. Bootstrap: configure, compile, run, and pass temporary-directory tests; manually verify keyboard focus, resizing and navigation on the host desktop.
2. Behavioral baseline: inventory shortcuts, selection, sorting, quick search and dialogs against Windows Salamander; record Linux equivalences and add regression tests. Introduce a panel controller and reusable filesystem provider only as needed.
3. Safe local operations: copy, move, rename, mkdir and trash through a background operation service. Define overwrite decisions, partial failures, cancellation, permissions, symlinks, hard links and cross-filesystem moves before implementing destructive commands. Test only inside temporary fixtures.
4. Core features: search, viewer, masks, archives and persistent settings. Profile large directories and slow/network storage.
5. Extensions and distribution: classify each plugin, implement portable equivalents, test X11/Wayland and another distribution, add CI and packaging. Compare Linux behavior with the upstream reference and review platform-specific exceptions.

Full Linux parity requires an explicit feature matrix. Windows-only facilities (Registry, COM, shell extensions, NTFS-specific features) require individual decisions. No completion date is estimated yet.

## Build and run

Install baseline packages on Mint/Ubuntu:

```sh
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config qt6-base-dev qt6-base-dev-tools qt6-wayland
cmake -S . -B build/qt -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/qt
ctest --test-dir build/qt --output-on-failure
./build/qt/salamander-qt
# Optional initial left and right directories:
./build/qt/salamander-qt /tmp /usr
```

Tests use the offscreen Qt platform; they do not validate actual X11/Wayland rendering. Qt Creator is optional. VS Code, Kiro or command-line tools may use the same CMake build.

## Manual acceptance checklist (pending)

- Tab switches panels when the file view has focus; text fields retain ordinary focus traversal.
- Enter activates directories; files are not launched in this bootstrap.
- Backspace navigates to the parent (when quick search is inactive); Alt+Up/Down moves among marked items; Ctrl+H toggles hidden files in both panels. This corrects the prototype shortcut to match Salamander.
- Insert/Space toggle marking and advance; arrows retain marks; Shift+navigation selects or unselects based on the initial item.
- Typing starts prefix quick search; Up/Down finds matching names; Right extends, Left/Backspace shortens and Escape exits. Full wildcard/IME behavior is pending.
- Ctrl+F3/F4/F5/F6 sort the active panel by name/extension/date/size; repeating reverses the order.
- Invalid directory input leaves the current location intact and displays an error.
- Unicode directory names, symlink navigation, large directories, inaccessible paths and disconnected mounts need additional coverage.
- Appearance, focus and shortcuts need manual checks in X11 and Wayland.

## Upstream relationship

The official Windows project is [OpenSalamander/salamander](https://github.com/OpenSalamander/salamander). This fork focuses on Linux, preserving source history and tracing adopted improvements. See [UPSTREAM.md](UPSTREAM.md) for checking new commits, preparing isolated integration worktrees and recording adaptation decisions. Matching Altap 4.0 workflows is the initial baseline; Linux-specific improvements are encouraged.

## Version control

Development branch: `improvement/salamander_for_linux`. Keep focused commits, run CTest before pushing, and use pull requests before merging to main. See [CONTRIBUTING.md](CONTRIBUTING.md). Repository visibility and release publication are separate future decisions.

## Interface milestone

The Qt window now follows the original menu order and includes top, places/volumes and function-key bars. Panels expose Name/Ext/Size/Date/Time/Attr, a nonselectable parent row, back/forward history, path dropdowns, active-panel indication and asynchronous free-space queries. POSIX permissions replace Windows attributes. Favorites added here are session-only. The Go to field navigates paths; it does not execute commands. Unimplemented file operations are disabled.

See [TESTING.md](TESTING.md) for compilation, disposable fixtures and a complete manual/automated test guide. Optional fixture launcher: `python3 tools/qt_smoke_fixture.py --launch`.
