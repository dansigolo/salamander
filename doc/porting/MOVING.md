# F6 move policy (experimental Linux backend)

F6 moves marked entries, or the focused item when nothing is marked. The destination defaults to the other panel and must be an existing absolute directory. F5 and F6 share the asynchronous transfer controller and conflict UI; jobs are serialized per window.

## Collisions and native moves

Existing files and symbolic links require Replace/Replace all, Skip/Skip all or Cancel. Directories are never merged or replaced in this milestone. Self and descendant destinations are rejected. Links are moved themselves, including dangling links; their targets are never traversed. Special files are refused.

Within a filesystem, renameat2 with NOREPLACE publishes each root without copying data. Explicit file/link replacement uses renameat. Inodes, hardlinks, ownership, permissions and extended metadata are preserved by the filesystem. Reading directory contents is unnecessary, but normal kernel rename permission checks still apply. Only EXDEV selects the cross-filesystem path; other errors are reported.

## Cross-filesystem moves

1. Scan without following links and validate metadata requirements.
2. Use the F5 private staging/publication engine for a complete destination root.
3. Move the source root into a private `.salamander-move-UUID/entry` sibling. Check its identity and version.
4. Compare file bytes, link targets and exact directory names against the manifest and published destination. Preserve and verify basic ownership, permissions and modification timestamps; synchronize copied files and directories.
5. Check versions again before unlinking each known source entry. Never recursively delete unexpected names and never chmod the source to bypass permissions.

This milestone refuses cross-filesystem hardlinked entries, ACLs/xattrs, special permission bits, unsupported entry types and ownership/metadata that cannot be preserved. These restrictions do not apply to native renames. Different timestamp precision or platform support for link timestamps may cause a safe failure with the source retained. Linux `/proc/self/fd` is used for link metadata checks.

Cancellation during copy removes unfinished staging. After publication, cancellation before removal retains both source and complete destination. Once removal of a verified root begins, cancellation waits for that root and stops before the next one. Completed moves remain at the destination; there is no whole-job rollback.

## Failures and recovery

An error before source removal retains the original. A quarantined source is restored to its original name with NOREPLACE. If another entry occupies that name, it is never overwritten: the warning gives the private recovery path. Do not remove that path until inspecting its contents.

A removal failure can leave a **partial source tree**, alongside the complete copied destination. The warning identifies this state and the original or recovery path. Empty-wrapper cleanup and directory-sync failures are also reported, even after a completed move. Cancellation/failure counts distinguish completed, skipped and copied-but-retained roots.

This is a best-effort checked operation, not a filesystem snapshot or crash-recovery journal. Concurrent writers holding open descriptors can still change data after a check; avoid editing source/destination during a move. Cross-filesystem moves are not atomic. A process/power failure may leave private source or destination staging for manual recovery. Replacing a file explicitly can discard its previous destination contents even if a later source-removal step fails.

## Regression tests

`file_move` tests native inode preservation, unreadable directory contents, conflicts, cancellation, actual EXDEV copies, dangling links, source mutation, rollback name collisions, unsupported metadata and partial cleanup. `qt_window` covers F6 keyboard invocation, marks, destination defaults and explicit conflicts, alongside existing F5 regressions.

Cross-device tests require a writable filesystem different from the temporary source directory. By default they use `/dev/shm/salamander-move-tests` (including CI). Override with `SALAMANDER_MOVE_TEST_DESTINATION=/path/on/another/filesystem`. Tests create and remove their own unique temporary directories; they fail if the paths are on the same device.
