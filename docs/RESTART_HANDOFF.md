# Restart handoff — 2026-10-01 shutdown checkpoint

Owner requests full implementation, followed by a measured lag/bug scan. This
checkpoint is NOT completion. Parent reported an imminent machine shutdown and
requested immediate source freeze and remote push.

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
