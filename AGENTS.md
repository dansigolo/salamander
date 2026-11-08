# Open Salamander development

The Windows application in `src/` remains the functional reference. The native Qt port is experimental and lives in `port/`.

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
