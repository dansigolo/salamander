# Qt port development log

## F5 copy milestone

- Copy marked entries or focus to the other panel, with an editable destination and asynchronous progress/cancel dialogs.
- Scan source trees without following links, stage each selected entry privately and publish completed entries atomically.
- Add Replace/Replace all files, Skip/Skip all and Cancel decisions; keep existing directories intact without merging.
- Detect self/descendant destinations, ordinary source changes, special files and destination races.
- Preserve basic file/directory permissions and timestamps; recreate symlinks verbatim.
- Retain completed entries on cancellation/failure, remove unfinished staging and report cleanup failures explicitly.
- Add backend and UI regressions, copy policy documentation and optional large sparse manual fixtures.


## Local operations

- Enable F7 single-directory creation and F2 rename through shared menu/toolbar/keyboard actions.
- Add a Widgets-independent Linux backend and asynchronous controller; serialize jobs per window.
- Refuse existing destinations atomically, including dangling symlinks, and rename source symlinks without touching targets.
- Rename marked entries through sequential prompts; cancellation/errors leave completed entries applied and skip the remainder.
- Validate names, report native failures and follow results with panel focus without redirecting navigation.
- Add backend collision/race/permission/symlink tests, full-window operation tests and manual test instructions.


## Selection policy (2026-11-09)

- Match the user-observed Altap 4.0 behavior: clear marks when changing directories while restoring navigation focus.
- Add a persisted Options preference to retain marks during the current session; default off.
- Keep current marks on failed/same-directory navigation and filesystem refreshes.
- Keep the filesystem model tree alive across navigation to avoid losing file monitoring during rapid directory revisits on Qt 6.4.
- Cover both policies, preference toggling, both panels and application-restart persistence in tests.

## Interface milestone (2026-11-08)

- Add original menu order, shared active-panel actions and top/places/function-key bars.
- Present Name/Ext/Size/Date/Time/Attr, basic Linux permission strings and a parent row excluded from marking.
- Add navigation history, editable path history, Go to directory, session favorites and mounted-volume navigation.
- Query free space and volumes in background jobs; show focused metadata and selected file-byte totals.
- Preserve persistent column indexes and selections while filesystem entries are updated.
- Keep unavailable file/network/user-command actions disabled.
- Add complete-window tests, a disposable manual-test fixture launcher and TESTING.md.

## Keyboard/navigation milestone

- Set Altap Salamander 4.0 x64 as the visual and functional reference.
- Focus this fork on Linux; keep the official Windows upstream as a tracked source of improvements.
- Add upstream checking and isolated source-integration tooling, with regression tests and a read-only monitoring workflow.
- Add Linux build/test CI. A Windows job was briefly introduced, then removed after the scope decision.
- Separate focus from marked items; Insert/Space mark and advance; Shift navigation selects or unselects ranges; Ctrl+A/Ctrl+D select all/none.
- Add prefix quick search with matching-name navigation, prefix editing and Escape.
- Keep directories first in ascending and descending sorts; provide extension sorting and Ctrl+F3/F4/F5/F6.
- Restore directory focus/marks within a session; focus the child directory when returning to its parent.
- Correct the bootstrap shortcut: Backspace opens the parent; Alt+Up/Down navigates marked items.
- Add a parity inventory and contribution workflow. Full toolbars, controls and operations remain future milestones.

## Initial bootstrap

- Native Qt two-panel application, CMake build and independent core/UI tests.
- Extract original mask-normalization algorithm without Windows dependencies.
- Initial prototype manually validated by the user on Linux Mint.
