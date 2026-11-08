# Native Qt port: initial architecture and roadmap

Status: experimental native Windows/Linux Qt port. Altap Salamander 4.0 x64 is the functional and visual target, confirmed by the user on 2026-11-07. The Open Salamander source is the technical reference. Full parity is not yet implemented; see [PARITY.md](PARITY.md).

## Decisions

- C++17, Qt 6 Widgets (minimum 6.4), CMake 3.21 and Ninja.
- Support Windows x64 and Linux x64 from one Qt codebase. Develop locally on Linux Mint 22.3 / Ubuntu 24.04. Test Windows through CI; validate X11 and Wayland separately.
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
5. Extensions and distribution: classify each plugin, implement portable equivalents, test X11/Wayland and another distribution, add CI and packaging. Validate the original Windows build on a Windows runner.

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

## Windows Qt build

Install Visual Studio 2022 C++ tools, CMake and an MSVC x64 build of Qt >= 6.4. Set `CMAKE_PREFIX_PATH` to that Qt installation. For example, from a developer PowerShell:

```powershell
cmake -S . -B build/qt-windows -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build/qt-windows --config Debug
ctest --test-dir build/qt-windows -C Debug --output-on-failure
./build/qt-windows/Debug/salamander-qt.exe
```

The example path must match the actual installation. The Qt CI builds Linux with the distro Qt and Windows with Qt 6.8.3. Build artifacts are not self-contained release packages. Packaging, runtime deployment and clean-machine testing are later milestones.

## Version control

Development branch: `improvement/salamander_for_linux`. Keep focused commits, run CTest before pushing, and use pull requests before merging to main. See [CONTRIBUTING.md](CONTRIBUTING.md). Repository visibility and release publication are separate future decisions.
