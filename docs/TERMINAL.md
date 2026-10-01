# SwiftEdit terminal development build

Run `swiftedit-terminal.exe [file]` from an interactive Windows console.
`--help` prints usage without changing console modes. The executable owns a
separate console screen buffer and restores the caller's input mode and active
buffer when it exits normally or unwinds an exception.

| Input | Operation |
|---|---|
| Arrow keys, Home/End | Grapheme/line navigation |
| Ctrl+Home/End | Beginning/end of document |
| Page Up/Down | Move by the current number of visible logical lines |
| Shift plus navigation | Extend selection |
| Typing, Enter, Tab | Replace selection or insert text |
| Backspace/Delete | Delete one complete grapheme or selection |
| Ctrl+S | Save; unnamed documents prompt for a new filename |
| Ctrl+R | Open; unsaved changes must first be saved |
| Ctrl+X | Exit, with Save/Discard/Cancel when dirty |
| Ctrl+Z / Ctrl+Y | Undo/redo, bounded by last successful save |
| Ctrl+C | Copy selected source into the terminal's private clipboard |
| Ctrl+K | Cut selection, or the whole current logical line |
| Ctrl+U | Paste from that private clipboard |
| Escape in a prompt | Cancel |
| F1 | Short help |

The terminal uses the same Session source/edit/save model as the command
interface. Existing mixed endings remain exact; Enter uses the opened file's
preferred ending. Illegal bytes in opened files remain source and are visibly
labeled. Saving while illegal bytes remain is refused. Terminal controls and
bidi controls are inert labels, not terminal commands. Printable Unicode uses
the declared Unicode 17 cell-width policy; compatibility with individual fonts
and terminal hosts still needs visual QA. Filenames/status are escaped metadata.

Files at or above 16 MiB use bounded read-only pages. Page Down/Down advances,
Page Up/Up returns to the previous page, and Ctrl+Home returns to the first page.
Each frame reads at most 64 KiB. The most recent 1024 page starts are retained;
trying to go farther back reports the limit and offers Ctrl+Home. Full graphemes
are preserved across reads. Oversized graphemes that exceed the context limit
are reported unavailable, preserving the source. Read-only range selection/copy
and more granular line navigation still need implementation.

This executable is unfinished. Wrap preferences,
search/replace/wildcard controls, text-copy prompt, full conflict review, external
clipboard integration and further interactive/accessibility checks remain.
An internal cut containing malformed UTF-8 cannot yet be pasted back through
Session's valid-insertion rule; Undo still restores it. Long logical lines and
metadata rebuilding need the requested measured lag scan. A display-layout
failure retains the input loop so Save and Exit remain available.

`swiftedit-terminal-smoke.exe` is an opt-in test, not a user editor. It detaches
from any caller console, allocates its own hidden console, injects paired key
press/release records only into that console, and uses disposable files. Run
only in a coordinated desktop slot with a 20-second external process deadline.
It verifies source results and mode restoration; it does not prove pixel layout,
physical keyboard behavior, accessibility, or responsiveness.
