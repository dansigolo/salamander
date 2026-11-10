# Local operations: first milestone

Implemented: F7 creates a single directory in the active panel; F2 renames marked entries, or the focused entry if there are no marks. Menus, toolbar and function-key controls share the same actions. Copy, move, trash and permanent delete remain disabled.

## Architecture

- `port/operations/fileoperations.h`: value requests/results and name validation, with no Widgets dependency.
- `port/operations/fileoperations_linux.cpp`: native Linux backend.
- `port/operations/operationcontroller.*`: one background operation at a time through QtConcurrent; emits typed results and busy state.
- `port/qt/namedialog.*`: names, validation, confirmation and cancellation.
- `MainWindow`: captures panel/directory/name snapshots and orchestrates single-item jobs and sequential rename prompts.
- `FilePanel`: eligible names from the current root and delayed focus/marking when filesystem updates arrive.

The operation job captures request values, not UI pointers. Closing the window is blocked while a job is running; it can be closed after completion. Navigating during a job does not redirect its target; a completion only adjusts panel focus if that panel still displays the original directory.

## Filesystem policy

- New names are single path components. Empty names, `.`, `..`, `/`, embedded NUL and invalid Unicode are rejected. Spaces are significant; Unicode names and Linux backslashes are allowed. Nested directory creation and moving to another directory through rename are not part of this milestone.
- For each submitted job, the parent directory is resolved once and held open with an O_PATH directory descriptor. Parent-directory symlinks are resolved normally. Later changes to an ancestor path do not redirect the already-open descriptor.
- Creation uses `mkdirat` with mode 0777 filtered by the process umask. Any existing entry, including a dangling symlink, is a conflict.
- Rename uses Linux `renameat2(..., RENAME_NOREPLACE)`. Checking existence and then calling an overwriting rename would be unsafe; the kernel enforces the non-replacement policy atomically. There is no copy/delete or overwriting fallback. Unsupported filesystems/kernels return an error.
- A source symlink is renamed as an entry, including a dangling symlink; the link target is not renamed or modified. Existing destination symlinks are conflicts.
- Same-name rename is a successful no-op only if the source entry still exists.
- Permission errors, missing sources/parents, existing destinations, unsupported operations and other native failures are returned explicitly. No automatic retry, overwrite, privilege escalation or rollback is attempted.
- Requests use QString filenames as exposed by Qt. Arbitrary filenames that cannot be represented through Qt's filename encoding need separate future work.

## Selection and cancellation

`..` and entries outside the visible root are never operation targets. F7 uses the current directory and ignores marks. F2 captures marked names in display order; without marks it captures the focused name. Each marked item has its own rename prompt; batch patterns and mass rename rules remain future work.

Canceling a prompt starts no operation for that item and skips all remaining prompts. Successful earlier renames remain applied. On a failure, remaining entries are not processed and the error reports any already-completed renames. These jobs are atomic filesystem calls; once submitted they cannot be interrupted. A rename result restores focus and, for originally marked entries, its mark when the result becomes visible. Navigation preferences still govern marks when changing directories.

Hidden results remain subject to the hidden-file filter; the success message confirms completion and Ctrl+H reveals them. Selection/focus updates never navigate the user back to an old directory.

## Tests and remaining milestones

Temporary fixtures cover Unicode/spaces, reserved/path-traversal names, files/folders, permission failures, live/dangling symlinks, same-name and missing-source cases, concurrent destination races and asynchronous job serialization. UI tests cover F7/F2, validation, cancellation, collisions, partial rename sessions, panel isolation, focus and function-key labels.

Future copy/move/trash jobs need their own policies before implementation: overwrite decisions, progress/cancellation, partial output cleanup, metadata, symlink/hard-link handling, cross-filesystem behavior and failures. Do not infer those semantics from these atomic single-directory operations.
