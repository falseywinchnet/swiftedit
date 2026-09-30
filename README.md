# Notepad

A standalone conventional plain-text editor for Windows, built in C++20 with
the installed GUI.Forms SDK and File Manager Document Picker package. No ribbon,
tabs, browser runtime, or persistent side panels.

Features: multiline editing, New/Open/Save/Save As, unsaved-change prompts,
undo/redo, clipboard editing, literal Find/Replace, word wrap, status and a small
font dialog. Open and Save As use the File Manager subset in an owned window.

This is a development build. File semantics and unresolved interview questions
are explicit in [docs/DECISIONS.md](docs/DECISIONS.md). Supported encodings are
strict UTF-8 and BOM-marked UTF-16 LE/BE. The original encoding, BOM and line
endings are preserved. The editable limit is 1 MiB of UTF-8 and 4096 UTF-8 bytes
per logical line. Unsupported files are refused before replacing the document.

## Build

Run `tools/Build-Windows.ps1` in PowerShell. The default dependencies are:

- `C:/Users/Shadow/plan-paint/build-deps/msys64/mingw64/bin`
- `C:/Users/Shadow/file_manager/gui_forms/.build/shadow-sdk`
- `C:/Users/Shadow/file_manager/.build/native-windows-x64/frontend-sdk`

Only installed public packages are linked. The GUI.Forms SDK must include the
negotiated multiline TextBox extension, and the picker SDK must include the
explicit trusted-local-host authority and admitted-roots extension. Building
against an older package fails rather than silently substituting a private UI.

The build script compiles and runs headless document/consumer tests, then stages
the executable, dependency DLLs, fonts and documentation in `dist/Notepad`.
It does not launch a window or perform desktop automation.

After coordinating desktop availability, `tools/Build-Windows.ps1 -NativeTests`
also runs a self-closing owned-window smoke test using disposable text files and
the public Application lifecycle. It does not send global keyboard or pointer
input. SDK contents are hashed; a changed or unknown checkpoint forces a clean
consumer rebuild, and SDK changes during the build abort packaging.

Launch `dist/Notepad/notepad.exe`, optionally with one quoted file path.
Paths are parsed using the native Unicode command line. Settings are session-only.

## Source ownership

This is its own Git repository. Plan Paint is a read-only lifecycle/build
blueprint. GUI.Forms owns text editing mechanics; File Manager owns its reusable
picker and filesystem-browser provider; Notepad owns byte interpretation,
document lifecycle and actual writes. No provider-private source is copied here.
