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
