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

Still required: connect this work to the terminal event loop, render bounded
horizontal windows with exact grapheme/control mapping, preserve caret/selection
and backward navigation across mode/width changes, implement space wrapping,
and exercise F2 and cancellation through the terminal host. This component does
not yet change the visible read-only terminal or close the wrap requirement.
