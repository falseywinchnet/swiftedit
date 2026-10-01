# SwiftEdit command session, protocol 1

Run `swiftedit-cli.exe` without arguments. It reads stdin and writes stdout;
the process owns one in-memory document. Startup: `ready<TAB>SwiftEdit<TAB>1`.
There is no shell, network service, remote control, auto-commit, or background
save. Closing stdin or `quit` drops memory, including unsaved edits.

Requests are one line, fields separated by literal tabs. Escape field content
using `\\`, `\n`, `\r`, `\t`, or `\xHH`. Responses encode control and non-ASCII
bytes as `\xHH`; decode to recover the exact UTF-8/file bytes. This prevents
file content from injecting terminal control sequences or extra protocol rows.

Each request finishes with `ok<TAB>command<TAB>revision` or
`error<TAB>escaped explanation`. A command may first emit data rows. `quit`
returns `ok<TAB>quit`. Requests exceeding 32 MiB of transport text are rejected.

| Request fields | Behavior |
|---|---|
| `info` | Emits revision, byte size, dirty 0/1, read-only 0/1, path |
| `word-count` | Emits `word-count, count` for valid UTF-8 editable text; observes working text without changing revision/history; use the incremental commands for paged files |
| `word-count-start` | Replaces any previous count task for the current document. Emits `word-count-progress, 0, size` without reading source bytes, or `word-count, 0` for an empty document |
| `word-count-next, budget` | Reads at most 1..65536 bytes. Emits `word-count-progress, offset, size`, or final `word-count, count`. Rejects a changed document identity/revision, file read failure or malformed UTF-8; no partial count is published. A failed task must be restarted |
| `word-count-cancel` | Discards the count task immediately between steps. Does not change document content, selection, revision or undo history; harmless without an active task |
| `open`, path | Opens an existing file and emits stamped context for first 4096 bytes; dirty session requires save or explicit discard first |
| `discard` | Explicitly drops the current document and starts empty |
| `page`, offset, byte-budget | 1–65536 bytes; emits `page, offset, next, total, bytes` |
| `context`, byte-budget | Restarts sequential stamped context at byte zero; 1–65536 source bytes |
| `context-next`, byte-budget | Continues that bounded read; edits, save, open or discard require a fresh context read |
| `find`, exact-text | Up to 100 matches in editable document, each with bounded context; no automatic wrap or mutation |
| `preview`, before, old, after, replacement | Matches exact concatenated context; emits all bounded candidate previews |
| `commit`, token, revision | Applies one explicitly chosen current preview; subsequent attempts with stale/used tokens fail |
| `undo` / `redo` | Session history, only back to last successful save |
| `restore-opened` | Replaces working bytes with original open snapshot; doesn't write disk |
| `save` | Snapshot-checked save to current path; illegal UTF-8 bytes block it |
| `save-as`, new-path | Creates a new file, never replaces an existing target |
| `save-text-copy`, new-path | Creates sanitized copy, one space per illegal byte; leaves current document and original untouched |
| `csv-get`, A1 | Decoded CSV field from an editable `.csv` file |
| `csv-set`, A1, text | Previews a correctly quoted field update; requires commit |
| `csv-clear`, A1, B3 | Previews contents-only rectangular clear; requires commit |
| `csv-calculate`, expression | Returns exact result and referenced cells, no document mutation |
| `csv-value`, A1 | Returns the cell's displayed value and direct formula references; stored formula source is unchanged |
| `csv-convert-to-value`, A1 | Previews replacement of a formula by its exact value; explicit commit required; errors preserve source |
| `quit` | Ends session; unsaved state is discarded |

Preview rows contain `preview, token, revision, byte-offset, removed-byte-count,
inserted-byte-count, before-context, removed-prefix, inserted-prefix,
after-context`. Prefixes are capped at 240 bytes, surrounding context at 80.
Use pages to inspect longer source regions. Candidate count <=100 and retained
preview payload <=32 MiB. A newer preview request replaces earlier candidates
on success. A mutation, open, discard or successful save expires all candidates.
CRCR is reserved for line-marker metadata and prohibited in replacement payloads;
existing source CRCR can still be read and saved unchanged.

Files >=16 MiB are read-only via a retained file handle and bounded reads.
Search/edit/sanitize are currently restricted to smaller files. There is no
asynchronous whole-file count yet. Read-only paging follows the opened file
handle if its directory entry is replaced. It does not silently follow a new file.

Word count uses nonempty runs separated by Unicode whitespace, including NBSP
and ideographic space. Apostrophes, hyphens and punctuation do not split a run.
Zero-width space is not a separator. This deliberately documented first policy
does not provide language-specific segmentation for scripts without spaces.
Malformed UTF-8 is refused, without sanitizing or changing the document.

Example (the separators below are actual tabs):

```text
preview				Hello\nworld
commit	1	1
info
page	0	4096
```

For a newly started process this inserts `Hello` + LF + `world`. Always take
token/revision from the actual preview response in real clients. A command
session isn't the promised full-screen nano-like terminal interface.

CSV uses comma delimiters, doubled quotes and CRLF/LF/CR row endings. Local
edits retain all unrelated source spelling, quotes and endings. Missing cells
in ragged rows are errors; clearing never manufactures or shifts cells. Math
supports strict decimal literals, parentheses, arithmetic, rectangular ranges,
comma lists and the function subset in SWIFTEDIT_OBJECTIVES.md. Stored formula cells evaluate recursively with cycle, depth and work limits;
references observe current source values.
Example `ROUND(SUM(A1:B2)/3,2)` explicitly authorizes rounding. `10/3` alone
fails. Error messages name invalid referenced cells. The reference list can
later drive native hover highlighting.


## Blank-line context metadata

`open`, `context` and `context-next` emit a `context, document-identity, revision`
row, one ordinary `page, offset, next, total, bytes` row, then zero or more
`blank, source-byte-offset, logical-line-number, escaped-marker` rows. The marker
is exactly CR CR L<number> CR CR before transport escaping. The source field is
always separate and escaped, including literal CRCR already in an opened file.
Never concatenate marker fields into a document edit. Mutation payloads containing
the reserved marker delimiter are refused intact.

Only terminated, truly empty logical lines receive markers. Spaces or tabs make
a line nonempty. CRLF counts as one ending even when split between pages; bare
CR and LF each end a line. No synthetic trailing line is added at EOF. Line
numbers are one-based observations of that exact document identity/revision,
not persistent edit addresses. Changing the source invalidates the traversal;
use `context` to restart. Exact context plus preview/commit remains the edit
addressing mechanism. Large read-only files use the same bounded reader without
loading or pre-indexing the whole file. `page` and `find` retain their original
raw-byte context response; they do not guess line numbers for arbitrary offsets.
