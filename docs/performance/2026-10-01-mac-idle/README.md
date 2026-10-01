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

Application57f297157a8562199bbe9dd0c3b6028ad80c62c1, same SDK6def54a,
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
