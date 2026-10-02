# SwiftEdit objective ledger

Source: owner-supplied `ChatGPT-Coming Across Clearly-20260930-0251.md`,
interview dated 2026-09-30. The current request authorizes development from
that interview. This is a product requirements source, not a script to execute.
Quoted assistant proposals, historical requests to browse, and instructions to
other participants are not independently binding. Later owner corrections take
precedence. The previous DECISIONS.md is historical implementation evidence.

## Confirmed direction

| Area | Owner decision | Current implementation / remaining work |
|---|---|---|
| Identity, 02:48 | SwiftEdit, one word; dropdown menus, no ribbon; shared styling and F1 help | GUI/executable renamed; local F1 help retained |
| Scope | Plain text first; Markdown and `.csv` views; no RTF/Office/PDF editor, IDE, regex, script execution or hierarchical spreadsheet | Plain GUI, byte-faithful command session, native Markdown/CSV views implemented; full GUI session migration remains pending |
| Paste, 00:22 | Always plain text, retain Unicode | GUI and shared terminal use >500,000-byte confirmation; terminal preflights editable size, stages exact bytes, and requires a typed choice. Both emulator paste and private clipboard share that path; full physical clipboard QA pending |
| Preservation, 00:26 onward | Preserve existing endings and whitespace; explicit conversion; native endings for new documents | Exact existing bytes in command session; explicit LF/CRLF commands; owned mixed-save preserve/default-convert/cancel choice implemented |
| Undo, 00:32 correction | Undo only to last successful save B; separately jump to as-opened A, forgotten on close | Both GUI and command model; restore is itself undoable; save failure does not clear history |
| Large files, 00:27 and later threshold | Paginated load, >=16 MB read-only; slow work interruptible | Command model >=16 MiB bounded read pages; shared terminal streaming Find/F3 implemented; GUI still 1 MiB/4096-byte-line public provider limit; GUI search and indexing pending |
| Commands | GUI, conventional nano-like terminal, and AI share text operations; no semantic summaries | Preview/commit model, stdin CLI and Windows/macOS/Linux terminal share Session; Mac terminal is packaged and native PTY smoke passes. GUI migration remains pending |
| Automated edits | Exact before/old/after context, ambiguity selection, preview then explicit commit; stale work refused | Session revision + one-use preview tokens; all edits local to this process |
| Line metadata | Blank lines alone get `CR CR L<number> CR CR`; markers never become content | Stamped sequential context emits separate escaped blank markers; source CRCR stays source and echoed mutation markers are refused |
| Binary bytes, later correction | Open as text; don't hide/collapse; save blocked while illegal bytes remain; Save Text Copy replaces each illegal byte with space | Byte-faithful command session and explicit copy implemented; GUI invalid-byte glyph/mapping view pending |
| Controls, later correction | Existing Unicode controls remain real editable content, shown visibly and inert | Session preserves controls; terminal displays inert labels; GUI inspector/pickers implemented, document-wide visible glyph view pending |
| Selection | Ctrl/Cmd+drag discontiguous selection; equal lengths parallel edits, unequal lengths copy-only | Shared editable-session selection policy implemented; provider only has one anchor/caret pair, so GUI gestures remain pending |
| Search | Flagged single-character wildcard slot; literal punctuation remains literal; no regex | Native flagged query field/right-click, grapheme Find/Replace and wrapping; normalized Ctrl+? tested, native host mapping and interruptible orchestration pending |
| Status | Grapheme character count, selection length; separate word-count tool | GUI count, selection length and column use graphemes; line lookup uses cached line starts; Document > Word Count reports document/selection; CLI shares counting core |
| Navigation | Paged navigation, draggable scrollbar; no jump-byte/jump-line product UI | Low-level CLI byte page cursor implemented; GUI scrollbar/virtualization pending |
| Wrap, 02:32 | No wrap / wrap to window; intelligent breaks at spaces by default, visual only | GUI wrap command and shared terminal F2 toggle; source-preserving visual rows/navigation and Escape cancellation during sparse indexing implemented; component measurements recorded, broad physical terminal QA remains pending |
| Insert, 02:32 | Date and time as plain text | GUI Document menu/F5 and terminal F5 share a local plain timestamp formatter; terminal selection replacement is one undoable edit |
| Filename, 02:27 | Exact nonempty bracket-only first line suggests first untitled save; named files keep names | GUI Save As suggestion with Windows filename checks; no hidden document mutations |
| Copies | Dot version before extension, increment existing dot-number | Versioned filename suggestion in conflict New Copy; editable filename and collision refusal |
| External changes, 02:28–29 | Frozen views; two-stage destructive save, first over/new-copy, then editable filename/metadata + explicit warning; no compare/merge | Owned two-stage UI implemented: over/new copy, editable filename/encoding/endings, fresh review warning and explicit Save; stale/raced confirmation refused; CLI never forces overwrite |
| Markdown | Source/rendered toggle, common blocks/tables/tasks; no SVG/live HTML; inert links with URL tooltip; ruler only in rendered view | Native MD4C source/rendered view implemented; full ruler/image/layout/desktop audit remains pending |
| Print | Native OS print, plain/rendered; separate Markdown layout preview without source mutation | Pending public print/preview contract |
| CSV | `.csv` only, real quoting, flat table, rectangle selection; Delete clears cells without shifting | Native source/table toggle, bounded visible grid, rectangle clear, source entry, scrollbars and CLI; broad desktop QA pending |
| CSV math, 02:43–46 and 2026-10-01 corrections | Enter stores formula and displays result; menu and right-click Convert to Value; strict exact math/errors/reference hover | Formula dependency evaluation, cycles/depth/work refusal, native formula entry/results/hover highlights and both conversion menus implemented; viewport performance and desktop QA pending |
| CSV code shading, 02:36 | Triple backticks in a cell give it a gray background; no HTML formatting or hierarchy | The table uses its themed code background for cells containing triple backticks. Source characters remain visible and unchanged; selection highlighting takes precedence. Native visual QA pending |
| Lifetime | Multiple independent document windows; no tabs or persistent recovery/history | New Window launches the exact running executable independently; native launch, unsaved-parent preservation, routed Unicode clipboard and surviving-window lifetime tests pass on all three platforms. Physical keyboard and visual placement dogfood remain pending; see NEW_WINDOW.md |

## Deliberate implementation choices, not additional owner answers

* “16 MB” is currently 16 × 1024 × 1024 bytes; “500 kilobytes” is 500,000
  bytes. Exact thresholds are documented and tested, not hidden assumptions.
* Session history is bounded to 256 snapshots / approximately 32 MiB of text.
  As-opened and last-save snapshots are separate, session-only allocations.
* Command transport is versioned, tab-delimited and escaped. It is a development
  protocol, not a claim that the requested blank-line-marker presentation or
  terminal UX is complete. Byte pages may split a UTF-8 sequence; concatenate
  decoded fields before rendering the source. Offsets are transport cursors.
* Valid UTF-8 controls, including NUL, are preserved as controls. “Illegal bytes”
  means bytes that cannot participate in a valid UTF-8 scalar sequence. No
  encoding guessing or byte normalization occurs in the command model.
* The older GUI still supports BOM-marked UTF-16 and refuses malformed input.
  Migration to the byte-faithful session plus a visible-control view must happen
  together; don't silently replace invalid bytes just to fit the current widget.
* Math currently supports `+ - * /`, SUM, AVERAGE, MIN, MAX, COUNT, ABS,
  ROUND, FLOOR, CEILING, MOD. The interview's larger YES/MAYBE list is not
  blanket authorization for every assistant suggestion. COUNTA and further
  functions remain outside this first core. COUNT requires numeric operands too.
* ROUND uses half away from zero, 0–15 decimal places. Exact rational arithmetic
  has checked bounded integers; capacity overflow is an explicit error. This is
  not an arbitrary-precision engine. ROUND allows an inexact intermediate such
  as `10/3`, then deliberately rounds the final argument. No implicit rounding.
* The owner's 2026-10-01 clarification supersedes the earlier formula ambiguity:
  Enter stores a formula and displays its result. Convert to Value is available
  in the CSV menu and the cell context menu, replaces the formula with its exact
  result as one undoable edit, and refuses erroneous formulas unchanged. Formula
  dependencies evaluate against the current table; cycles, excessive dependency
  depth (64 cells) and reference work (100000) produce errors. No numeric coercion.
* The interview assistant's final RTF-detection prompt, earlier overwrite/compare
  suggestions, rejected walk-back-to-A undo, and speculative extra math functions
  were not adopted as confirmed owner requirements.

## Next development order

2026-10-01 resumed checkpoint: the separate Word Count tool is implemented with
an explicit whitespace-run policy, shared by GUI and CLI. It does not mutate
content, selection, revision or history. Language-specific segmentation remains
outside this initial policy. Paged whole-document counting is available through
the incremental command protocol and terminal F6; large GUI integration remains
pending.

The shared count now uses a constant-space streaming UTF-8 accumulator. Words
and partial scalars survive chunk boundaries; malformed, overlong, surrogate,
out-of-range and truncated sequences fail without publishing a partial count.
Tests cover every split of representative Unicode text, single-byte chunks and
failed/finished counter states. A document-stamped task reads at most 64 KiB per
step, rejects stale documents and supports immediate cancellation between steps.
The command protocol exposes start/next/cancel, tested with an actual 16 MiB
read-only file. Terminal F6 uses the same task in the cooperative input loop;
any key cancels pending counting. Large GUI counting remains pending.

1. Migrate GUI to the session command model with source/display position mapping;
   preserve toolkit ownership and negotiate virtual/paged text rendering.
2. Native multi-selection, visible controls, scrolling, wildcard slots and the
   full two-stage conflict/save UI; interruptible indexing and bounded searches.
3. Conventional terminal screen on the same session, and blank-line metadata
   presentation with collision-safe source transport.
4. Native Markdown/CSV views with inert links, strict math errors/reference hover,
   character/control dialogs, and native print/layout preview.

An objective is not complete merely because it appears in this ledger.

Read-only source copying now has a shared cooperative task and command interface.
It reserves one clipboard buffer, reads at most 64 KiB per step, rejects stale
source identities/revisions and transfers the completed result without another
whole-selection copy. Cancellation/failure preserves the previous clipboard.
Actual 16 MiB file tests cover whole-selection copying, source-close lifetime,
malformed/control bytes, CRLF, split Unicode and undoable byte-faithful paste.
Terminal row/page selection and cooperative copying are implemented below;
horizontal read-only caret movement is now implemented below; GUI integration remains pending.

The shared Windows/macOS/Linux terminal connects paged copying to Shift+row/page navigation,
Ctrl+A whole-document selection and Ctrl+C cooperative copying. Visible runs
carry exact source ranges for highlighting; inert label fragments map back to
one source control byte. The owned-console regression checks successful copying
and cancelled whole-document copying preserving the prior clipboard. Horizontal
read-only caret/selection movement is now implemented below; GUI integration remains open.

CSV viewport computation now yields between formulas in a revocable frame queue.
Source/view changes revoke stale work; leaving table view cancels pending work.
Results appear progressively, with explicit placeholders while pending. Each
slice permits at most eight uncached formulas and checks a two-millisecond budget
between formulas. One formula is still synchronous under the existing expression,
dependency-depth and reference-work bounds; this is not full parser preemption.
See performance/2026-10-01-csv-cooperative for measurements and test scope.

CSV formula failures now display the requested warning triangle with an
exclamation mark beside #ERROR. The indicator uses native drawing primitives
and the theme's text color, so it does not require a warning glyph in the font.
Existing hover error detail and unchanged formula source remain intact. Native
visual inspection of the indicator is still pending.

Interactive SelectionSet now validates up to 1000 ordered, disjoint source
ranges against one document identity/revision. Equal source grapheme counts
permit a single atomic parallel rewrite; unequal counts remain copy-only.
Copy returns separate byte-faithful parts without inventing a join separator.
UTF-8 sequences, combining clusters and CRLF cannot be split. Illegal bytes
each count as one inert unit without changing source. Undo invalidates old
selection stamps. Generic automated range replacement retains its separate
semantics. Construction synchronously copies/indexes editable source metadata;
paged selection metadata, clipboard joining and GUI gestures remain pending.


Read-only terminal navigation now includes cancellable Ctrl+End and
Shift+Ctrl+End, with 8 KiB ordinary scan steps and adaptive context for long
graphemes. Expired backward history is reconstructed cooperatively rather than
forcing Ctrl+Home. Resizing invalidates row/page geometry according to its width
and height dependencies while preserving the source anchor. Detailed tests and
remaining performance outliers are recorded in TERMINAL_PORTABILITY.md and
performance/2026-10-01-terminal-end/paired-8k.md. These earlier changes did not complete
horizontal read-only caret navigation or GUI virtualization; horizontal movement
is implemented in the later checkpoint below. Streaming terminal
large-file Find/F3 is now implemented through SessionSearch; CLI search-start/next/cancel expose the same task;
large GUI integration remains pending (see TERMINAL_PORTABILITY.md).

Provider coordination checkpoint: public SDK723 remains pinned. The next
retained DocumentView depends on native transactional presentation integration,
exact revision/source-display mapping and retained view lifecycle, followed by
public hit-test/selection geometry and installed-consumer validation. Source-only
models or private adapters do not satisfy the GUI requirement. Print/preview
still needs an executable snapshot/projection backend. The separate blank GUI
CPU investigation has not established a fix for the owner's 7% observation.


Consumer review of the proposed retained visible-window batch requires exact
revision/projection/config identities, real empty-row baselines and EOF anchors,
CR/LF/CRLF source ownership, and lossless completion delivery under a full
owner-dispatch queue. A practical initial cap is 512 visible/overscan row
descriptors within the same aggregate budgets; this is an implementation
checkpoint, not an owner-approved document limit. Typed unavailable for long
paragraphs leaves long-line support explicitly open. CSV/formula and rendered
Markdown displays need separate derived projections where display offsets are
not raw source offsets. Mirroring is valid only for the exact same immutable
projection; independent controls must not revoke the document's layout work.
These conditions were sent to the provider coordinator; no new SDK is accepted.


Horizontal read-only Left/Right and Shift selection are now connected to source
grapheme boundaries and visible caret geometry. An off-screen backward move
reconstructs the earlier row cooperatively; source and pending selection stay
unchanged until completion. Details and validation are in TERMINAL_PORTABILITY.md.
This closes the basic terminal horizontal-motion gap; GUI integration and broader
physical terminal dogfooding remain unfinished.

Read-only terminal Home/End now use cancellable, bounded logical-line scans with
Shift selection. This removes their editable-buffer fallback; desired-column
vertical caret motion and physical terminal dogfooding remain unfinished.

Large-file Save Text Copy implementation stage (2026-10-01): TextCopyStream now
accepts up to 64 KiB per append, retains at most three trailing bytes, preserves
complete UTF-8 across chunk boundaries, and replaces each illegal source byte
with exactly one space. Finalization consumes incomplete trailing sequences;
finished streams refuse further input. Validation/allocation failure preserves
pending state. The result reports illegal-byte count for that emitted chunk.

Tests compare all 65,536 split two-byte inputs with the established text_copy
operation, plus every chunk width for valid emoji/CJK/combining text, CRLF,
surrogates, out-of-range code points and truncated sequences. Oversized-input
refusal preserves a pending emoji. All 23 local tests passed in 11.35 seconds;
spelling audit passed across 119 source files. This does not yet enable the
paged Save Text Copy command: remaining work is a sibling temporary streaming
writer with atomic no-overwrite publication, source-stamped cancellable task,
terminal/CLI wiring and cross-platform failure/cancellation tests. The existing
16 MiB snapshot writer is not suitable for arbitrarily large copies.

The streaming copy writer is now implemented for Windows and POSIX in
NewFileWriter. It owns a sibling temporary, accepts bounded output chunks, and
publishes only to an absent destination. Windows renames the owned open handle
with replacement disabled; cancellation marks that handle for deletion. POSIX
retains the parent directory descriptor, validates temporary identity, links the
new name without replacement, removes the temporary, and flushes the directory.
Write/publication failure prevents further writes or publication. Final flush is
synchronous and is not yet an end-to-end responsiveness claim. Cancellation
cleanup is best effort in destructors; explicit publication/flush failures report
whether the new name was already installed.

Windows tests published 16 MiB + 64 KiB + 4 bytes, confirmed cancellation cleanup,
existing-name refusal, a destination created after preparation remaining intact,
repeated-publication refusal and absence of temporary leftovers. Full local
suite: 23 passed in 11.80 seconds. Final Windows path-size overflow guard passed
the session test again in 0.45 seconds. House-style audit: 120 files, no spelling
findings. Native POSIX validation is pending this push. The preceding Home/End
checkpoint 828dfb7 passed native run 36962515925 and portable run 36962515920.
The paged Save Text Copy command remains disabled until the source-stamped task
and terminal/CLI orchestration are connected and verified.

Paged Save Text Copy is now connected (2026-10-01). SessionTextCopy validates
source identity/revision/size for each bounded read-convert-write step and again
before publication. A ready task does not publish automatically; dropping it
cancels its owned temporary. The terminal Ctrl+T command advances paged copies
cooperatively and checks queued input before publication. Any key cancels; Escape
is consumed. Successful output leaves the open source, selection and revision
unchanged. CLI save-text-copy uses the same task synchronously; it supports paged
files but does not yet expose cooperative protocol advancement. Final OS flush
and atomic publication are synchronous and require separate latency measurement.

Tests cover one-byte task steps, cancellation without publication, a source
change after readiness, >16 MiB Session publication, actual 16 MiB terminal save
and Escape cancellation, plus CLI SHA-256 equality and existing-target refusal.
Full local suite: 23 passed in 11.15 seconds. Expanded CLI test passed in 1.15
seconds. House-style audit: 122 source files, zero spelling findings. Native
integration validation is pending. This supersedes earlier ledger statements
that paged Save Text Copy is unavailable; large GUI integration remains pending.

Cooperative text-copy protocol is now exposed as text-copy-start/next/publish/
cancel. Preparation and publication are separate, so clients can cancel even a
fully prepared copy. Invalid starts/budgets and premature publication preserve
the existing task; stale source publication fails and cleans up. CLI tests cover
a 16 MiB copy in 256 steps with SHA-256 equality, no destination before explicit
publish, one-byte reads across an emoji and illegal/truncated UTF-8, ready-state
cancellation, failed replacement start and source identity invalidation. The CLI
integration passed in 1.44 seconds; the 122-file spelling audit passed. This closes
the earlier cooperative-protocol gap; final flush/publication remains synchronous.

Text-copy timing now has a reproducible headless benchmark and retained local
raw evidence in performance/2026-10-01-text-copy. Conversion/write steps were
sub-millisecond in this sample, but final flush/publication reached 62.3312 ms.
This is an identified remaining interactive pause, not a completed lag audit.
Next implementation must move final publication off the interactive thread
without weakening destination refusal, cancellation boundaries or ownership.
The b5d0a88 paged-copy integration passed native run 36963297860 and portable
run 36963297895; cooperative CLI changes and this benchmark await their next run.

The terminal's final text-copy publication now runs on one owned background
thread. Source validation and the final cancel opportunity precede dispatch;
after dispatch the copy finishes and reports its result. Ctrl+X defers exit,
while failure cancels the deferred exit and keeps the terminal open. Worker state
owns the prepared writer and completion synchronization, never Session/UI
borrows. Destruction joins any outstanding publication. Only active publication
uses short condition-variable waits, so no idle polling is added. CLI publication
remains synchronous to preserve command response semantics. Local dispatch and
completion timings and limitations are in performance/2026-10-01-text-copy.
Full 23-test suite passed in 18.83 seconds; the added exit-during-publication
shared-loop test passed in a final terminal-app run of 5.09 seconds. Native
validation and physical input latency remain pending.

Native CI now runs the copy benchmark after the existing search benchmark and
retains only summary, raw samples and a source/provider/platform/executable
receipt. The fixture/output files stay in the runner workspace. Failed exits and
timeouts retain an explicit receipt and still fail the build. No numerical
performance threshold is asserted. This wiring awaits its first native run.

Additional publication-owner tests verify that a worker failure is reported,
a raced destination remains unchanged, and resetting the source Session after
publication dispatch cannot invalidate the worker's prepared-file ownership.
The focused session test passed in 0.96 seconds. House-style audit passed across
123 source files, and the updated native build script parsed successfully.

Publication-thread signal review: POSIX workers now inherit a blocked mask for
SIGWINCH/SIGINT/SIGTERM/SIGHUP/SIGTSTP, with the creating thread's mask restored
immediately after thread creation. This keeps lifecycle signals on the terminal
input thread and preserves its flag-check/pselect contract. Added POSIX mask
preservation assertions and an owned PTY SIGTERM case after publication starts.
Windows session tests passed in 0.96 seconds, house-style audit passed (123
files), and the PTY script parsed. POSIX execution is pending CI, not claimed from
Windows tests. The preceding ee23416 native run 36964143324 and portable run
36964143300 both passed.

A separate remaining input issue was identified by source review: POSIX
input_ready can report raw partial input, after which read waits for a complete
UTF-8/escape/paste event. During a background publication this can delay displaying
completion until more input arrives. A bounded nonblocking decode path and native
partial-input completion tests are the next fix; no idle polling is desired.

POSIX active-work readiness now decodes at most 4096 available bytes and reports
only a complete event, lifecycle signal or closed input. Incomplete UTF-8/paste
input returns control to background work rather than entering blocking read.
Escape-sequence deadlines survive both nonblocking probes and blocking waits;
the idle terminal continues using its ordinary indefinite input wait without
polling. Owned native PTY regressions withhold the remainder of an emoji and a
bracketed paste until a 16 MiB copy reports completion, then verify exact copy
contents, exit and terminal restoration. These POSIX-only edits passed source
review, the 123-file spelling audit and Python syntax validation. Native build
and runtime verification are pending; no Windows test is claimed as proof of the
POSIX behavior. The preceding native signal/benchmark run is still active.

2026-10-02 validation update: native run 36965181568 and portable run
36965181557 passed at 9474eacb6bda1fb0c0aadf3b491324e2cb34f388. This validates
the POSIX partial-input completion regressions on macOS and Linux and supersedes
the pending-native statements above. All three packaged startup checks passed.
The public dogfood-9474eacb6bda release contains Windows x64, macOS ARM64 and
Linux x64 archives with checksums and source/SDK manifests. Anonymous downloads
of all three archives were independently checked against their SHA-256 sidecars
and matching source revisions. Master pushes now publish an immutable prerelease
only after the complete native matrix succeeds. This is release infrastructure
completion, not completion of the product or its lag audit.

Mac CPU investigation update: provider Time Profiler run 36963650387 completed,
but attempt 076b0fda4a7943d1b97fffc20b12c0d8 still reports incomplete and
comparison_accepted=false. Both recorder processes exited successfully and the
traces contain time-profile and os-signpost table schemas. Focused profiled work
completed its 120.084828291-second span; symbolized sample analysis is pending.
Both cleared-focus workloads failed hold_duration_invalid (131.926753125 seconds
profiled, 131.528282875 seconds unprofiled). They cannot support a valid comparison.
The provider owner was asked to export/analyze the existing focused trace and
investigate the cleared timing failure without weakening its acceptance gate.
No Mac CPU fix or attribution is established by this capture.

Read-only vertical navigation review found that Up/Down currently move to row
starts rather than preserving the desired display column. A bounded frame-to-source
target mapper now resolves shared wrap boundaries in the same order as caret
rendering, clamps to legal grapheme boundaries, and handles blank rows and empty
EOF after wrapped CRLF. It does not yet change interactive navigation: persistent
desired-column state and cooperative offscreen movement still need integration.
Five focused regressions passed, together with all six terminal test targets
(8.50 seconds); a final rebuilt terminal target passed in 2.28 seconds. The
123-file house-style spelling audit passed. This is navigation groundwork, not
a claim that the reported behavior is fixed.

Read-only Up/Down now uses that mapping and retains the desired display column
through short lines and viewport scrolling. Shift retains the original source
anchor. Exhausted upward history uses the existing cancellable reconstruction
task before publishing the new caret; other commands and changed viewport
dimensions reset the desired column. The row mapping indexes only candidate-row
boundaries while applying canonical wrap/newline overrides from the full frame.
Shared-loop tests verify short-line column restoration, scrolling down and back
up, and the exact 156-byte Shift selection. All 23 local tests passed in 57.07
seconds; the 123-file spelling audit passed. Native validation is pending.
The existing 32768-arrow history stress case now makes terminal-app take 49.01
seconds locally versus roughly five seconds before caret-aware movement; these
are whole-test timings, not per-key latency. Repeated-key scheduling and frame
mapping need explicit measurement in the remaining lag audit. Page Up/Down still
use their older viewport-start semantics and remain a separate navigation task.

Visible-page decoding now starts with 8 KiB and doubles its source context only
when needed to fill the requested viewport or complete a grapheme. The maximum
individual window remains 64 KiB; retries total at most 120 KiB. Previously each
arrow-triggered rebuild decoded 64 KiB regardless of the visible text required.
The unchanged terminal-app workload fell from 49.01 seconds at 6cd2c9c to 10.08
seconds in the first local adaptive-window run and 10.64 seconds in the final
full-suite run. These are sequential, uncontrolled whole-test wall times, not
paired per-key latency samples or a platform-wide performance guarantee.
All 23 local tests passed in 18.40 seconds. Regression coverage includes a
12001-byte combining grapheme requiring growth beyond 8 KiB, subsequent visible
rows, and a large viewport requiring the full 64 KiB with an unfinished trailing
grapheme. House-style audit passed (123 files). Native validation remains pending;
the prior vertical-navigation native run 36966475497 is still in progress.

Page Up/Down now retain the visible caret row and desired column across page
changes, clamping to an available legal source boundary on a shorter final page.
Shift preserves the source anchor, including cooperative reconstruction of
expired backward history. The shared-loop round-trip regression verifies the
rendered cursor and exact 140-byte selection. All 23 local tests passed in 17.48
seconds; house-style audit passed (123 files).

Native run 36966475497 at the earlier 6cd2c9c checkpoint failed its Mac terminal-app
test at the unchanged 60-second timeout; the other 30 Mac tests passed. That
checkpoint predates adaptive visible-page decoding. Linux passed, while Windows
was still running when this result was inspected. The adaptive change must pass
a fresh Mac run before the timeout regression can be considered resolved.

Read-only navigation key repeats now run in slices of at most 16 movements,
with redraw and Escape cancellation between slices. Unexecuted repetitions stay
owned by the main loop and wait for cooperative row/line reconstruction; they
no longer enter key dispatch and cancel their own outstanding task. A genuine
new event that cancels reconstruction also supersedes its queued repetitions.
No idle timer or thread was introduced. Tests verify all 23 requested movements,
the unchanged source selection, Escape stopping a 1000-repeat event after the
first 16 movements, and reconstruction inside a repeated Up event. The expanded
terminal-app test passed in 17.49 seconds. The other 22 tests passed in the
preceding full-suite run; final test-only edits split the two distinct history
reconstruction cases. House-style audit passed across 123 files. Native validation
is pending; these event-count bounds do not establish a wall-time latency bound.

Native validation update: 11d8a9ca747a4681ed31a57828a41eb2629e5ac7 passed all
three native jobs in run 36966883286 and portable run 36966883356. Mac terminal-app
completed in 27.60 seconds under the unchanged 60-second timeout; all 31 Mac
tests passed in 65.42 seconds total. This clears the earlier CI timeout for the
adaptive decoding/page-navigation checkpoint, not the separate idle-CPU issue.
The automated dogfood-11d8a9ca747a release is public with all nine expected
archives/checksums/manifests. Repeat slicing at 970cd73 was pushed afterward
and requires its own native validation.

Provider ownership check: the File Manager coordinator's latest report identifies
the multiline-text chat as idle and the multi-paragraph document view as unfinished.
SwiftEdit requested an explicit owner and public-capability checkpoint for its
remaining document-view/mapping, multi-selection and print dependencies. These
must not be described as actively progressing without fresh evidence. The frozen
SDK remains unchanged and no provider source was modified.

DisplayPage source/display selections now use complete source graphemes rather
than individual Unicode scalars. Combining accents and joined emoji cannot be
split by either mapping direction. Byte-preserving segmentation metadata treats
each illegal byte as an independent control sentinel; displayed labels and source
bytes remain unchanged. Regressions cover combining text, joined emoji, illegal
bytes adjacent to combining marks, literal label-looking source, CRLF and maximum
expanded pages. All 23 local tests passed in 31.44 seconds; spelling audit passed
(124 files). This does not supply missing page-boundary context or claim GUI
DocumentView integration.

The coordinator subsequently transferred bounded ownership of the provider
prepared-window input/identity/admission prototype and focused tests to this
SwiftEdit chat. Initial edits must be new files only, with exact paths reported
before editing. Existing host/rendering/A2-worker files, canonical contracts,
public SDK availability, root CMake and workflows remain coordinator-owned.
Provider commits/pushes are prohibited for this assignment; source-review and
test evidence go back for independent integration. No workers or subagents.
This supersedes the idle-prototype ownership statement, not the capability gaps.
