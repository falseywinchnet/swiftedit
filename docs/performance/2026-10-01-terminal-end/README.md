# Read-only terminal end-navigation measurements

This is partial responsiveness evidence for the shared end-navigation component,
not the final application lag audit and not a Mac GUI idle-CPU result.

The local Release run uses runtime implementation ef156ec, installed SDK
723cd7f, GCC 16.2.0 (MSYS2 Rev4), and a Windows guest reporting AMD EPYC 9354,
4 cores and 8 logical processors. Background load and filesystem cache were not
controlled. Harness SHA-256:
`5880ebb7cd22768f8cf00c5840ef6cde668dbf0c530878eb656ae9f0bdc911e1`.
Measured executable SHA-256:
`dd500b80ccb71e3286aa4df3e1102a0e46a2e3619b3c60db77f419564f393aa2`.
Raw CSV SHA-256:
`4f754829a43ed3cf7300b609e8d436d4a34f8104d08e0b7591c24804caa48477`.

Each fixture is exactly 16 MiB and ends in ASCII Z: one long ASCII line, NUL
controls, and mixed tabs/Unicode/combining marks/CRLF. The viewport is 80×24.
Five complete scans per fixture produce 1,285 step samples per fixture. Timing
uses steady_clock and includes clock overhead. Percentiles use nearest rank;
step samples are pooled across the five scans. With only five samples, p95/p99
for publication, release, cancellation and total equal the observed worst case.
These tail numbers do not establish a confidence bound.

| Fixture | Step p50 ms | p95 ms | p99 ms | Worst ms | Total p50 ms | Total worst ms |
|---|---:|---:|---:|---:|---:|---:|
| ASCII | 3.1222 | 5.0095 | 5.3608 | 5.7132 | 856.0200 | 977.0819 |
| NUL controls | 3.2805 | 5.3466 | 5.8931 | 6.7856 | 924.8295 | 1050.3808 |
| Mixed | 1.8076 | 2.9874 | 3.1769 | 3.9301 | 513.6218 | 537.2705 |

Worst publication plus first-frame construction was 0.6903 ms. Worst pending
task release was 0.2447 ms. A key arriving during a step still waits for that
step to return; task-release timing alone is not cancellation latency. Native
event delivery, screen drawing, display latency and a real user's machine are
outside this headless benchmark. No timing pass threshold is asserted.

Every completed trial verifies EOF, the final Z, and unchanged document state.
Cancellation runs 32 steps, drops the pending task and verifies that the pager
remains at its original source offset. Raw fixture files remain only in the
owned build output directory. The CSV and summary are retained here.

Reproduce with `swiftedit-terminal-end-bench <new-output-directory>`. The output
directory must not exist. Native CI now runs the same harness after its tests,
with a 120-second process bound, and uploads raw CSV, summary and a receipt
containing source, SDK, platform, executable hash and measurement scope. Native
benchmark execution has not yet been verified at this checkpoint.
