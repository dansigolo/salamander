# Altap Salamander 4.0 x64 parity tracker

Reference confirmed by user screenshot on 2026-11-07. Use the user's Windows installation for manual behavior and layout comparison. Use `src/` and `help/src/hh/salamand/` as technical references; Open Salamander may differ from Altap 4.0. The user's custom paths, favorites and commands are configuration, not defaults to hardcode.

Goal: preserve workflows, controls, menus and toolbars on Linux wherever the platform provides an equivalent. Windows development remains with the official upstream. The baseline does not limit independent Linux improvements. Exact system fonts, native decorations and platform-specific integration may differ. Every exception must be recorded, not silently omitted. This is an initial inventory and must be expanded by walking each menu/dialog and plugin; it is not a claim of full coverage.

Statuses: partial = implemented subset; pending = not implemented; platform decision = requires an explicit OS-specific equivalent.

| Area | Current status | Remaining work / acceptance |
| --- | --- | --- |
| Native Linux Qt application | Partial | Linux locally compiled/tested; X11/Wayland desktop validation remains |
| Dual panels and splitter | Partial | Match proportions, focus indicators, density and detailed layout |
| Menu bar: Left, Files, Edit, Commands, Plugins, Options, Right, Help | Partial | Prototype File/View menus only; inventory and implement actual commands |
| Top toolbar | Pending | Match original actions, icons, separators, tooltips, customization |
| Middle toolbar | Pending | Inventory actions and placement |
| Drive/mount toolbar | Platform decision | Windows drives; Linux mounts/volumes and free space |
| User menu and favorite-directory bars | Pending | Configurable shortcuts, icons and folders; no hardcoded user paths |
| Panel toolbars and path bars | Partial | Original controls, history, dropdowns, free-space indicators |
| Detailed columns: Name, Ext, Size, Date, Time, Attr | Partial | Extension supported; exact column order, separate time and OS attributes pending |
| Parent-directory `..` row | Pending | Current Up button/Backspace works; add original row semantics |
| Status lines and selection totals | Partial | Counts only; file metadata and selected bytes pending |
| Command line | Pending | Working directory, history, quoting, shell integration |
| Bottom function-key bar | Pending | F1 Help, F2 Rename, F3 View, F4 Edit, F5 Copy, F6 Move, F7 MakeDir, F8 Delete, F9 UserMenu, F10 Menu, F11 Connect, F12 Disconnect |
| Focus separate from selected items | Partial | Keyboard covered; mouse/rubber-band modes and visual indicators pending |
| Insert and Space | Partial | Toggle and advance; background directory-size calculation pending |
| Shift+navigation | Partial | Select/unselect ranges; detailed keyboard fixtures added; other view modes pending |
| Ctrl+A / Ctrl+D | Partial | Basic all/none; configuration-specific directory inclusion pending |
| Select by mask / invert / restore selection | Pending | Match mask language and dialogs |
| Prefix quick search | Partial | Up/Down/Home/End, extend/shorten, Escape; wildcard aliases, Shift/Alt search combinations and IME pending |
| Sort by name / extension / date / size | Partial | Directories first both ways; Ctrl+F3/F4/F5/F6; locale/natural sorting, options and persistence pending |
| Sort by attributes | Platform decision | Map Windows attributes and POSIX metadata |
| Navigation focus/selection memory | Partial | Session-only directory snapshots; large/slow/removable filesystem testing pending |
| Hidden files | Partial | Native Qt semantics; Linux dot files and Windows hidden attribute fixtures |
| View modes / zoom / thumbnails | Pending | Detailed, brief, icons, thumbnails, tiles and associated settings |
| Copy/move/rename/mkdir/delete/trash | Pending | Background service with overwrite/cancel/error/symlink policies |
| Filesystem watching and refresh | Partial | QFileSystemModel provides watching; selection survival and reconnect tests pending |
| Search/find dialog | Pending | Content/metadata searches, results actions, cancel and progress |
| Internal viewer / external editor | Pending | Text/binary modes, encodings and associations |
| Archives / pack/unpack | Pending | Inventory built-ins and plugins; provider abstraction |
| Clipboard / drag-and-drop | Pending | Paths, files, URI lists, inter-panel/desktop operations |
| Compare/synchronize/checksums/rename tools | Pending | Inventory exact core/plugin boundaries |
| Settings / themes / shortcuts / translations | Pending | Defaults, persistence, migration and customization |
| Plugin system and supplied plugins | Pending | Inventory every plugin, dependencies and available source; no Windows DLL ABI promise |
| Registry / COM / Windows Shell integrations | Platform decision | Official upstream retains Windows integrations; decide Linux equivalents per command |
| Network paths / FTP / remote providers | Pending | Native paths plus plugin functionality and missing dependencies |
| Help / About / licenses | Pending | User-facing documentation and notices; existing LICENSE retained |
| Distribution | Pending | Versioning, runtime deployment, installers/packages, clean-machine tests, release notes |

## Evidence for this milestone

- `help/src/hh/salamand/basicwork_select.htm`: marking and range selection.
- `help/src/hh/salamand/basicwork_qsearch.htm`: prefix search, matching navigation and advanced wildcard behavior.
- `help/src/hh/salamand/basicwork_sort.htm`: directory-first default and Ctrl+F3/F4/F5/F6.
- `help/src/hh/salamand/environment_focus.htm`: focus distinct from selection, Backspace/navigation context.
- `src/fileswn0.cpp`: Insert/Space advance, range-selection rules and quick-search state.
- `port/tests/panel_tests.cpp`: automated temporary-directory keyboard/navigation/sort fixtures.

Automated tests are necessary but do not establish visual parity. Linux desktop acceptance against the Windows reference remains a separate check for each milestone. Windows builds of this fork are out of scope.
