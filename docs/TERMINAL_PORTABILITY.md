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
