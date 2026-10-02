# Query editing component measurements

Measured on Windows x64, AMD EPYC 9354, using GCC 16.2.0 (MSYS2 Rev4),
CMake Release (`-O3 -DNDEBUG`) and public GUI.Forms SDK
723cd7f9f8016d854f667f41e0c5dbcbe4cc0adf. Application source is commit
166b912b8b576e7d95ebd2e5171c1017ee4120ec plus the accompanying benchmark and
CMake target additions. The source changes in this checkpoint do not change
query editing behavior. Receipt recorded 2026-10-02T06:26:18Z.

Benchmark executable SHA256:
`2331de7a0b6f4f6b1bf78102fed9b259378d140e6436a50b85ab3eeb82a1f650`.

Each fixture starts near the 4096-byte query limit with every grapheme flagged
as a wildcard. Each trial inserts literal `x` at the beginning via Window text
dispatch, then performs Ctrl+Z via Window key dispatch. Assertions verify the
inserted character is literal, the original occurrence retains its flag, and
undo restores all original text and flags. Each fixture has 20 unrecorded warmup
pairs followed by 200 recorded pairs. The 1200 raw operation samples are in
`samples.csv`; no outliers were discarded. Percentiles use nearest ranks.

| Fixture | UTF-8 bytes | Graphemes | Operation | p50 ms | p95 ms | p99 ms | Maximum ms |
|---|---:|---:|---|---:|---:|---:|---:|
| ASCII | 4095 | 4095 | Insert | 1.1843 | 2.0242 | 2.8021 | 3.2811 |
| ASCII | 4095 | 4095 | Undo | 0.6826 | 1.1333 | 1.5267 | 1.7033 |
| Unicode é | 4094 | 2047 | Insert | 0.4710 | 0.7025 | 0.7777 | 0.9539 |
| Unicode é | 4094 | 2047 | Undo | 0.2575 | 0.4436 | 0.4976 | 0.7104 |
| Combining e + acute | 4095 | 1365 | Insert | 0.3377 | 0.4237 | 0.4889 | 0.5423 |
| Combining e + acute | 4095 | 1365 | Undo | 0.2300 | 0.2794 | 0.3719 | 0.3813 |

Timing begins immediately before routed dispatch and ends on return. It includes
query snapshot preparation, edit validation, flag mapping, history and UI
invalidation. It excludes fixture preparation, correctness checks, file output,
painting, OS input delivery and physical presentation. No native window is
launched. This single-machine component measurement is not a hard deadline,
cross-platform comparison or completed GUI responsiveness audit. Scheduling and
cache state are uncontrolled.

Reproduce with the installed public SDK configured normally:

```powershell
cmake --build .build/swiftedit-sdk-723cd7f --target swiftedit-query-edit-bench --parallel 2
.build/swiftedit-sdk-723cd7f/swiftedit-query-edit-bench.exe samples.csv
```

The executable requires the same compiler and SDK runtime DLL search paths as
the other SwiftEdit headless test executables.
