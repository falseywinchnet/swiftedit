# Standalone GUI.Forms build adoption

Compared with PlaySuite `origin/main` at `80f5ba8` on 2026-10-07, especially
`applications.yml`, `restore_toolkit_cache.py` and the public CMake targets.
SwiftEdit now uses the same reviewed GUI.Forms source pin, `e589142`, and its
LLVM 22 contract. `ci/dependencies.json` is the authoritative full-revision and
archive-digest lock. The File Manager picker is separately pinned at `964261f`.

## Reuse and boundaries

- GUI.Forms is checked out at `gui_forms/`, with its build at
  `.build/native-<platform>/gui-forms/`. This matches the provider cache layout.
- All components use named `clang`/`clang++` drivers, the provider's
  `cmake/llvm22.cmake`, and ccache with a workspace base directory and compiler
  content checking. Windows uses CLANG64, not the previous GCC/MINGW64 packages.
- The published cache archive digest, repository, revision and platform are
  checked before extraction. Path and link checks precede writes. An unavailable
  download permits source compilation; a mismatched payload is an error.
- Provider cache entries are imported before SwiftEdit's own cache restore.
  Compiler/environment identity separates CI cache keys. Compatible prior
  application objects remain available across source changes.
- The renderer has a separate source/output cache. macOS runtime imports are
  verified by the provider's payload/compiler/SDK validator and minimum-platform
  audit, with source rebuild on mismatch.
- GUI.Forms and the picker are built by their own CMake projects, installed
  together and consumed through `GUIForms::Core`, `GUIForms::Application` and
  `FileManager::DocumentPickerView`. No provider-private implementation is
  copied into SwiftEdit. The picker-only target does not build backend services.
- SwiftEdit's compiler cache is saved after successful compilation, before
  tests/package startup. A cached object is never treated as test evidence.
- The duplicate core matrix is manual-only; its additional file-adapter test
  is now part of the normal SwiftEdit suite. Native tests and packaging remain
  required on every supported platform before release publication.

## Entry points

`prepare_dependencies.py --platform <platform> --restore-cache` prepares the
reviewed packages. `CMakePresets.json` defines matching configure/build/test
presets for `windows-x64`, `linux-x64` and `macos-arm64`. Local concurrency is
two jobs; dedicated CI runners use four.

`swiftedit-app` builds GUI/terminal/CLI; `swiftedit-check` builds and runs tests;
`swiftedit-package` creates and checks the platform archive. The Windows staging
wrapper uses this same toolchain and incremental build. It checks SDK content
fingerprints without cleaning reusable objects on every dependency refresh.

## Measured local validation

Windows x64, Clang 22.1.8, Release, two jobs, 2026-10-07:

- The pinned GUI.Forms and picker compiled and installed successfully.
- An unchanged application build reported `ninja: no work to do`, taking
  1.369 seconds including CMake regeneration for added convenience targets.
- After deleting only generated application outputs through Ninja's clean
  target, rebuilding took 20.543 seconds including linking. All 103 compilation
  calls were direct ccache hits; zero misses. This measures local warm-cache
  reconstruction, not the hit rate of the imported provider archive.
- Cache admission tests cover successful import, wrong digest, wrong revision
  and path traversal, with no writes on rejected input.
- LLVM's Windows DLL boundary exposed leaf scrollbar RTTI assumptions in the
  view tests. Tests now inspect the exported ScrollBar interface and explicitly
  check orientation. No production navigation assertions were relaxed.

Native CI results must be recorded separately; local headless checks do not
establish macOS/Linux packaging or physical input latency.
