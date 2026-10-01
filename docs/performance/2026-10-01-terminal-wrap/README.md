# Wrapped navigation component measurement

The initial wrapped viewport repeatedly scanned from the logical line's start
when locating a caret or moving backward. A page movement repeated that scan
for each visual row. The candidate stores sparse visual-row starts at least
4096 source bytes apart, for at most 320 logical lines. Binary search chooses
a nearby checkpoint; source grapheme layout still determines the actual row.
Document identity, revision and viewport width invalidate the entire cache.
Entries are fully prepared before publication and evicted in a bounded cycle.

`swiftedit-terminal-wrap-bench samples.csv` measures the production view, with
an 80-cell width and 24-row height. It inserts one long logical line, moves to
the document end (preparing Unicode navigation metadata before timing), then
records its first frame. It measures 31 successive Page Up plus frame calls.
Each operation is followed by checks that the caret is visible and the source
is unchanged. Checks and sample writes are outside the timed interval.

| Fixture / milliseconds | Before p50 | After p50 | Before p95 | After p95 | Before p99/worst | After p99/worst |
|---|---:|---:|---:|---:|---:|---:|
| 102400 ASCII bytes | 149.927 | 4.699 | 219.139 | 6.394 | 228.885 | 6.486 |
| 106496 mixed bytes | 84.775 | 2.085 | 114.089 | 3.317 | 115.612 | 3.448 |

The mixed fixture repeats ASCII, tab, CJK, a combining cluster, Escape and an
emoji. The single first-frame observations were 15.966 to 8.002 ms for ASCII
and 8.400 to 4.372 ms for mixed. These observations include wrap preparation
but exclude the prior Unicode metadata build. They are not cold-start latency
distributions. Raw samples are in before.csv and after.csv. Nearest-rank p50,
p95 and p99 use sorted zero-based indexes 15, 29 and 30 of 31 samples.

One sequential run per implementation on the same Windows host; background
activity and power state were not controlled. Environment: Windows 11 Home
10.0.22621, AMD EPYC 9354, MinGW GCC 16.2.0, CMake Release,
.build/swiftedit-sdk-6def54a, provider SDK 6def54a. No native window or console
painting is timed. These are component measurements, not input-to-screen or
maximum-file latency guarantees.

Baseline production commit: 1bd9a22793eb8980196183c95cc8bd767efb0741.
Candidate terminal_wrap_view.cpp blob: c600aa9fb8f07d3977d35bd8dd1a72302696a71e.
Candidate header blob: 6f4f51e032cb6109d10b9f877757866cbc027df2.
Shared benchmark source blob: f44a1a9af82aeabdd1583aa8fad92a35d5167e18.
Baseline executable SHA256:
5BC3DBECE04F6F11682482385088F575E806F215B0F48825E5A94C1FEBD702CD.
Candidate executable SHA256:
C4E2BC6832F118A7E42E127DCD345E49CBCA900B593DD5182866C73082B195C9.

All 17 headless suites passed in 2.54s. Additional checkpoint-boundary, eviction
and width invalidation regressions passed afterward. First line preparation,
edit-time Unicode metadata rebuild, resize, and cancellation remain synchronous
work needing improvement. Very large graphemes can exceed a checkpoint's usual
source interval. No claim of complete responsiveness or final bug audit is made.
