# F5 copy policy

F5 captures marked names from the active directory, falling back to the focused entry. The destination dialog defaults to the other panel's current directory and allows another existing absolute directory. Copy never deletes source entries. `..` is not eligible. Requests and source names are captured before the job starts; the job does not read mutable UI state.

## Worker and publication

`copyoperations_linux.cpp` performs all scanning, reading, writing and metadata operations on a worker. `CopyController` sends throttled progress and conflict signals to the GUI. While a conflict awaits input, the worker waits on a condition variable; the GUI stays responsive and cancellation wakes that wait. Dialogs are asynchronous and use plain text for filenames.

The job first scans the selected trees without following symlinks. It rejects special files (FIFOs, sockets and devices), directory cycles and nesting beyond 256 levels. The manifest stores native child names and source identities/metadata. Total bytes cover regular-file content, including hidden files inside selected directories regardless of the panel's hidden-file filter.

Each selected entry is copied into a private `.salamander-copy-*` directory on the destination filesystem. A staged file/tree is published under its final name only when complete. Publication uses `renameat2(RENAME_NOREPLACE)` by default, so a destination appearing during the copy cannot be overwritten silently. Copying onto the same inode or into a selected directory is rejected, including directory aliases.

Files are copied in bounded buffers. Basic rwx permissions and modification/access timestamps are applied to regular files/directories; special permission bits and ownership are not copied. File and directory sync errors are reported. Symlinks, including dangling links, are recreated with their stored target text and are never traversed. Relative links may resolve differently at their new location, as expected when copying the link itself.

## Conflicts

- **Skip / Skip all:** retain the destination and skip the selected entry.
- **Replace / Replace all files:** available for regular-file/symlink conflicts only. The old destination entry is replaced atomically after the new content is complete; a destination symlink is replaced as an entry, not written through. Sources remain intact.
- **Cancel copy / Escape:** stop the job cooperatively.
- Existing directories are not merged, deleted or replaced in this milestone. They can be skipped, or the job canceled and another destination chosen.

Replacement consent applies to the destination pathname; concurrent external edits are not protected by a filesystem transaction. Source identity, size and modification/change timestamps are checked during copying and ordinary source changes are reported. This is best-effort detection, not a consistent snapshot of a concurrently edited tree.

## Cancellation and failures

Cancellation is checked during scanning, between entries and between buffer writes. A kernel filesystem call already in progress may need to return before cancellation proceeds. The dialog remains visible while cleanup runs. Completed selected entries remain in the destination; only the unfinished private staging entry is removed. There is no whole-job rollback. If permissions, mount failure or another error prevents cleanup, the error explicitly names the remaining staging directory.

Failures stop the remaining entries. The result reports completed/skipped names, published bytes, cancellation and errors. The progress bar measures processed planned bytes (including skipped entries); it is not a count of bytes durably published. The final summary reports published bytes only.

Completed output names become visible in the destination panel without navigating either panel elsewhere. Source marks are retained. The application serializes mutation jobs per window and blocks new mutation actions while the copy dialog/job is active.

## Known limits and next work

Directory merging, resuming, copy-as/new destination names, automatic creation of destination directories, retry/skip-on-I/O-error, ACLs, xattrs, sparse extent preservation and preservation of hard-link relationships remain pending. Hard-linked regular files are copied as independent contents; sparse files may consume their full logical size. Top-level names are represented through Qt's filename encoding, while native byte names inside copied trees are preserved.

Do not implement F6 by deleting sources merely because a copy reported success. Move needs a separate identity/verification, cancellation, error and cross-filesystem policy.
