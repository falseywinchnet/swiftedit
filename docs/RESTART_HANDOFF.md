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
