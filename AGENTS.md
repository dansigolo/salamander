# Open Salamander development

The Windows application in `src/` remains the functional reference. The native Qt port targets Linux, is experimental, and lives in `port/`. The official upstream maintains the Windows product. Altap Salamander 4.0 x64 is the user-facing parity reference; the upstream Open Salamander source is the implementation reference.

- Use C++17, Qt >= 6.4 Widgets and CMake >= 3.21 for the port.
- Keep platform-independent algorithms in `port/core/`; do not include Windows or Qt GUI headers there.
- Keep Qt UI in `port/qt/`. File-operation scheduling and platform integrations must be separate services when introduced.
- Preserve the upstream license and attribution when extracting code. Document its origin and any semantic differences.
- Do not mass-reformat or change the existing Visual Studio build as part of port work.
- Follow the root `.clang-format`; new port files use UTF-8. Follow existing BOM conventions when modifying upstream files.
- Use meaningful tests for navigation, compatibility, filesystem semantics and operation safety. Use temporary directories for filesystem tests.
- Do not claim Windows plugin ABI compatibility or full feature parity without evidence.
- Never perform blocking file operations on the GUI thread. No copy/move/delete implementation without explicit collision, symlink, cancellation and failure policies.

Build: `cmake -S . -B build/qt -G Ninja -DCMAKE_BUILD_TYPE=Debug`
Compile: `cmake --build build/qt`
Test: `ctest --test-dir build/qt --output-on-failure`
Run: `./build/qt/salamander-qt`

Architecture and scope: `doc/porting/README.md`.

- Track parity explicitly in `doc/porting/PARITY.md`, including menus, toolbars, controls, keyboard behavior and plugins.
- Maintain Linux Qt CI. Windows is not a product target of this fork; preserve upstream source and avoid unnecessary divergence.
- Track upstream with `.upstream-sync.json` and `tools/upstream_sync.py`. Integrate source changes in an isolated review branch; extracted Qt/core code requires a separate adaptation review.
- Upstream parity is a baseline, not a ceiling: Linux-specific improvements are welcome with documented behavior and tests.
- Use focused commits on `improvement/salamander_for_linux`; do not force-push, change repository visibility, merge into main, or publish releases implicitly.
