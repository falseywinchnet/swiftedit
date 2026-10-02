# Logical paging for read-only terminal views

The read-only terminal currently hard-wraps pages and ignores F2. The owner
requires no-wrap and intelligent space wrapping without source mutation. The
existing visual-row pager cannot implement that simply by toggling a flag:
its row cursors and backward navigation are tied to hard-wrap width.

`TerminalLogicalPage` is the first implementation step toward that change. It
discovers up to 300 logical rows starting at a verified line boundary, retaining
only byte offsets, body lengths and separator lengths. It does not retain line
text. CR, LF and CRLF remain distinct, including CRLF across read boundaries.
A final separator leaves an empty EOF line; pagination preserves that line
without adding an empty line to an unterminated document.

Construction reads at most two bytes to validate a nonzero start. Each explicit
step reads at most 8 KiB (the caller may request less). A very long line yields
between steps rather than growing a source buffer. Row storage is reserved once
for the requested height. Dropping the task cancels it without changing Session
or a visible viewport. Results are unavailable until the requested viewport or
EOF is complete; identity/revision/size are checked before advancing or exposing
a result. Returned row metadata borrows the task and must be consumed within
that lifetime, with source validity checked again at later publication.

Tests cover step budgets 1 through 9 and viewport heights 1, 2, 3 and 300 across
empty, unterminated, mixed-ending, malformed-byte and Unicode fixtures. They
exercise invalid dimensions/budgets/starts, split CRLF refusal, foreign and
mutated sessions, incomplete/stale publication, cancellation by destruction,
and a real read-only file with a 16 MiB line. That line is discovered in 2,049
steps while retaining two row records. The source bytes remain unchanged.

Local Windows Release build and all 25 headless tests passed in 21.89 seconds;
the 132-file spelling audit passed. Source review checked byte-unit arithmetic,
bounded allocation/read work, CR lookahead, immutable result borrows and stale
source refusal. The initial test fixture used the mutation API for raw CRCR
input and hit its intentional metadata-marker refusal; the fixture now opens
exact bytes from a unique disposable file, matching the requested open behavior.

`TerminalHorizontalLine` now prepares a bounded horizontal window from a
completed logical page. Each step reads at most 8 KiB; retained source context
is capped at 64 KiB. It stops after reaching the visible right edge, retains
only intersecting runs and legal caret boundaries, and preserves absolute
source offsets. Partially visible wide glyphs become spaces; tabs and inert
control labels may be clipped. Oversized graphemes refuse intact. Failed tasks
cannot publish or resume, and stale identity/revision/size refuses publication.

Tests compare the editable row renderer across small read budgets, widths and
horizontal positions, including malformed bytes, tabs, combining marks, ZWJ
emoji and regional indicators. A real 16 MiB read-only line reaches columns
8190..8201 in two 8 KiB reads without retaining that entire line. Cancellation,
oversized context and stale results are covered. The complete Windows Release
build and all 26 headless tests passed in 23.07 seconds; the 135-file spelling
audit passed. These are bounded-work checks, not physical latency measurements.

`TerminalHorizontalPage` composes the scanner and row renderers into a complete
viewport task. One step advances only the scan or one renderer, preserving the
8 KiB read budget. Completed row output is copied into reserved viewport storage
and row scratch is released. Partial or failed viewports remain unavailable;
the caller can retain its previous published owner while replacement work runs.
Tests cover exact horizontal slices, clipped tabs, CRLF pagination, empty EOF
rows, cancellation without changing an existing completed viewport, and stale
identity refusal. The focused test passed in 0.14 seconds and the spelling
audit remained clean. This task is not yet wired to the host input loop.

`TerminalLogicalPrevious` supplies width-independent backward navigation. It
scans from a verified logical boundary, counts complete CR/LF/CRLF separators,
and clamps at document start. One step reads at most 8 KiB; only scalar scan
state is retained. CRLF pairing survives chunk boundaries. No byte-to-character
conversion or wrapped-screen history is needed to find an earlier logical row.
Tests compare backward results from every row of the mixed-ending/Unicode/raw
byte fixtures at budgets 1..9 and heights 1, 2, 3 and 300. A real 16 MiB line
rewinds in 2049 bounded steps. Unfinished/stale/foreign results and invalid
starts/budgets are refused; dropping work cancels without changing source.

Still required: connect this work to the terminal event loop, preserve caret/selection
and backward navigation across mode/width changes, implement space wrapping,
and exercise F2 and cancellation through the terminal host. This component does
not yet change the visible read-only terminal or close the wrap requirement.
