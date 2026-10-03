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

## Sparse selection follow-up

Baseline for `--sparse`: dbd2fba with the extended benchmark harness only.
Two one-character selections lie near opposite ends of each same 80-byte-line
fixture. Same Release build/SDK, one warmup and five retained samples, timed
construction/destruction only. Raw evidence: `sparse-before.csv` and
`sparse-after.csv`. Host activity remains uncontrolled.

| Document bytes | Enclosing-interval baseline ms | Separate context groups ms |
| --- | --- | --- |
| 65,536 | 2.0683–3.2568 | 0.0053–0.0074 |
| 1,048,576 | 28.9104–30.1466 | 0.0077–0.0287 |
| 16,777,215 | 469.409–516.253 | 0.0053–0.0055 |

The earlier limitation for widely separated short selections is now addressed:
ordered selections share segmentation when their LF-delimited contexts overlap;
separate groups skip unrelated intervening lines. One metadata string reuses
capacity across groups. Counts remain global across all selections, preserving
the equal-grapheme rewrite rule. Large selected ranges and long LF-free lines
remain potentially expensive; there is no general worst-case latency bound.

The byte-pair oracle regression additionally combines each candidate range with
a separate initial grapheme, comparing acceptance and rewrite eligibility to
whole-document segmentation. Selection and projection suites passed in 0.25s;
149-file spelling audit is clean. No native GUI latency claim is made.
