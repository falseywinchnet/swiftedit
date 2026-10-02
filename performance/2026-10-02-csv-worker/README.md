# CSV viewport worker checkpoint

Viewport formula evaluation now runs on one lazily created worker per CsvView.
The worker receives shared immutable Csv storage and an owned batch of at most
4096 cell addresses, matching the maximum 128 by 32 visible grid. It never calls
controls, windows, host handles or wake callbacks. A replaceable request slot and
at most eight queued completions bound the queue. The worker waits on a condition
variable when idle or when the result queue is full. It retains one successful
formula result for identical formula reuse; errors remain local to their roots.

Source and viewport changes request cancellation and discard queued results under
the same mutex used by publication. An obsolete request's stop token is checked
again before storing a completion. New work can replace a pending request without
joining the running worker. Completion addresses are checked before UI adoption.
Disposal cancels and joins, and the destructor also joins on other lifetime paths.
No worker completion can dereference a closed GUI object. All aliases must leave
submitted Csv objects immutable; CsvView constructs const storage directly.

While calculations remain pending, the UI schedules a completion check with a
one-millisecond requested delay. The UI drains results for up to eight distinct
formulas or a two-millisecond target, preserving placeholders, reference hover,
error display and exact stored source. No calculation checks are scheduled after
completion or cancellation. Empty checks do not explicitly invalidate the view.
The worker sleeps when idle. This avoids an after-close host-wake dependency, but
native scheduler latency and active polling overhead require measurement.

Queue bounds count results, not allocator bytes. Each result can retain up to the
existing 100000-reference evaluation limit; the active calculation, last reused
result, queued results and UI-consumed result may coexist. Allocation, reference
metadata preparation, source parsing, queue cleanup and joining are not guaranteed
to meet a wall-clock deadline. Convert to Value still computes synchronously;
this change moves viewport evaluation only.

## Measurements and rejected first attempt

Local Windows Release build using installed public aaca5d0 SDK. The existing
657927-byte distinct-formula fixture verifies 85 exact visible results on every
sample. Its harness now requests sleep_for(1 ms) between polling turns and uses
a five-second watchdog rather than a synchronous step-count limit. Actual sleep
granularity is uncontrolled and may be much coarser than one millisecond.

The first prototype handed off one cell per poll. The retained
single-result-poll.csv shows median completion of 1346.75 ms and maximum
1362.47 ms. That orchestration was rejected. The batched producer can calculate
ahead until its eight-result queue fills: batched-distinct.csv records median
185.796 ms, p95 201.105 ms and maximum 201.651 ms. The maximum initial UI handler
was 0.2016 ms and maximum measured UI slice was 0.4671 ms. The final small change
to suppress empty-check invalidation was made after these samples and passed the
full test suite; timings were not rerun for that change.

These totals include polling sleeps and cannot be compared as calculation-only
timings with earlier synchronous reports. The result delay remains materially
visible in this harness despite shorter UI work slices. Native input-to-screen
latency, cancellation/close latency and actual scheduler result delay are still
unverified; this is not a completed snappiness audit.

## Validation

All 28 local tests passed in 19.03 seconds. Worker tests cover source lifetime,
one-time consumption, a 40-result batch exceeding queue capacity, cancellation
with potentially queued results, 32 rapid cancellation/replacement cycles,
formula-error recovery and destruction with active work. View tests cover stale
source/viewport rejection, pending hover updates, duplicate formula reuse and
zero extra scheduling/dirty marks after completion. The C++ spelling audit passed
145 files; native execution remains pending CI for this checkpoint.
