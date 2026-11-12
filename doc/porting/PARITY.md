# Altap Salamander 4.0 x64 parity tracker

Reference confirmed by user screenshot on 2026-11-07. Use the user's Windows installation for manual behavior and layout comparison. Use `src/` and `help/src/hh/salamand/` as technical references; Open Salamander may differ from Altap 4.0. The user's custom paths, favorites and commands are configuration, not defaults to hardcode.

Goal: preserve workflows, controls, menus and toolbars on Linux wherever the platform provides an equivalent. Windows development remains with the official upstream. The baseline does not limit independent Linux improvements. Exact system fonts, native decorations and platform-specific integration may differ. Every exception must be recorded, not silently omitted. This is an initial inventory and must be expanded by walking each menu/dialog and plugin; it is not a claim of full coverage.

Statuses: partial = implemented subset; pending = not implemented; platform decision = requires an explicit OS-specific equivalent.

| Area | Current status | Remaining work / acceptance |
| --- | --- | --- |
| Native Linux Qt application | Partial | Linux locally compiled/tested; X11/Wayland desktop validation remains |
| Dual panels and splitter | Partial | Match proportions, focus indicators, density and detailed layout |
| Menu bar: Left, Files, Edit, Commands, Plugins, Options, Right, Help | Partial | Original menu order present; active-panel actions implemented; full command inventory remains |
| Top toolbar | Partial | Navigation, hidden files and path copy work; operations disabled; original icons/customization remain |
| Middle toolbar | Pending | Inventory actions and placement |
| Drive/mount toolbar | Partial | Mounted Linux volumes and free space loaded asynchronously; mounting/unmounting pending |
| User menu and favorite-directory bars | Partial | Standard places and session favorites work; persisted customization and user-command menu pending |
| Panel toolbars and path bars | Partial | Back/forward/up, editable history dropdown, theme-aware active-panel indicator and free space implemented; original customizations remain |
| Detailed columns: Name, Ext, Size, Date, Time, Attr | Partial | Six columns in original order; Linux permissions replace Windows attributes; locale/natural sorting options pending |
| Parent-directory `..` row | Partial | Enter navigates up; always first and excluded from marks; further original edge cases remain |
| Status lines and selection totals | Partial | Focused metadata, marked count and file bytes implemented; recursive directory sizes pending |
| Command line | Pending | Working directory, history, quoting, shell integration |
| Bottom function-key bar | Partial | F1/F2/F3/F5/F6/F7/F8/F10 work; remaining keys disabled until their operations exist |
| Focus separate from selected items | Partial | Keyboard covered; mouse/rubber-band modes and visual indicators pending |
| Insert and Space | Partial | Toggle and advance; background directory-size calculation pending |
| Shift+navigation | Partial | Select/unselect ranges; detailed keyboard fixtures added; other view modes pending |
| Ctrl+A / Ctrl+D | Partial | Basic all/none; configuration-specific directory inclusion pending |
| Select by mask / invert / restore selection | Pending | Match mask language and dialogs |
| Prefix quick search | Partial | Up/Down/Home/End, extend/shorten, Escape; wildcard aliases, Shift/Alt search combinations and IME pending |
| Sort by name / extension / date / size | Partial | Directories first both ways; Ctrl+F3/F4/F5/F6; locale/natural sorting, options and persistence pending |
| Sort by attributes | Partial | Header sorts basic Linux permission text; ACL/special bits and options pending |
| Detailed list focus and Full Row Select | Implemented | Name-area focus and marking by default, with a persistent full-row option for both panels |
| Navigation focus/selection memory | Partial | Focus memory retained; marks clear on navigation by default, with a persisted opt-in for session selection memory; large/slow/removable filesystem tests pending |
| Hidden files | Partial | Native Qt semantics; Linux dot files and Windows hidden attribute fixtures |
| View modes / zoom / thumbnails | Pending | Detailed, brief, icons, thumbnails, tiles and associated settings |
| Copy/move/rename/mkdir/delete/trash | Partial | F2/F7, F5 copy, F6 move and F8 desktop Trash run in background. F8 confirms first and reports partial results. Directory merging, permanent delete and complete cross-filesystem metadata support remain pending; see OPERATIONS.md, COPYING.md and MOVING.md |
| Filesystem watching and refresh | Partial | QFileSystemModel watching with selection-preservation regression; reconnect/slow-storage tests pending |
| Search/find dialog | Pending | Content/metadata searches, results actions, cancel and progress |
| Internal viewer / external editor | Partial | F3 views the focused file in a read-only text window with search, wrapping and a bounded preview. Hex mode, additional encodings and F4 editing remain pending |
| Archives / pack/unpack | Pending | Inventory built-ins and plugins; provider abstraction |
| Clipboard / drag-and-drop | Pending | Paths, files, URI lists, inter-panel/desktop operations |
| Compare/synchronize/checksums/rename tools | Pending | Inventory exact core/plugin boundaries |
| Settings / themes / shortcuts / translations | Partial | System/Light/Dark/Dracula, accents and custom JSON themes; selection preferences persist. Shortcut editing, migration and translations pending |
| Plugin system and supplied plugins | Pending | Inventory every plugin, dependencies and available source; no Windows DLL ABI promise |
| Registry / COM / Windows Shell integrations | Platform decision | Official upstream retains Windows integrations; decide Linux equivalents per command |
| Network paths / FTP / remote providers | Pending | Native paths plus plugin functionality and missing dependencies |
| Help / About / licenses | Pending | User-facing documentation and notices; existing LICENSE retained |
| Distribution | Pending | Versioning, runtime deployment, installers/packages, clean-machine tests, release notes |

## Evidence for this milestone

- `help/src/hh/salamand/basicwork_select.htm`: marking and range selection.
- `help/src/hh/salamand/basicwork_qsearch.htm`: prefix search, matching navigation and advanced wildcard behavior.
- `help/src/hh/salamand/basicwork_sort.htm`: directory-first default and Ctrl+F3/F4/F5/F6.
- `help/src/hh/salamand/shortcuts_keyboard.htm`: F3 views the focused file.
- `help/src/hh/salamand/environment_focus.htm`: focus distinct from selection, Backspace/navigation context.
- `src/fileswn0.cpp`: Insert/Space advance, range-selection rules and quick-search state.
- `port/tests/panel_tests.cpp`: automated temporary-directory keyboard/navigation/sort fixtures.

Automated tests are necessary but do not establish visual parity. Linux desktop acceptance against the Windows reference remains a separate check for each milestone. Windows builds of this fork are out of scope.

## Interface milestone — 2026-11-08

See `TESTING.md` for the manual acceptance checklist and fixture launcher. `port/tests/window_tests.cpp` covers shared actions/shortcuts, active-panel routing, favorites, menu order and unavailable operations. `DetailedFileModel` adds six presentation columns while preserving persistent selection indexes across source-model changes.

## Local operations milestone

F7 creates a single directory in the active location. F2 renames marked entries one at a time, or the focused entry. Original mass-rename masks, multi-level mkdir and remaining file operations are not yet implemented. Read OPERATIONS.md for collision, symlink, cancellation and partial-session policies.
