# Full implementation continuation

Owner direction (2026-10-01): keep working through the feature set, then perform
a measured lag/bug scan. This file tracks work; unchecked items are not complete.

- [x] Separate Word Count GUI/CLI tool; committed bc8a331.
- [x] Final SDK clean build and headless integration.
- [x] Revision-checked atomic source-range edit core and bounded display mapping.
- [x] Flagged one-grapheme wildcard core with bounded search slices.
- [x] Stored CSV formulas, dependency evaluation, exact errors and circularity checks.
- [x] Native CSV view, cell entry, rectangular clear, draggable scrollbars.
- [x] Menu/context Convert to Value, undo, formula hover/reference highlighting.
- [ ] Source/display provider contract and coherent SDK integration.
- [ ] GUI shared session migration, invalid/control-byte editing and paged viewing.
- [ ] Discontiguous selection and flagged wildcard GUI/terminal controls.
- [ ] Mixed-ending save choice and full external-conflict/copy workflow.
- [ ] Conventional terminal screen and separate blank-line marker metadata.
- [ ] Native Markdown rendering, inert links, source toggle and rendered ruler.
- [ ] Native print, Markdown layout preview, multiple independent windows.
- [ ] Character/control inspector and insertion tools.
- [ ] Final native interaction/visual checks and measured lag scan/fixes.

CSV clarification: Enter stores a formula and displays its result. Convert to
Value appears both in the menu and cell context menu. Error conversion is refused
without mutation; successful conversion is one undoable change.

Current tests: all six headless suites pass after native CSV integration, 1.59 s.
The display suite covers mapping/atomic edits/wildcard slices. CSV and CLI tests
cover dependency changes/conversion/undo/circularity. Editor tests exercise the
native grid's source toggle, cell entry, menu conversion, right-click popup and
shared command invocation, clipboard cell source, undo and error preservation.
Native desktop interaction and final performance claims remain unverified.

Provider work is coordinated with the GUI.Forms owner and Orchestrator registry
owner through the original parent. SwiftEdit owns source bytes and save policy;
provider owns virtual text/shaping/selection/viewport and native print seams.
Frozen SDKs and Plan Paint remain untouched.
