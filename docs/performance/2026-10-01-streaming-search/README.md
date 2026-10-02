# Streaming search responsiveness baseline

The harness `tests/session_search_bench.cpp` measures the shared SessionSearch
implementation used by terminal Find/F3 and CLI search-start/next/cancel.
Three owned 16 MiB sources (ASCII, NUL controls, mixed Unicode/CRLF) end in Z.
Each source has three literal-Z trials followed by three trials with a flagged
one-grapheme wildcard followed by literal Z. Comparison budget is 4096;
ordinary source acquisition is 8 KiB, with adaptive context as documented.

Correctness gates require a complete match ending at the final source byte,
literal length one, wildcard length at least two, unchanged document revision
and clean source. Each cancellation sample drops a separate pending task after
32 work steps. All 18 completed-search and cancellation gates passed locally.

| Fixture/query | Step p50 ms | p95 ms | p99 ms | Worst ms |
| --- | ---: | ---: | ---: | ---: |
| ASCII literal | 0.0222 | 0.2251 | 0.3001 | 0.6694 |
| ASCII wildcard | 0.0158 | 0.1830 | 0.2055 | 0.5215 |
| Controls literal | 0.0222 | 0.1826 | 0.3013 | 2.4084 |
| Controls wildcard | 0.0158 | 0.1670 | 0.1832 | 0.5353 |
| Mixed literal | 0.0815 | 0.1472 | 0.1911 | 0.4208 |
| Mixed wildcard | 0.0148 | 0.1422 | 0.1612 | 0.4454 |

Local whole-scan medians ranged from 326.4632 to 509.8953 ms. Largest measured
cancellation release was 0.0025 ms. This measures destruction only, not the time
between a physical key and cancellation. Work steps include both acquisition
and comparisons, whose costs differ; pooled percentiles are not per-phase data.

[Summary](local-summary.txt), [compressed raw CSV](local-samples.csv.gz), and
[receipt](local-receipt.json) preserve the run. CSV validation checked 258174
finite nonnegative samples across 90 fixture/trial/operation groups. Compression
was verified byte-for-byte. Receipt hashes identify the runtime, harness and
executable. Raw CSV checksum refers to its original CRLF bytes.

Machine: Windows guest on AMD EPYC 9354, 4 cores/8 logical processors,
GCC16.2 MSYS2 Rev4. Cache and background load were uncontrolled. Percentiles
use nearest rank and pool steps across three trials. Wall intervals include
CPU-clock overhead; GetProcessTimes samples are quantized in 15.625 ms units
here and are not reliable short-step CPU attribution. Process CPU is not
main-thread CPU. The mode order is fixed; do not infer a controlled performance
comparison between literal and wildcard modes.

Native CI now executes the same harness and retains raw samples, summary and
source/SDK/executable receipt. Native results are pending. The 120-second
process timeout is a run bound, not a latency acceptance threshold. There is
no invented performance pass threshold. These six simple-pattern cases do not
cover long/repetitive patterns, all grapheme shapes, or physical UI latency.
The final application lag audit and the owner's blank GUI CPU defect remain
open.

Harness integration validation: full local suite 23/23 passed in 8.62 s;
source spelling audit passed 117 files and CI Python syntax parsed successfully.
