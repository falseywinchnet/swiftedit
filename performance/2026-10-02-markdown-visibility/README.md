# Rendered Markdown visibility audit

The former viewport lookup started at `top - maximum_run_height`. A quote rule
or table border spanning a long paragraph made that global height enormous, so
painting and link hover traversed many off-screen text runs. Painting submitted
those runs to the native painter even though they were clipped later.

The replacement keeps stable run order and builds a balanced extent index after
layout. Each subtree records its greatest bottom coordinate. Its first run's
sorted top coordinate provides the other bound. A synchronous cursor skips a
subtree when it cannot intersect the viewport or pointer's vertical position.
Paint and hover share this traversal. Tall overlapping decorations still appear;
they do not force unrelated off-screen text to be submitted.

The existing 250000-run ceiling bounds the index to 524288 doubles (4 MiB).
Replacement may temporarily retain both old and new indexes. A cursor borrows
the immutable layout only during one synchronous call and uses a fixed 32-entry
stack; the admitted tree needs fewer than 20 pending branches. Index preparation
is linear after the existing sort and remains synchronous. This is not a fix for
initial whole-document Markdown layout latency or a completed responsiveness
audit. These index bytes are additional to the existing text/URL payload budget.

The new 10000-word quote regression failed against the preceding renderer.
With the index, it verifies fewer than 200 submitted text runs in a scrolled
140-pixel-high viewport and retention of the intersecting tall quote rule.
Additional tests verify the tail link's actual hover hit after End navigation
and a tall quoted table with both its border and quote decoration retained.
The final view suite passed in 0.12 seconds. Editor and view suites passed in
2.21 seconds before the final test extension; no application code changed after
that combined run. All 145 files passed the spelling audit. Native executables
compile locally; native validation of this change remains pending.

This evidence concerns submitted drawing work and hit correctness, not measured
wall-clock speed, raster output, physical input latency or all Markdown behavior.
