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
