# Encoding-preserving Session foundation

The GUI currently owns the older Document model, which supports UTF-8, UTF-8
with BOM, and BOM-marked UTF-16 LE/BE. The byte-faithful Session used by terminal
and CLI previously had only a UTF-8 publication path. Moving the GUI to Session
without an explicit output codec would lose existing encoding behavior.

`Session::open_decoded(path)` now provides a separate, explicit open path for
future mapped GUI integration. It uses the existing BOM detection and UTF
validation, preserves the detected output encoding, and stores logical text as
UTF-8. It does not guess a legacy code page. Source ranges, clipboard bytes,
revisioned edits, projections, undo/save baselines and as-opened restoration all
refer to that logical UTF-8 text. They are not offsets into a UTF-16 file.

The original encoded file snapshot remains separate and supplies save conflict
checks. Save and Save As encode the current logical text using the retained
codec before invoking the existing safe publisher. Failed conversion or
publication does not change the path, codec, saved baseline, revision, or undo
history. Successful publication retains the existing save-B/restore-A policy.

The default `Session::open` is unchanged: it retains every input byte, including
BOMs and malformed sequences, and uses UTF-8 publication after illegal bytes
are removed. Reset or a later raw open clears the decoded output codec. Existing
terminal and CLI callers still use that path. No transport command or product
menu has been added for the new internal API.

The shared decoder/encoder now accepts an explicit `TextControls` policy.
Existing GUI callers default to their legacy control rejection. The new Session
path requests preservation of every valid scalar, including NUL and C0 controls,
so it can support the required visible, inert mapped display. No control is
replaced, hidden in the source model, or converted to a display label by the
codec itself. Do not connect this content directly to the old GUI text widget;
the mapped display must be integrated at the same time.

Save Text Copy remains an explicit UTF-8 copy, preserving valid source scalars
and replacing illegal bytes as before. It does not change the source document's
codec, path or saved baseline.

This API is synchronous and currently accepts only encoded files and decoded
text below the 16 MiB editable threshold. A smaller UTF-16 file can expand past
that threshold in UTF-8; that open is refused without replacing the current
document. Paged UTF-16 decoding, cancellable GUI opening, native mapped editing,
encoding selection in the reviewed-save flow, and actual GUI Session adoption
remain unfinished. The foundation does not establish those features.

Regression coverage in `tests/session_tests.cpp` includes all four encodings,
exact BOM/byte-order output, mixed endings, non-BMP Unicode, controls/NUL,
clipboard and explicit UTF-8 copy behavior, edit undo/redo, save boundaries,
restore-opened, Save As collisions, encoded-snapshot conflicts, malformed UTF-16
open refusal, raw reopening, source/decoded-size boundaries and encoded-output
overflow. Existing Document tests retain the GUI's legacy codec policy, and
CLI/terminal suites exercise unchanged byte-faithful operation.

Local validation: all 29 suites passed in 22.96 seconds. After adding the
encoded-output overflow assertion, the Session suite passed again in 1.28
seconds. The C++ spelling audit passed all 148 files. Native cross-platform
validation of this foundation remains pending.
