# Paired 8 KiB / 64 KiB terminal end scans

Local Windows headless measurement, 2026-10-01, SDK723cd7f. This accompanies
the smaller-step implementation and visual-column carry fix. Native validation
is pending. This is active terminal work, not the blank GUI idle CPU report.

Three owned 16 MiB files (ASCII, NUL controls, mixed UTF-8/CRLF), 80x24 viewport.
Both budgets run on the same Session/file/process in ABBAABBA order: four
trials per budget, no cache clearing or machine isolation. The final cursor
must agree across budgets; final Z must display, source must remain clean,
and cancellation must preserve the initial page. All correctness gates passed.

| Fixture | 8 KiB step p99 / worst ms | 64 KiB step p99 / worst ms |
| --- | ---: | ---: |
| ASCII | 0.8246 / 3.3896 | 4.6729 / 16.4770 |
| Controls | 0.8850 / 20.0236 | 5.3028 / 5.6147 |
| Mixed | 0.3824 / 0.6821 | 2.9594 / 3.1121 |

Smaller steps reduced typical and p99 interruption intervals in this run.
The control fixture still had a 20 ms outlier; worst-case latency is not
bounded by byte count. Total scan time did not uniformly improve: controls
median was 843.3169 ms versus 838.4797 ms, and mixed 472.6353 versus 459.1585 ms.
The additional yields trade throughput overhead for more cancellation points.
Do not extrapolate these distributions to a Mac or input-to-screen latency.

[Raw samples](paired-local-samples.csv) and [summary](paired-local-summary.txt)
retain all trials, including outliers. Percentiles use nearest rank, pooling
steps per budget. Wall intervals include clock-call overhead. Windows process
CPU samples are quantized in 15.625 ms increments here; short samples are not
precise per-step CPU costs. Process CPU is not main-thread CPU.

Local machine: AMD EPYC 9354 guest, 4 cores/8 logical processors; GCC 16.2.0
MSYS2 Rev4. The final local suite passed 22/22 in 7.71 s; spelling audit found
zero findings in 113 source/header files. Benchmark cursor-equality validation
was added after that suite and the benchmark was rebuilt and rerun successfully.
Independent CSV validation checked 55,464 finite nonnegative samples in 168
fixture/trial/operation groups.

Benchmark executable SHA-256: `b316e717ff4e93f8930b1be2f37329df87e1c5f9db9194ba96933ad973b6ba32`.
