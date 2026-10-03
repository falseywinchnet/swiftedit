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

Native run 37096500201 at 6b0d1a8 passed Windows, Linux and macOS tests and
packaging. That validates the extent-index checkpoint, before the metrics-cache
and native stress-measurement follow-up below.

## Initial layout measurement reuse

A follow-up regression showed that initial layout resolves the same repeated
word and space thousands of times. Layout now owns a temporary metrics cache
for its current font. It retains only size/ascent/descent, at most 1024 entries
and 65536 text bytes; tokens longer than 256 bytes are measured without caching.
Map-node overhead is separate from the text-byte budget. Any FontSpec change
clears the cache, and no cache survives the synchronous layout call. This avoids
reusing metrics across font styles, painters, provider changes or later rebuilds.

The repeated-word measurement regression failed before the change and now
requires fewer than ten measurements for the 10000-word fixture. A separate
check verifies that identical italic, bold and code words each receive their
requested font measurements. Editor/views passed in 2.11 seconds; the expanded
view suite subsequently passed in 0.11 seconds. The spelling audit is clean.

The native Markdown test retains its styled-content capture and then loads a
10000-word stress quote. It observes the first native paint through a requested
10 ms timer and reports public presentation counters. A counting painter checks
that the resulting viewport contains between 1 and 2000 stress words rather than
a layout error or the entire document. The five-second readiness limit is not a
latency threshold. The native executable compiles; measured native results remain
pending. Initial parse/layout is still synchronous and not yet interruptible.
