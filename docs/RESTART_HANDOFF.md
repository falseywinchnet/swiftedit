# Restart handoff — 2026-10-01 active implementation

Owner requests full implementation, followed by a measured lag/bug scan. This
checkpoint is NOT completion. The owner explicitly revoked the historical
shutdown freeze; the coordinating chat acknowledged the correction. Implementation
is active under the full-feature goal, followed by the measured lag/bug scan.
Do not interpret the earlier shutdown checkpoint as a current stop instruction.

Completed since bc8a331: atomic revision-checked source-range edits, inert bounded
source/display mapping, flagged grapheme wildcard search core, native CSV table
view/formula entry/results/dependency errors, menu AND right-click Convert to
Value with undo, table scrolling/rectangle clear/hover metadata. Checkpoint
 a4fa27d passed six headless suites, including actual context-menu command use.

Markdown work now builds: pinned MD4C v0.6.0 (MIT, unmodified upstream parser and
entity lookup), native inert block/span parsing, source/rendered toggle, common
blocks/tables/tasks, cached native painting, scrollbar navigation and URL tooltip.
No browser/HTML/SVG execution or image fetch. Images currently show alternate text.
Ruler is provisional visual spacing, not calibrated print-page geometry. Native
visual QA and full semantic house-style review remain pending. Spelling scan had
zero findings across 30 authored C++ files before shutdown.

Initial eight-suite run passed seven; view painting exposed RangeControl refusing
zero-width numeric ranges when content fits. Corrected Markdown scroll extents
and the corresponding single-row/single-column CSV case using a disabled minimum
nonempty scrollbar range. Final rerun result is recorded in VALIDATION.md.

Next steps:
1. Review/test single-cell/empty CSV and Markdown scrollbar cases, resize/font/
   DPI invalidation, callback disposal and preview error state. Native visual QA.
2. Continue first-party semantic style review (not just the spelling scan).
3. Integrate reviewed provider D1 then D2/D3/D4: new document identity+revision,
   exact page request tokens, source/display mapping and virtual document control.
   Current GUI still uses older bounded TextBox; don't claim byte-faithful GUI or
   large-file support yet. Provider owner:01a0f0bf-3d19-7fe1-8995-119065b2448b;
   registry owner:01a0f013-1863-7143-a336-1cd7b9122624; parent:
   01a0f009-7508-78a2-9fd4-cbf544e9193d. Read their reviewed D1 development contract.
   A rejected page retains producer slot/payload; destroy source/display/scratch
   then finish(exact token). Successful publish requires prior scratch release.
   Max2 producers includes cancelled work until release, plus1published page.
4. Finish GUI session migration, flagged wildcard UI, discontiguous selections,
   external-save conflict workflow/mixed endings, terminal screen, blank-line
   metadata, multiple windows, character tools and separate native print contract.
5. Run measured responsiveness/lag scan AFTER feature integration: cold/warm
   layout, worst-case CSV formula/viewport work, long lines, large files, typing,
   caret/scroll latency, cancellation, repeated edits/undo and shutdown. This scan
   has NOT been completed; don't represent warm-cache test as a lag certification.

Build: tools/Build-Windows.ps1 with house-style-final SDK pair, build directory
.build/swiftedit-resumed. Last-stage dist/SwiftEdit-resumed predates this checkpoint;
do not distribute it as these new binaries. No new release ZIP was made. Avoid
launching native desktop tests without coordination. Preserve older stages/SDKs.


2026-10-01: conflict core + owned review dialog implemented. Nine-window native
smoke passed (VALIDATION.md records hash). Ten headless suites pass. New Copy is
versioned, filename/encoding/endings editable; final Save binds reviewed snapshot
and document stamp, refuses changed fields/races. CLI remains strict. Continue
shared byte-faithful GUI/provider integration, terminal UX/metadata, print,
independent windows and measured lag scan. Active owner goal remains unfinished.

2026-10-01 context checkpoint: src/context.hpp/cpp implements bounded sequential
ContextCursor stamped by document identity/revision. CLI open emits context/page/
blank records, context restarts and context-next continues. Exact CRCR L<number>
CRCR markers are escaped separate metadata, never source. Eleven headless suites
pass; full terminal screen is still outstanding. Source CRCR stays byte-faithful.

2026-10-01 terminal model checkpoint: src/terminal_buffer.hpp/cpp drives Session
with grapheme/CRLF navigation, selection, preferred-column up/down, line cuts,
undo/redo and save boundaries. tests/terminal_tests.cpp includes illegal opened
bytes and undo restoring a scalar around an old caret. Twelve suites pass.
Next: actual terminal console renderer/input loop and large-file page navigation;
model alone is not the promised nano-like UI. Metadata rebuilding is synchronous;
final lag scan remains outstanding. Private navigation placeholders never reach
source/output. Session still refuses newly inserted malformed UTF-8, including
an internally cut malformed sequence; resolve explicitly during terminal work.

2026-10-01 terminal layout checkpoint: TerminalGlyph/TerminalRow now provide
inert control/illegal-byte labels, Unicode 17 width policy, tab expansion,
source/selection/caret cell mapping and horizontal clipping. New pinned width/
emoji sources and deterministic generator are checked in. Twelve suites pass;
51-file spelling scan clean. Console host/input loop is still next. Do not claim
host Unicode geometry proven; row construction still scans a full logical line.

2026-10-01 console executable checkpoint: src/terminal_windows.cpp builds
swiftedit-terminal.exe and a macro-selected opt-in owned-console smoke. Native
smoke passed after paired press/release fixture correction; no running process
remains. Thirteen headless suites pass. docs/TERMINAL.md lists shortcuts and
unfinished behavior. Next terminal work: large read-only pages, visual wrap,
search/wildcard, save review, external clipboard and responsiveness. Do not
claim full goal completion. Provider visual-only prepared paragraph proving
stage accepted with typed unsupported interaction; full interactive D4 remains
required, no consumable SDK yet.

2026-10-01 terminal pager checkpoint: src/terminal_page.hpp/cpp now drives
large-file readonly pages in terminal_windows.cpp. PgDn/Down next, PgUp/Up
previous, CtrlHome first. Bounded64KiB reads, 1024 cursor history, deferred last
unproven grapheme, literal label continuation and exact full-row/newline state.
Thirteen suites and extended native console smoke pass. Largefile readonly
selection/copy and granular navigation remain; wrap/search/wildcard plus final
latency/GUI/provider work still outstanding. docs/TERMINAL.md tracks limits.

2026-10-01 terminal search checkpoint: terminal_search.* and terminal_query.*
provide bounded Find/Next, wrapping, stamped selection publication/cancellation,
grapheme query editing and literal-preserving wildcard flags (CtrlShiftOEM2).
Thirteen suites and extended owned-console smoke passed. Native production host
routes query input to TerminalQuery, not filename input. Search currently valid
UTF8 editable-only; snapshot preparation remains synchronous. Replace controls,
wrap, large-file selection/search, clipboard/save parity and finalperf unfinished.

2026-10-01 terminal Replace All checkpoint: CtrlH query+replacement prompt drives
TerminalReplace/ReplacementScan, private bounded work, cancel/stamp/selection
revocation, one undoable whole publication. Deferred work exceptions now retain
input loop/session. Thirteen suites and native console three-wildcard-replacement
assertion/undo passed. Whole publication latency and pre-existing CRCR source
payload rejection remain documented limitations. Next full-goal work still wrap,
clipboard/save parity, large-file interaction, provider integration and finalperf.

2026-10-01 SessionReplacement checkpoint: existing CRCR replacement limitation
resolved through opaque Session-issued one-use operations, not a broad bypass
of payload validation. TerminalReplace now prepares/commits that operation.
Session validates only inserted payload, preserves source chunks, binds issuing
identity/revision, consumes completion once and moves output into change().
All thirteen suites passed; extra direct stale/no-op reuse tests passed. No new
native run in this slice. Full goal remains open, including snapshot/publication
latency, wrapping, clipboard/save parity and provider GUI integration.

2026-10-01 source clipboard checkpoint: Session now issues owned SourceClipboard
snapshots from checked source ranges. Terminal Copy/Cut/Paste uses these snapshots
to preserve malformed bytes and existing CRCR verbatim, including after the source
document is replaced. Cut captures before deletion; paste checks destination
identity/revision, range and the editable size limit before one undoable edit.
External payload validation remains unchanged. Build and all thirteen headless
suites passed (3.08 s total); regressions cover byte roundtrips, cross-document
lifetime, stale capture/destination, range/capacity refusal and undo preservation.
House-style spelling audit: zero findings across sixty authored files; semantic
review checked ownership, source-only construction and publication ordering.
No new native console run for this slice, and suite runtime is not UI latency.
Full goal remains open: wrapping, large-file interaction, external clipboard/save
parity, public-provider GUI integration and final measured lag/bug validation.

2026-10-01 terminal row latency checkpoint: bounded sparse per-line cell/source
indexes now serve the actual console host for viewport draw and caret reveal.
Document identity/revision invalidates cached entries; 320 lines maximum, one
checkpoint per 256 graphemes. Full-traversal equivalence tests and all fourteen
headless suites passed; coordinated own-console smoke passed and slot released.
Measured 1 MiB ASCII warm-row p50 127.2604 -> 0.0279 ms; insert+row 171.5926 ->
47.2718 ms. Raw samples, percentile distributions, workload and build details:
`docs/performance/2026-10-01-terminal-rows/README.md`. These are component timings,
not end-to-end snappiness evidence. Edit metadata still rebuilds synchronously;
wrapping, provider integration and final full lag/bug scan remain unfinished.

2026-10-01 MacBook dogfood direction: see MACOS_BUILD_HANDOFF.md. Native Windows build and all 14 suites passed after startup/localtime/picker guards. SDK-independent production CSV core build and tests also passed locally. New cross-platform core CI runs on push; its remote result is not yet known and it does not package the app. Public TextStore::replace probe retained; no speculative metadata optimization integrated. Mac SDK/POSIX adapters/native packaging still unfinished.

2026-10-01 paged file portability checkpoint: PagedFile/Page moved to independent
paged_file.hpp. Windows retained-handle semantics are in paged_windows.cpp; the
actual Windows Session links that adapter. paged_posix.cpp uses bounded pread,
regular-file/NOFOLLOW checks and before/after identity/size/mtime/ctime checks.
POSIX cannot prevent uncooperative writes; changed pages are refused, not frozen
by a mandatory lock. Common tests cover exact raw bytes, EOF, offsets, budget and
32 MiB bounded tail reads; POSIX adds mutation, symlink and nonblocking FIFO tests.
Windows full 15-suite run passed (2.43s); standalone core/readers 2/2 passed.
CI now compiles both adapters on their actual runners. Mac/Linux result pending
for this commit; native application save adapter/SDK/package still unfinished.

Verified cross-platform follow-up: commit 27ffbfc passed run 36848596407 on
Windows-2022, macOS-26 and Ubuntu-24.04. Both CSV/formula and paged-reader tests
compiled and executed on each runner. POSIX-specific changed-file, symlink and
FIFO checks passed on macOS/Linux. Evidence:
https://github.com/falseywinchnet/swiftedit/actions/runs/36848596407
This proves the tested file-reader component, not a native app or downloadable
Mac bundle. Next: POSIX save/conflict semantics and installed macOS SDK packaging.

2026-10-01 POSIX save checkpoint: 3e655d4 passed real Windows/macOS/Linux CI run
36849322565. POSIX adapter creates sibling files privately, preserves existing
permissions/attributes, checks snapshots and metadata before publication, uses
atomic no-overwrite link publication for new files and atomic name exchange for
replacement. Cooperating writers are excluded by flock. A detected race during exchange verification
leaves displaced bytes at the reported sibling recovery path and reports
failure; it does not claim the visible target was unchanged after an uncertain
publication. POSIX uncooperative writers are not prevented by mandatory locks.
Deterministic test-only publication hooks prove new-target collision refusal and
displaced-content retention. Ordinary saves delete temporary files; no persistent
history is retained on success. Unsupported atomic exchange/metadata copy fails
without a destructive fallback. New files start private (0600).
Main CMake now selects platform adapters and creates a macOS app bundle; Windows
console target stays Windows-only. Fixture process IDs, permissions and cooperating
writer ownership are portable. Windows native build + 15 suites passed (2.10s),
69-file spelling audit clean. Full Mac GUI/SDK/bundle validation remains pending.
The modification-time follow-up is being rechecked on macOS/Linux CI.

Verified follow-up f12d33c: cross-platform run 36849715223 passed Windows, macOS
and Linux, including fresh modification time after metadata-preserving save.
https://github.com/falseywinchnet/swiftedit/actions/runs/36849715223
The POSIX publication hook is compiled only into the standalone adapter test
target; production builds have no injected callback. No native GUI Mac build or
bundle has been validated. Provider-installed SDK archives remain outstanding.

2026-10-01 native newline correction: new documents and files without existing
endings now use LF on macOS/Linux and CRLF on Windows. Opened files keep their
first observed ending, including mixed-ending files. GUI New/initial construction
and terminal reset/initial construction use the same native_newline policy.
Windows full build and 16 suites passed (3.39s); standalone 4-suite gate passed.
Cross-platform CI checks the actual compiled host convention and preservation.

2026-10-01 Mac dogfood published: native run 36853053157 is fully green at
5d7d1c7 (Windows 17/17, Mac 16/16, Linux 16/16, including native lifecycle).
The Mac app is released as v0.2.0-dogfood.20261001. See DOGFOOD_2026-10-01.md
for source/SDK pins, checksums, packaging/sign/startup evidence and limitations.
The earlier missing-SDK/Mac-app gate is resolved. Next work resumes the unfinished
expanded feature set, especially shared-session GUI migration and public-provider
coordination; the final end-to-end lag/bug scan remains pending.

2026-10-01 continued consumer work: CSV cells containing triple backticks now use
the theme's code background, preserving literal source and selection priority.
This explicit 02:36 owner requirement was missing from the ledger and is now
recorded. Status columns now count graphemes, using binary lookup over the
existing TextStore line starts instead of scanning from the document beginning.
Regressions cover combining marks, joined emoji, CRLF/mixed endings, trailing
empty lines and empty content. Local pinned-SDK build and 16 suites passed in
2.29 s; 71-file C++ spelling scan clean. Cross-platform native CI is pending.
The provider coordinator resumed A2; it remains a visual-only initial service,
not the D4 editing capability required for complete GUI migration.

Follow-up: deeb5fe passed native Windows/macOS/Linux run 36854075164. The next
change splits selection refresh from full document refresh and retains cached
format/count metadata. On a 266240-byte warm selection/status fixture, p50 fell
from .1635 ms to .0035 ms; p95 .2006 to .0045 ms. Raw samples, source/binary hashes,
scope and reproducible command are in performance/2026-10-01-editor-navigation/.
This is component callback evidence only. Local 16-suite regression passed in
2.04 s; 72-file spelling scan clean. New native CI remains pending this checkpoint.

Verified follow-up: be353db native run 36854587884 and portable run 36854587765
both completed successfully across Windows, macOS and Linux.

Terminal wrap work now has a source-preserving visual-row core in
terminal_wrap.hpp / terminal_row.cpp. terminal_wrap_span computes one span;
terminal_wrapped_row produces source-mapped inert runs. Spaces/tabs are preferred
breaks, long words fall back at grapheme boundaries, tabs use visual-row stops,
and source whitespace is not trimmed. A full-width logical ending has an empty
continuation row for its end caret. Narrow views may clip a whole wide glyph or
inert label but never split its source range. All 17 local suites passed (2.69s),
including new conservation, resize-width, combining/wide glyph, malformed/control,
CRLF, and end-caret checks. This is layout groundwork, not a shipped wrap toggle.

Continue by integrating editable viewport/navigation and an explicit wrap toggle
in terminal_windows.cpp. Track top/caret visual positions as logical line plus
source offset, not byte-as-column coordinates. A nonterminal span advances by its
source length; a terminal span advances to the next logical line. Width and
DocumentStamp must invalidate retained geometry. Bound retained navigation data
and yield/cancel slow preparation; current first-use TerminalBuffer Unicode
metadata still rebuilds synchronously and is not made interruptible by this core.
Then coordinate the owned-console smoke and measure wrapped navigation/resize.

Verified follow-up: c993209 native run 36855505707 completed successfully on
Windows, Linux and Apple silicon macOS, including Mac packaging.

Wrapped navigation now has terminal_wrap_source for display-cell to source-caret
mapping, and TerminalBuffer::move_to for revision/identity-checked application
with anchor-preserving extension. Tabs, wide glyphs and labels map atomically;
past-row cells clamp to the last caret owned by that row (soft boundaries belong
to the next row). Tests sweep every display column in the layout fixtures and
verify that the resulting caret is painted in the requested row, preserve
backwards/across-anchor selections, and reject stale or split-grapheme positions.
All 17 headless suites passed in 2.37s; the expanded column sweep passed afterward.
The 74-file spelling audit was clean. These operations are not yet wired into
the console viewport or keys; the integration and responsiveness work above
remain required. No new native desktop launch was performed locally.

2026-10-01 wrapped viewport integration: TerminalWrapView now owns a bounded
visible-row collection and source-based visual navigation. Windows console F2
toggles no-wrap/wrap-to-window; arrows, Page Up/Down, Home/End and Shift extension
use visual rows when enabled. Ctrl+Home/End remain document motions. Width or
document stamp changes invalidate geometry; vertical movement retains display
column across shorter rows. Local 17-suite regression passed in 2.15s; 76-file
spelling audit clean. Prior 9b1b0b2 native run 36856216549 and portable run
36856216518 both succeeded on all platforms.

Coordinated owned hidden-console smoke passed exit 0 in under one second on
2026-10-01 04:39 local. It now also verifies F2, wrapped Down/Home insertion,
exact saved source bytes, return to no-wrap and restored console mode. SHA256:
3A4C4B9D94D05EA410FDD58A5C00590A5B7B2F371F6AB68CA481EA8427869F9D.
Logs: .build/swiftedit-sdk-6def54a/wrap-smoke-20261001-043922.stdout.txt and
.stderr.txt. Desktop slot released to coordinator immediately afterward.

Next: measure and improve long-line wrap location/reverse movement. Current
locate/previous scan logical lines synchronously and can repeat scans for a
page; do not claim responsiveness/cancellation complete. First-use Unicode
metadata is also synchronous. Geometry reset currently reveals caret at the
top after an edit/resize; preserve useful scroll context during subsequent
viewport refinement. Add measured long-line and resize evidence, then expand
the remaining owner feature set/provider integration. The goal stays active.

Wrap lag follow-up: sparse per-line checkpoints now avoid repeated full-line
scans for warm locate/previous. Entries cover at most 320 lines, with row starts
at least 4096 source bytes apart. Identity/revision/width invalidation and
prepare-before-publish are explicit. Added opt-in terminal-wrap-bench and raw
before/after CSVs in performance/2026-10-01-terminal-wrap/. For a 100 KiB ASCII
line, 31 Page Up + frame samples improved median 149.927 to 4.699 ms and worst
228.885 to 6.486 ms. Mixed fixture median 84.775 to 2.085 ms. These exclude
native console painting and initial Unicode indexing. Full 17-suite regression
passed 2.54s; checkpoint boundaries, 320-entry eviction and resize invalidation
also passed. First preparation still scans the full logical line synchronously;
do not infer cancellation or final responsiveness completion. Viewport scroll
context refinement after edit/resize remains open. Native run 36856797302 for
the preceding 1bd9a22 was last observed live; portable 36856797339 succeeded.

Viewport follow-up now preserves the last painted caret row across same-document
edit/resize; new documents reset it, and height shrink reveals it in bounds.
Sparse indexes extend only through requested positions, with valid row-prefix
publication and a completion flag; opening the start no longer scans the unused
line suffix. Tests cover row retention through edit/undo/resize, height shrink,
progressive checkpoints and eviction. All 17 suites passed (2.44s); spelling
audit 77 files clean. Added fresh-wrap-view-at-start workload: 1 MiB ASCII,
80x24, 31 fresh views after Unicode metadata preparation, median .4514 ms,
worst .5506 ms; samples and scope in the existing wrap performance directory.
Distant-position indexing and first-use Unicode navigation still synchronous;
cooperative/cancelable work remains the next lag gap. No native desktop run
was made for this follow-up. Previous 508c220 native CI 36857210839 remains
observed in progress; portable 36857212014 succeeded. Provider A2 remains
unavailable as public SDK; resumed its already coordinated work by message,
without assuming editing support from visual-only shaping.

Wrapped scrolling bug scan reproduced an exact-bottom boundary bug: one Down
past the last visible row jumped the caret to the top. The previous comparison
used a probe already advanced to that same first-offscreen row. Reveal direction
now compares against the old viewport top. A regression fails on the old code
and checks exact top offsets and caret cells across 40 downward and 40 upward
moves; the fix passes. Full 17-suite run passed in 2.73s; 77-file spelling audit
clean. No new desktop launch. Reconciled stale objective-ledger status for the
implemented terminal, Markdown/CSV views, control pickers and wrap; this does
not remove pending full GUI migration or final visual/responsiveness audits.

Checked installed public Application API again: fixed startup window vector,
maximum64, weak show/hide/close/fullscreen handles, no dynamic registration.
Relayed independent New Window runtime/lifetime prerequisite to coordinator.
A2 remains OFF-default draft work and not consumer SDK availability. Current
ae3cfc5 native run 36857611197 and portable36857611148 were observed live at
the start of this follow-up; continue checking those exact jobs.

Cancellation follow-up: TerminalWrapView accepts a synchronous borrowed named
control with typed TerminalWrapInterrupt. Polls occur at operation entry and
roughly each4096 processed source bytes during layout/index traversal. Whole
graphemes/rows remain indivisible; Unicode navigation metadata is not polled.
Cancelled page movement does not publish partial selection. Document motions
now bypass old-caret wrap lookup, allowing Ctrl+Home to recover cheaply. Windows
Console probes only queue-head Escape (and ignored key releases), preserving
other events. Wrap suspension leaves Save/Open/Exit and prompt Escape usable;
next editing/navigation retries. Probe disabled while prompts are active.
Borrowed control outlives view; no callback reentry/source mutation is allowed.

All17 headless suites passed3.42s, including mid-scan cancellation/recovery and
atomic selection. Coordinated hidden AllocConsole smoke passed exit0 around1s:
exactly one wrap cancellation, Ctrl+Home recovery, wrapped edit/save exact bytes,
and a separate dirty-exit Escape plus existing Unicode/undo/paging/mode tests.
SHA256 54D2CC77983F28A6B8FFB6C3AD1C328919DA0DFF709C38F473DEBB7C562D48FE.
Logs .build/swiftedit-sdk-6def54a/cancel-smoke-20261001-045524.stdout.txt and
.stderr.txt. Slot released immediately. Spelling audit77 files clean.

873838a native36857936261 and portable36857935904 both succeeded all platforms.
Earlier ae3cfc5 native36857611197 was cancelled/superseded, not passed. Current
wrap benchmarks use no console probe and do not measure poll overhead or total
Escape latency; broader interactive p50/p95/p99/worst work remains. Also retain
GUI/provider/full-objective gaps; cancellation here is only one owned phase.

Terminal Save Text Copy is now reachable through Ctrl+T and an editable filename
prompt. Uses existing Session sanitization and new-file-only publication, with
next dot-version suggestion. Escape writes nothing; destination collision leaves
the prompt open; successful copy preserves original path/bytes/selection/dirty
state and history. Read-only paged copy is explicitly unavailable. Save/Open/
Exit/Text Copy remain usable while wrap layout is suspended.

Headless17 suites passed1.66s including new terminal dirty-state/selection/stamp/
undo/redo preservation checks. Coordinated hidden-console smoke exit0 under1s
verified prompt cancel/nooutput, twoillegalbytes->twospaces with CRCR preserved,
original disk bytes unchanged, open document still dirty and exactlyone copy
success across an existing-target retry. Prior wrap/cancel/Unicode/paging/mode
checks also passed. BinarySHA256:
ACD4EF0647345C00AB8BF7380D3EE7A23A4C17E89468711E78A0D6C7D9D83F1D.
Logs .build/swiftedit-sdk-6def54a/textcopy-smoke-20261001-045937.stdout.txt and
.stderr.txt. Desktop slot released immediately;77-file spelling audit clean.
Previous11eafc0 native36858517854 still observedlive; portable36858518167 passed.
No full-feature/lag completion claim: GUIprovider, pagedcopy, synchronous metadata
and comprehensive native responsiveness work remain outstanding.

CI follow-up:11eafc0 native36858517854 failed before Mac compilation because
GitHub release SDK download returned HTTP500. Portable36858518167 succeeded.
Do not report that native run passed. Build-Native.py now retries only the
read-only SDK download up to3 attempts with2/4-second backoff; final failure
propagates and pinned SHA256/platform/revision validation remains mandatory.
Mocked immediate success, retry success and exhausted failure checks passed.
7b76967 contains the Text Copy implementation; its native36858915672 and
portable36858915692 were observedqueued before this retry-helper change.

Provider reports independent A2 service/session/layout/raster work compiles and
6 focused tests pass, with review pending at root. Window/Painter/native host
integration and SDK export are not available; keep6def54a pinned. This report
does not establish D4 editing availability or resolve SwiftEdit GUI migration.

CSV audit found/reproduced cache-order-dependent depth enforcement. A shared
dependency first reached shallowly was returned from the numeric cache before
checking a deeper caller's64-cell limit. Cached numbers now retain dependency
depth; active frames propagate maximum child path. Bothreferenceorders accept64
and refuse65 cells; refused conversion leaves formula source intact. Full17
suites passed2.71s;78-file spelling audit clean. No new native launch.

Added productionCsvView scroll baseline: 512x8 CSV,85visible formulas summing512
literal cells,5warmups+31three-row scrolls, exact rendered1024 results verified
outside timing. Median4.9806ms,p955.3842ms,worst5.3898ms. Source/binary identities,
raw samples and limitations are in performance/2026-10-01-csv-view/. This is
headless component evidence, not nativepaint or maximum-workload completion.
496bd40 native36859063461 succeeded; earlier7b76967 native36858915672 was
cancelled/superseded. Do not report the cancelled run passed. SDK remains6def54a.

CSV viewport audit reproduced a partially visible-column bug: at800px width,
keyboard selection treated the sixth sliver column as fully visible. A failing
regression established it. Navigation now uses whole-column capacity while
painting can include a partial column; explicit selection and geometry changes
also reveal the caret. Scrollbar extents use content minus whole viewport size,
reset fitting axes, and wheel input cannot move through a disabled placeholder
range. During development the full suite caught an obsolete horizontal offset
breaking the existing right-click conversion test; corrected extent/reset logic
fixes it. Regressions cover arrow reveal, explicit selection, shrink/expand,
smaller source and fitting-row wheel input. All17 suites passed1.75s;78-file
spelling audit clean. No local native desktop launch. Continued under the latest
goal continuation after the prior verified remote checkpoint; full feature and
native responsiveness completion remain open.

CSV rectangle Clear lag improvement: replaced repeated reverse erase (quadratic
suffix copying) with validated forward source-gap assembly. Source/delimiters/
record endings unchanged outside selected fields; missing cells in ragged
rectangles remain an atomic refusal. Added quoted/multiline/mixed-ending/empty/
ragged regressions.17-suite pass2.48s;79-file spelling audit clean. Opt-in
csv-clear benchmark at100000cells/899999bytes,3warmups+31samples: median85.4735
to0.3082ms; worst87.4842 to0.3371ms. Raw before/after, source/binary identities and
scope in performance/2026-10-01-csv-clear/. This excludes parsing, editor undo/
publication and nativepainting. No local desktop launch. Prior4e15c30 native
36860154826 last observedlive; portable36860154953 passed. Full goal still open.
