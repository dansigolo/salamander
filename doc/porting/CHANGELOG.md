# Qt port development log

## Unreleased — keyboard/navigation milestone

- Set Altap Salamander 4.0 x64 as the visual and functional reference.
- Target a common Windows/Linux Qt codebase and add a CI build/test job for each.
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
