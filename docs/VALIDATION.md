# SwiftEdit 0.2 checkpoint — 2026-09-30

Measured locally with GCC 16.2.0, Release C++20, Ninja, Windows x64.
Command: `tools/Build-Windows.ps1 -BuildDirectory C:/Users/Shadow/notepad/.build/swiftedit-dbe3766 -StageDirectory C:/Users/Shadow/notepad/dist/SwiftEdit -NativeTests`.

All six suites passed in 2.57 seconds:

| Suite | Seconds | Evidence |
|---|---:|---|
| document | 0.07 | Unicode/encoding round trips, safe publication, conflicts, read-only and hard-link refusal |
| session | 0.17 | Exact-context ambiguity, stale commits, undo/save boundary, opened restore, malformed bytes, sanitized copy, transport, naming, 16 MiB threshold and bounded pages |
| csv | 0.04 | Quoting, embedded newlines, local serialization, rectangular clear, exact decimals, explicit rounding, strict errors and reference metadata |
| cli | 0.59 | Real stdin/stdout process, explicit preview/commit, saved bytes, dirty-open refusal, exact CSV results and no autosave on quit |
| editor | 0.12 | Native-control headless input, save boundary, original restore, newline-conversion undo, large-paste cancellation, existing menu behavior |
| native | 1.57 | Self-closing owned windows, actual save, Find/Replace/undo, picker cancellation/reopening, clean shutdown |

Frozen SDK pair: `C:/Users/Shadow/file_manager/.build/sdk-checkpoints/dbe3766/windows-x64/{gui-forms-sdk,picker-sdk}`.
Combined SDK fingerprint: `CD8F39DA910556E56B401F783727BB4370BB363EBAE2F8E60B7C850908CEB654`.
Application DLL: `45CB7BB9267686F5E93333952F691B55F2C6DB0AA80D256EF9DF3DF2AB8C12EA`.
SwiftEdit.exe: `07FD6819BCDCE3D5F89F5C3B367C0CECC5200AA8BD218C37455BA6B18D90AE69`.
swiftedit-cli.exe: `FAECC56C7D59590E1852035AE940F148757756C180F6F89B857EA0031F512370`.

The GUI.Forms owner supplied and tested the additive clear_undo_history API;
the parent published a separate matching provider/picker checkpoint. No provider
source was modified by this consumer task. The original Notepad and Notepad-dpi
executable hashes remain respectively `9E4F5E85845E05AB74D0B7EE8100BAB18D78001EB30D658A1A2952AF59787C7B`
and `624A7A259C8C09CB86A991A9A746A04DF854909B6AA1B833E7A38DADE1B5AEC5`.

These are development tests, not full UI acceptance or cross-platform evidence.
The CLI/core large-file and CSV features are not yet present in the native GUI.
No 500 MB stress benchmark, full-screen terminal, visual CSV grid, Markdown
renderer, print pipeline, complete metadata editor, multi-selection or Unicode
control-glyph view is claimed. See SWIFTEDIT_OBJECTIVES.md for the remaining work.

---

## Historical Notepad evidence
# Windows development validation

## DPI follow-up checkpoint

The DPI-fixed public GUI.Forms SDK and matching clean rebuilt picker package
were consumed in a fresh build directory, preserving the original package:

```powershell
./tools/Build-Windows.ps1 -BuildDirectory C:/Users/Shadow/notepad/.build/windows-dpi -StageDirectory C:/Users/Shadow/notepad/dist/Notepad-dpi -NativeTests
```

**MEASURED:** all 3/3 suites pass in 2.44 seconds (document 0.08 s, editor
0.38 s, native 1.96 s). The consumer test additionally checks that scale
transitions 1.0 → 1.5 → 2.0 → 1.0 preserve document text and selection.

The provider independently reproduced the stale glyph-metric cache bug and
added device scale to the multiline cache key. Its focused regression covers
remeasurement, wrapping, row height, hit testing, caret scrolling and warm-cache
reuse; the provider reports 65/65 toolkit tests passing. The consumer test is
an integration state-preservation check, not a substitute for that metric oracle.

Updated launch path: `C:/Users/Shadow/notepad/dist/Notepad-dpi/notepad.exe`.

- EXE SHA-256: `624A7A259C8C09CB86A991A9A746A04DF854909B6AA1B833E7A38DADE1B5AEC5`
- GUI.Forms DLL SHA-256: `3C66E1C188A183EFF2C93833C7C9A3781DE57936A14721F00FA9DDBC865C664F`

The original `dist/Notepad` executable and DLL were not replaced. Build/staging
paths are configurable, and the build script refuses directories currently used
by a Notepad process. The source feature set and provisional text/save decisions
are unchanged.

## Original checkpoint

Date: 2026-09-29, Shadow Windows host, MinGW GCC 16.2.0, Release C++20.

Final clean command: `tools/Build-Windows.ps1 -NativeTests`.

**MEASURED:** 3/3 Notepad CTest suites passed in 2.03 seconds:

| Suite | Result | Coverage |
|---|---|---|
| document | Pass, 0.10 s | Strict UTF-8/UTF-16 round trips, BOM and mixed newline preservation, supplementary/combining characters, malformed/binary input refusal, literal search/replacement, create/replace, exact external conflict, read-only/hard-link refusal, deleted source, competing writer, failed-open preservation, UTF-16 save and successful temporary cleanup |
| editor | Pass, 0.15 s | Public multiline control input, configured CRLF Enter, menu undo/redo/cut/copy/paste/select-all/save/wrap, clean/dirty save boundary, mixed endings, oversized-line rejection, unsaved Cancel/Discard and failed Save cancels New |
| native | Pass, 1.77 s | Public Windows Application lifecycle, actual file save, owned Find/Replace and Font windows, literal Find and Replace All with single undo, shared Open/Save As cancellation/reopening, owner command restoration, all test windows close |

All file mutation fixtures are uniquely named disposable files. The native test
uses only its own application windows and public lifecycle/control calls. No
global keyboard, pointer, or desktop automation was used. Native tests are
opt-in because they briefly show self-closing application windows.

An initial harness assumed `request_close()` was synchronous. Native observation
showed a timer tick could arrive before the posted close event. The corrected
test waits for each actual closing callback before checking restored state.
This correction required no provider patch.

## Dependency checkpoint

Only installed public packages are consumed:

- GUI.Forms: `C:/Users/Shadow/file_manager/gui_forms/.build/shadow-sdk`
- File Manager: `C:/Users/Shadow/file_manager/.build/native-windows-x64/frontend-sdk`
- Toolchain: `C:/Users/Shadow/plan-paint/build-deps/msys64/mingw64/bin`

Provider-reported prerequisite evidence: GUI.Forms 65/65 tests, clean rebuilt
File Manager frontend 11/11 tests, independently installed picker consumer 1/1.
Those are provider reports; the three Notepad suites above were run here.

The multiline TextBox extension changes C++ object layout. Both picker and
Notepad were clean rebuilt against the new SDK. `Build-Windows.ps1` hashes all
installed headers, libraries and GUI runtime DLLs; a new checkpoint forces a
clean consumer build, and changes during tests/staging fail packaging. The exact
verified combined fingerprint is staged in `dist/Notepad/sdk-fingerprint.txt`.

The staging script recursively inspects executable/DLL imports and copies the
non-system runtime dependency closure. Fonts and their bundled attributions/
licenses accompany the application. The source is its own local repository;
there is no remote publication or installer/association side effect.

## Scope of the evidence

These tests establish the exercised development behavior, not an exhaustive
native acceptance review. Manual keyboard-only use, all DPI/font/script cases,
screen readers, complete IME/bidi caret behavior, large/slow filesystem latency,
power-loss recovery, broad metadata/ACL behavior and other operating systems
remain unverified. See KNOWN_GAPS.md and DECISIONS.md for precise restrictions
and the future product interview entrypoint.

## 2026-10-01 resumed implementation

Repository ownership resumed on explicit owner direction. Prior checkpoint
`e9d2715` was clean and matched origin/master on falseywinchnet/swiftedit.

Implemented Document > Word Count with document and optional selection counts,
plus the shared CLI word-count operation. Counting uses Unicode whitespace runs;
punctuation remains within a run. It is observational and does not change text,
selection or save state. Invalid UTF-8 is refused. Paged whole-document counts and
language-specific segmentation remain pending.

Clean Release build: .build/swiftedit-resumed; stage: dist/SwiftEdit-resumed.
Consumed the matching house-style-final GUI and picker SDK pair. Five headless
suites passed: document 0.49 s, session 0.59 s, CSV 0.07 s, CLI 1.15 s, editor
1.35 s; total 3.72 s. Native smoke executable compiled but was not run because
desktop launches require coordination. Thus the prior final-SDK build/headless
integration gap is closed; native desktop validation against it remains pending.
C++ spelling audit: zero findings in 17 files. git diff --check: clean.

Tests cover empty/whitespace text, CR/LF, apostrophes/hyphens, combining accents,
emoji, Unicode whitespace, zero-width space, unsegmented CJK, malformed input,
GUI document/selection reporting and state preservation, and the CLI response.

## 2026-10-01 full-feature continuation checkpoint

Native CSV/formula/source-mapping checkpoint a4fa27d was followed by Markdown
parser/native preview work. The first paint test found a zero-extent scrollbar
range error; fixed in both Markdown and single-row/column CSV handling.
Final full build passed all eight headless suites against house-style-final:
document 0.30 s, session 0.19 s, CSV 0.02 s, display 0.02 s, Markdown 0.02 s,
CLI 1.22 s, editor 0.26 s, views 0.13 s; total 2.18 s.
Native desktop QA, full semantic style review of this new scope, and the final
measured lag scan remain pending. See RESTART_HANDOFF.md. No updated release ZIP
or stage was published. Frozen SDKs and prior distributed packages are unchanged.
