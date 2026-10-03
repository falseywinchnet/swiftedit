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

Native run 37096824234 at decb372 passed all three platforms and packaging.
Exact first-paint diagnostics are retained in native-*.txt. Single observations:

| Platform | First paint observed ms | Presented duration ms | Visible stress words |
|---|---:|---:|---:|
| Windows x64 | 21.6673 | 21.0547 | 396 |
| Linux x64 | 87.0944 | 86.827551 | 408 |
| macOS arm64 | 70.7535 | 60.649167 | 324 |

These include layout and native presentation work under uncontrolled CI host
conditions. The observation timer adds delivery latency. Different native font
metrics/window sizes affect visible word counts. No before/after native speed
ratio is claimed. These durations leave initial-layout responsiveness unfinished.

## Cancellable parsing worker foundation

`parse_markdown` now accepts a stop token and throws `MarkdownCancelled` without
publishing a partial model. Checks bracket validation, run at each MD4C callback,
run during attribute decoding, and precede final publication. Exceptions stay
inside callback boundaries until MD4C returns. Validation, library work between
callbacks, allocations and cleanup remain noninterruptible within those calls.

`MarkdownPreparation` owns one worker, one replaceable source request and one
completed model. It owns source bytes by value, publishes only while its request
token remains current under the publication mutex, transfers failures through
an exception pointer, and sleeps when idle. Replacement/cancellation clears old
results. Shutdown requests stop and joins. It never retains a UI callback, painter
or native window handle. Caller operations are serialized on the owning executor.
The active source/model may coexist with one pending source and a completed model;
source copying and cancelled-model cleanup are not hard-latency bounded.

All 29 local suites passed in 19.97 seconds, including owned source lifetime,
32 rapid cancellation/replacement cycles, invalid UTF-8 error/recovery, empty
completion, one-time adoption and active-work destruction. Precancellation tests
cover empty, valid and invalid UTF-8 sources. These do not measure mid-library-call
cancellation latency. The spelling audit passes 148 files. The worker is not yet
connected to MarkdownView; that integration and native validation remain next.

Native run 37097170527 at 249f49a passed all three platforms and packaging for
the worker foundation.

## Visible-view worker integration

MarkdownView now submits owned source to the worker and displays a preparation
message instead of parsing on the GUI thread. A pending-only frame request checks
for completion. Source replacement revokes old work and presentation; only the
current completed model is adopted. Errors display an explicit diagnostic.
Escape and leaving rendered mode cancel pending work. Reopening the same cancelled
source submits a fresh request. Disposal cancels and joins before releasing the
worker. No polling remains after completion or cancellation.

The view still copies source, releases prior presentation allocations and performs
native text layout on the UI thread. This integration removes parsing from that
thread; it does not yet make initial layout interruptible or guarantee a deadline.
Both preparation and native layout must finish before presentation_ready is true.
Pending/failed models do not expose old link hit regions or an old rendered page.

Editor/view tests passed in 2.09 seconds. They cover pending display, latest-source
adoption, source-mode cancellation, reopening, Escape, invalid-UTF-8 diagnostics,
source preservation, and zero scheduled requests/dirty marks after completion.
Existing layout tests explicitly await model adoption before testing geometry.
Native tests now await completed presentation, verify settled-idle scheduling and
close with a replacement still pending, measuring through owner release. The
native executables compile locally; execution of this integration is pending CI.

Native run 37097550906 built all three platforms, but the Markdown probe timed
out after its stress paint. The observer changed its own timer interval after
resetting activity counters, contaminating the zero-request assertion. Its timer
callback also lacked a failure boundary that closed the window. The probe now
begins measurement on the following tick and captures exceptions before closing.
This preserves the settled-idle assertion rather than relaxing it. The corrected
native probe compiles locally; four focused headless suites passed in 2.12 seconds
and the spelling audit passed across 148 files. Native revalidation is pending.
