# Qt port development log

## Selection policy (2026-11-09)

- Match the user-observed Altap 4.0 behavior: clear marks when changing directories while restoring navigation focus.
- Add a persisted Options preference to retain marks during the current session; default off.
- Keep current marks on failed/same-directory navigation and filesystem refreshes.
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
