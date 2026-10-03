# Selection context measurement

Baseline: 2457d1d with the new `tests/selection_bench.cpp` harness and CMake
target only. Candidate: the accompanying selection-context implementation.
Both use the same local Windows Release build and frozen aaca5d0 SDK.
The harness creates an editable Session with 80-byte LF-terminated ASCII lines,
selects one character near its midpoint, warms once, then records five samples.
Timed work includes SelectionSet construction and destruction; document creation,
copy, rewrite, rendering and native input are excluded. Other host activity is
uncontrolled. Five samples establish an observed range, not tail percentiles.

| Document bytes | Baseline observed ms | Candidate observed ms |
| --- | --- | --- |
| 65,536 | 1.9038–3.2574 | 0.0051–0.0066 |
| 1,048,576 | 28.4029–35.2520 | 0.0029–0.0044 |
| 16,777,215 | 468.463–507.793 | 0.0027–0.0032 |

Raw samples are `before.csv` and `after.csv`. The implementation no longer copies
and segments unrelated document lines for this selection. It finds the preceding
LF and next LF surrounding the ordered selection interval, segments that complete
context, and subtracts its base only for metadata queries. Source ranges remain
absolute. LF resets grapheme context, including regional-indicator parity; CRLF
is never split by the cropping boundary. No fixed scalar look-behind is assumed.

The regression compares acceptance of every ordered byte endpoint pair against
whole-document TextStore segmentation for a fixture containing empty lines,
CRLF, combining sequences, leading combining marks, malformed bytes, regional
indicators, an emoji ZWJ sequence, and lone CR. Existing parallel-edit, stale,
illegal-byte, copied-source and decoded projection tests remain in place.

Long lines without LF and widely separated ranges can still require large
synchronous metadata work. This is a measured common-case improvement, not
completion of the lag audit or proof of native GUI input latency. The old GUI
has not yet adopted the Session selection path.
