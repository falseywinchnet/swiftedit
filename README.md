# SwiftEdit

A native C++20 text editor built with the installed GUI.Forms and File Manager
Document Picker packages. Traditional dropdown menus, owned dialogs, plain-text
clipboard, no ribbon, browser runtime or persistent recovery files.

**Downloads:** [SwiftEdit releases](https://github.com/falseywinchnet/swiftedit/releases).
Choose a cross-platform dogfood prerelease and download the archive for your
platform. These are public downloads; GitHub sign-in is not required.

| Platform | Archive | Launch after extracting completely |
|---|---|---|
| Apple silicon, macOS 26 | `SwiftEdit-macos-arm64-*.zip` | Open `SwiftEdit.app` |
| Windows x64 | `SwiftEdit-windows-x64-*.zip` | Open `SwiftEdit/SwiftEdit.exe` |
| Linux x64, Ubuntu 24.04-compatible runtime, X11 | `SwiftEdit-linux-x64-*.tar.gz` | Run `SwiftEdit/SwiftEdit` |

Mac builds are ad-hoc signed and not notarized; Windows builds are unsigned.
Each release includes SHA-256 checksums and source/SDK manifests. The terminal
editor is included on all three platforms; Windows and Linux also include the CLI.

Native CI builds and tests Windows x64, macOS ARM64 and Linux x64 on every push.
It consumes the matching installed SDK archives pinned in
`ci/native-sdk-lock.json`. After all three native jobs and packaged startup checks
pass on a master push, CI publishes an immutable `dogfood-<commit>` prerelease
containing all three archives. Pull requests build and test without publishing.
The
[first release evidence](docs/DOGFOOD_2026-10-01.md) records the exact checks.

This development checkpoint adds the SwiftEdit identity, separate as-opened
restore, last-save undo boundary, grapheme character/selection counts, explicit
LF/CRLF conversion, date/time insertion (F5), bracket-based filename suggestions,
and confirmation before pasting more than 500,000 bytes.

`swiftedit-cli.exe` provides a separate explicit command session with byte-faithful
editing, exact-context preview/commit, stale revision guards, bounded file pages,
read-only mode at 16 MiB, invalid-byte save refusal and Save Text Copy. CSV
commands preserve unaffected source spelling and offer strict exact arithmetic.
See [command protocol](docs/COMMAND_PROTOCOL.md) for commands and examples.

The GUI and command model are not yet unified. The GUI still uses the public
TextBox's 1 MiB / 4096-byte-line bounds and supports UTF-8 plus BOM-marked UTF-16.
The command model preserves raw bytes and only publishes valid UTF-8. Native
Markdown/CSV views, the conventional full-screen terminal and remaining provider
features are tracked in [the objective ledger](docs/SWIFTEDIT_OBJECTIVES.md).
This is not a claim that the full expanded product is finished.

## Build and run

Run `tools/Build-Windows.ps1` in PowerShell. Default compiler is the existing
MinGW-w64 installation at `C:/Users/Shadow/plan-paint/build-deps/msys64/mingw64/bin`.
The frozen matching SDK pair is
`C:/Users/Shadow/file_manager/.build/sdk-checkpoints/house-style-final/windows-x64/`:
`gui-forms-sdk` and `picker-sdk`. The TextBox `clear_undo_history` API is required.
Only installed public packages are consumed; no provider-private source is copied.

The script hashes SDK contents, cleans on any checkpoint change, compiles, tests,
and stages `dist/SwiftEdit/SwiftEdit.exe`, `swiftedit-cli.exe`, runtime DLLs, fonts
and documentation. Use `-NativeTests` to run the authorized self-closing native
smoke. It never sends global desktop input. Use `-BuildDirectory` and
`-StageDirectory` for separate checkpoints; running executables are protected.

Launch `dist/SwiftEdit/SwiftEdit.exe`, optionally with one quoted Unicode path.
Run `dist/SwiftEdit/swiftedit-cli.exe` with stdin/stdout pipes for command sessions.
No installer, association, OS-default change or remote publication is performed.
The old `dist/Notepad` and `dist/Notepad-dpi` dogfood copies are preserved.

## Ownership and boundaries

This is an independent repository at `C:/Users/Shadow/notepad`; the source
directory and historical internal C++ namespace have not been renamed.
GUI.Forms owns native editing mechanics, File Manager owns the picker, SwiftEdit
owns interpretation, session behavior and publication. See
[known gaps](docs/KNOWN_GAPS.md) and [validation](docs/VALIDATION.md).

## MacBook and cross-platform builds

Native GUI and terminal builds, tests and packaging run on Windows, macOS and
Linux in [the native workflow](.github/workflows/native-builds.yml). Download
applications from Releases; CI diagnostic artifacts contain additional test and
performance evidence. Older Mac-only releases remain available.

These are development prereleases. The reported blank-window Mac CPU issue is
still open, and the expanded feature set is not complete. See the
[objective ledger](docs/SWIFTEDIT_OBJECTIVES.md) for remaining work.
