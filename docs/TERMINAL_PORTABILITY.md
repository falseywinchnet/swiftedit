# Shared interactive terminal implementation

The interactive editor loop now lives in `src/terminal_app.hpp` and depends on
`TerminalConsole`, not Windows console records. Editing, prompts, search/replace,
wrap, paging, copying, word count and cancellation still use the same Session
operations as before. This is the integration boundary for the pending POSIX
terminal host; it does not by itself make an interactive Mac/Linux terminal
available.

The caller owns the console for the whole Terminal lifetime. The editor borrows
it synchronously; the existing wrap controller borrows that same console.
The Windows implementation owns and restores its screen/input handles as before.
The adapter translates native events into explicit key identities, modifiers,
repeat count, resize notifications and a lossless UTF-16 input stream. The shared
loop validates surrogate pairs and encodes complete scalars as UTF-8. Filename
construction declares UTF-8 explicitly on every platform. Neither source bytes
nor clipboard content are passed through the input-event encoding.

Special key identities are independent of Windows virtual-key numbers. Letter
commands use uppercase ASCII identities while typed text preserves its original
case. Other native keys remain nonzero identities so command/input state resets
retain their previous behavior. Key releases are ignored; input order and repeat
limits are unchanged.

The new headless `terminal-app` test uses an owned scripted console with the
actual editor loop. It covers non-key events, resize, supplementary Unicode,
undo/redo, grapheme navigation/delete, a non-ASCII filename, save/exit and recovery
after invalid surrogate input. It writes only its unique disposable fixture.
Local Windows Release builds and all 21 tests pass in 4.59 seconds; the spelling
audit passes for 108 source/header files. The existing native Windows console
smoke remains the OS boundary regression and requires native CI validation.

Remaining POSIX work includes raw-mode lifetime/restoration, bounded escape and
UTF-8 decoding, terminal resize and input readiness, conventional key sequences,
safe exit behavior, and owned pseudo-terminal smoke tests. It must run the shared
loop rather than a separate editor implementation. Read-only horizontal
selection/search and the other previously recorded terminal gaps also remain.

## POSIX host implementation checkpoint

The macOS/Linux executable now uses the same Terminal loop. Its host owns raw
mode, alternate-screen and bracketed-paste enablement, restores the prior modes
on normal exit and exceptions, and handles resize and termination signals with
signal-safe flags. It blocks in poll while idle. A 4 KiB input buffer amortizes
reads; no idle polling timer is installed. Escape sequences have a 50 ms
continuation wait, while split UTF-8 and bracketed paste wait for actual input.
External SIGTSTP currently exits with restoration rather than implementing
suspend/resume; keyboard Ctrl+Z remains Undo. This limitation is explicit.

The incremental decoder recognizes conventional xterm navigation, modifier and
function sequences, UTF-8 and Ctrl-letter commands. Backspace uses DEL (0x7f);
Ctrl+H invokes Replace. Ctrl+slash/underscore (0x1f) toggles the wildcard slot.
Shift+selection requires the terminal emulator to send modified key sequences.
Unsupported or oversized sequences produce a bounded error. Bracketed paste
is one exact insertion, including control bytes, never an interpreted command
stream. Its 16 MiB input bound is drained before refusal, preserving source and
preventing the remaining bytes from becoming commands. Existing editable-file
limits still apply. Pasting into an exit choice cannot accept that choice.

New decoder and shared-loop tests cover split supplementary Unicode, malformed
input, modifiers, sequence bounds, embedded command bytes, atomic paste undo/redo
and recovery after oversized paste. The owned Python pseudo-terminal smoke
checks real raw mode, Unicode/control paste, save/exit, SIGTERM cleanup and exact
terminal-mode restoration on Mac/Linux. It has bounded waits and kills/reaps only
its own child on failure. Native compilation and this PTY smoke remain pending
CI; no POSIX interactive availability or packaging is yet claimed from the
Windows headless results. The prior extraction passed native/core tests on all
three platforms (runs36951475029 and36951475071).

## Native validation and Mac package checkpoint

Native run 36952915521 compiled the POSIX host on macOS and Linux. Windows and
Linux passed; macOS failed the first PTY mode comparison. Its diagnostic showed
that only local flag 0x20000000 differed: Darwin's transient PENDIN state, set by
the kernel when restoring ICANON. All other flags, speeds and control characters
matched. Apple's `bsd/kern/tty.c` sets this state in `ttioctl` and processes it
in `ttnread`, the FIONREAD path. The test now queries the input queue before both
mode snapshots. This neither consumes nor flushes input, and the comparison
still checks every field without masking any flags. Both normal and SIGTERM
cases must pass native CI before packaging is accepted.

The Mac packager now includes `Contents/MacOS/SwiftEdit-terminal`, resolves its
dependencies, signs it, records its hash and runs the owned PTY smoke against the
packaged executable with library-path overrides removed. This addition remains
unvalidated until the new native run passes. It is not included in the existing
v0.2.6 dogfood download. Portable-core CI only covers newline, CSV and file-reader
tests; its success does not establish terminal or GUI availability.

Validation completed at `6fbdecc3f78c783ac93c695a7a80dc47d6e89f2e`: native run
36953195677 passed all three platforms, including all 30 Mac tests and the
packaged terminal PTY smoke. Portable run 36953195651 also passed all three.
The resulting verified archive is published as v0.2.7-dogfood.20261001. Normal
and interrupted exits passed the exact mode comparison after the queue query;
no production restoration change or ignored mode bits were required.

## Signal wait race correction

Source `927ffc57f053ad10ca003be2dd1b30e1921d2a57` closes the gap between checking
termination/resize flags and entering an indefinite input wait. Handled signals
are blocked during that check; pselect restores the previous mask atomically
while waiting. A scoped owner restores the caller's mask on every exit. No
periodic idle wakeup is introduced; Escape continuation retains its 50 ms bound.

The owned PTY regression now covers SIGINT, SIGTERM, SIGHUP and SIGTSTP during
idle input, an incomplete Escape sequence, split UTF-8 and incomplete bracketed
paste. Every case also sends SIGWINCH, requires bounded exit, verifies the source
is unchanged, and compares all restored terminal mode fields after the Darwin
queue-state transition described above. These 16 cases exercise cleanup states;
they do not claim exhaustive timing interleavings or suspend/resume support.

Native run 36953856671 passed Windows, macOS and Linux; portable-core run
36953856558 passed all three. Mac passed all 30 native tests and repeated the
expanded PTY smoke against the packaged terminal. The change is on master and
in that run's Mac artifact; the earlier v0.2.7 release remains unchanged.

## Large paste confirmation

Terminal-emulator paste and Ctrl+U now use the same threshold as the GUI:
more than 500,000 bytes requires an explicit typed Y before insertion. Exactly
500,000 bytes does not prompt. Read-only and oversized resulting documents are
rejected before staging. The pending choice owns the original pasted bytes and
records the document identity, revision and selection. Confirmation rechecks
those values before one insertion. N/Escape discard the pending bytes, and
pasted Y, Ctrl+Y or other commands cannot approve or bypass the choice.

The editable terminal now binds Ctrl+A to its existing Select All operation;
the read-only path already had that binding. Shared-loop regression scenarios
exercise both clipboard paths, exact threshold, cancellation, one undo returning
to a clean document, redo and exact saved bytes. Source
`7c4fedafe9f5b8dd3ede41a1d7e16ad529c415a3` passed all 22 local tests (6.11 s),
native run 36954678518 on Windows/macOS/Linux, and portable-core run 36954678433
on all three platforms. This verifies the shared-loop behavior on each platform;
physical terminal-emulator clipboard interaction still needs broader dogfooding.

## Cancellable read-only end navigation

Ctrl+End now scans to the final viewport of a read-only file. Shift+Ctrl+End
extends the source selection to EOF; Ctrl+C uses the existing cooperative copy
task. The scan reads at most 64 KiB per step, checks document identity/revision
and size, and retains at most 32768 preceding row cursors plus the final viewport.
Any key cancels between steps, and resize cancels the old-width scan. Incomplete
or stale work cannot publish a destination. Source bytes are never modified.
Up/Page Up work after completion within the retained history; unbounded backward
navigation, horizontal read-only caret motion and large-file search remain work.

The scan shares page geometry but skips off-screen paint strings and processes
more rows per source chunk than the visible viewport. The initial control-heavy
16 MiB shared-loop regression took 12.82 s locally; eliminating repeated decoding
reduced the same test to 1.44 s. These are single local test durations, not a
native latency distribution or a general performance guarantee. The final local
suite passed all 22 tests in 6.92 s, including Shift+Ctrl+End/full-file copy,
Escape cancellation, stale/incomplete refusal, label continuation and Page Up.
Cross-platform native validation remains pending for this change.
