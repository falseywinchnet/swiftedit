# MacBook dogfood implementation checkpoint

The owner's 2026-10-01 direction is a downloadable, usable SwiftEdit MacBook
application with cross-platform repository builds. This remains required; a CSV
library build or a passing source audit does not satisfy it.

## Current evidence

* No native SwiftEdit macOS package exists or has been verified.
* `portable-core.yml` now runs the real CSV/formula implementation and regression
  tests on Windows, macOS and Linux. Its artifacts are test diagnostics, not apps.
* Application startup now uses Windows Unicode arguments on Windows and ordinary
  process arguments on POSIX. Picker drive enumeration is Windows-only; POSIX
  uses the filesystem root. Local date insertion uses the platform localtime API
  and reports conversion failure. Windows build validation preserves current use.
* The native CMake build still intentionally refuses non-Windows configurations
  until real file and page adapters exist. No unsupported platform success claim.
* File Manager has a macos-arm64 dogfood app release, but its release assets do
  not supply the matching installed GUI.Forms/picker SDK required by SwiftEdit.
  The provider confirms no new prepared-text SDK is available yet.

## Resume in this order

1. Obtain a coherent installed public GUI.Forms Application + DocumentPicker SDK
   for macOS arm64 and Linux, including redistribution/runtime dependency data.
   Coordinate with toolkit owner; do not copy private provider implementation.
   SwiftEdit is private and has no repository secrets. Its GITHUB_TOKEN cannot
   read a different private repository. Mirror verified SDK-only archives into a
   pinned SwiftEdit dependency release, or configure narrowly scoped access.
   Do not expose a personal authentication token in workflows or logs.
2. Move Windows PagedFile code out of session.cpp into a platform adapter; add a
   POSIX descriptor implementation with bounded pread and mutation detection.
   Add a POSIX file adapter that preserves new-target non-overwrite, snapshot
   conflict detection, metadata, temporary cleanup and recoverability. POSIX
   advisory locks are not Windows deny-write-sharing; test and document that
   distinction instead of claiming equivalent mandatory protection.
3. Port process-ID test fixtures and writer-conflict tests. Update CMake platform
   source selection, shell32 gating and the macOS bundle target. Windows terminal
   code remains Windows-specific; the native app must not depend on it.
4. Add native build/package jobs for macOS/Windows/Linux consuming the pinned
   installed SDKs. macOS-26 arm64 matches File Manager's existing runner baseline;
   this does not establish support for Intel Macs or older macOS versions.
5. Bundle SwiftEdit.app with required dylibs/fonts/notices, fix and verify loader
   paths, ad-hoc sign the development bundle and verify startup plus fixture
   open/edit/save using native CI. Preserve test and source/hash receipts. Publish
   a clearly labeled dogfood archive and give the owner a working download link.
   Developer ID signing/notarization are separate from ad-hoc signing; do not
   claim either without actual verification.

The previous 3f26376 rendering checkpoint remains valid. A new public-API timing
probe shows installed TextStore::replace still costs about 41.6 ms median on a
1 MiB ASCII line; merely switching to that API will not remove edit latency.
Its samples are retained beside this handoff in performance/2026-10-01-navigation-probe/.
The full feature goal and final end-to-end lag review remain open.
