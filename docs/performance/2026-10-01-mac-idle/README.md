# Mac idle CPU baseline — unresolved defect

Owner report: SwiftEdit consumes approximately 7% CPU on an Apple silicon Mac
running macOS 26 while showing a blank document. No claim of acceptability or
resolved cause is made.

Native CI 36864154136 built application 5d09897b312dea056f60106ec71ff973877bc4c2
against provider 6def54ad2bf2089b57c09337c0b3f82cc1178187. All three native jobs
passed. The Mac packager ran the bundled app with an empty text fixture, removed
SDK font/dynamic-library environment overrides, waited five seconds, and sampled
process CPU time with /bin/ps around ten quiet seconds. The owned child was
terminated and reaped after observation.

- Runner: macOS 26.6.2 ARM64.
- Elapsed interval: 10.104475374999993 s.
- Process CPU delta: 0.24 s.
- CPU: 2.375185163930395% of one core, not normalized across machine cores.
- Focus, caret focus, occlusion, minimization and window backing scale were not
  controlled or captured. This is one observation, not a distribution or a
  reproduction of the exact owner machine's 7% reading.
- No pass threshold was inferred from this result.

The exact artifact manifest is baseline-manifest.json. Downloaded ZIP hash
matched its checksum file:
1e034a23307147ddfa0c398f2e01f8de83b79fad7a29cea1b637b4910be89faf.

Shared toolkit coordinator owns Mac host/caret/timer investigation and trusted
picker symlink navigation. Source inspection shows focused TextBox schedules
caret redraw at 530 ms; this is a lead, not a proven explanation. Hidden owned
window scheduling also requires profiling. Consumer Home/start path correction
is implemented and tested, independently of these unresolved provider defects.

## Native stack observation

Application57f297157a8562199bbe9dd0c3b6028ad80c62c1, same SDK 6def54a,
Mac job in native run36864723389 passed. Before profiling, CPU delta0.12s
across10.063658417s gave1.192409311% of one core. The difference from the earlier
2.375% observation is not an improvement claim: focus/occlusion/backing scale
remain uncontrolled and no provider fix is present.

After that CPU interval, /usr/bin/sample captured five seconds of the owned
child at a requested1ms interval. Main thread had3488 observations,3459 in the
mach_msg2_trap event wait. These are sampled stacks, not CPU-time percentages.
Active stacks include GUIFormsView drawRect/drawRetainedRect, TextBox on_paint,
RecordingPainter measure_text_utf8, SkiaRaster resolve_text_layout_utf8,
HarfBuzzFontEngine resolve/shape and FreeType glyph loading. Raster fills and
macOS image/color conversion also appear. No per-window identity was captured;
there is no evidence here to attribute the work to a particular hidden dialog.
The sample does not prove the cause of the owner's7% reading or establish an
acceptable idle budget. Raw sample, sampler status and manifest are retained.

Provider investigator separately found an initial-hidden-window occlusion
synchronization gap. Its causal contribution requires a native comparison;
it must not be conflated with this sample's text shaping/raster observations.
Both reports were delivered to the coordinator and Mac host investigator.

## Controlled focus comparison (first attempt; final phase failed)

Run 36865627733 / app 7b59e46 used the actual nine-window SwiftEdit topology and
unchanged SDK 6def54a. Three seconds settled startup; each measured interval
lasted about five seconds with a single explicit measurement timer.

| State | CPU seconds | Elapsed seconds | One-core CPU | Main paints/deadlines | Hidden Save picker paints/deadlines |
|---|---:|---:|---:|---:|---:|
| Document focused |0.142749|5.01702|2.84529%|9/9|9/9|
| Document text focus cleared |0.0328|5.01246|0.654369%|0/0|9/8|

Main painted damage totaled 4796820 area units during the focused interval;
Save picker totaled 115776 in each interval. All other owned dialogs had zero
paints/deadlines. Both pickers retained model focus and occluded=false; only
Save picker recorded repeated painting. Main had no measure/arrange passes,
so source/geometry layout was not repeatedly rebuilt in these intervals.

This is direct evidence of hidden Save picker drawing and focused main redraw
cost. It does not assign exact CPU fractions to individual windows, establish
physical MacBook equivalence, or prove the 7% owner reading fully explained.
An initial truncated-output summary incorrectly said every hidden child was
quiet. Full-row extraction revealed Save picker activity; the user and both
provider investigators received an immediate explicit correction.

The intended final hidden-main phase failed because the installed public handle
refused primary-window hide. The native run therefore FAILED; only the first
two observations exist. No hidden/minimized CPU result is claimed. Follow-up
bdbe270 removes the unsupported phase and reruns the supported two-state probe.
Raw first-attempt rows and failure are in controlled-first-attempt.txt.


## Completed controlled probe

The corrected two-phase probe, app bdbe270 in run 36866101259, completed on
macOS with unchanged SDK 6def54a. Its raw output is controlled-completed.txt.

| State | CPU seconds | Elapsed seconds | One-core CPU | Main paints/deadlines | Hidden Save picker paints/deadlines |
|---|---:|---:|---:|---:|---:|
| Document focused | 0.108723 | 5.06108 | 2.14822% | 9/9 | 9/9 |
| Document text focus cleared | 0.02113 | 5.0724 | 0.416568% | 0/0 | 9/8 |

Damage totals and active windows match the first attempt. This reproduces
hidden Save picker painting in a completed probe. It also isolates a repeatable
change when main text focus is cleared, while keeping the app and hidden-dialog
topology present. The measurement timer and sampling/reporting remain fixture
overhead. No minimized/hidden-primary state or performance acceptance is claimed.
Provider fixes must be compared against this same fixture and SDK provenance.
