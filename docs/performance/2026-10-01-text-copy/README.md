# Streaming text-copy latency

Three 16 MiB fixtures, three trials each, measured on the local Windows host.
Every emitted copy was compared byte for byte with whole-source text_copy;
cancellation after 32 steps left no destination and preserved source revision.
Raw observations, executable/source hashes and method are retained alongside.

The 768 step samples per fixture had p99 wall times 0.4358 ms (ASCII),
0.2993 ms (Unicode), and 0.3076 ms (invalid bytes). Their worst observations were
0.4966, 0.4437, and 0.3789 ms respectively. Publication including final flush
was much slower: worst 44.2431, 62.3312, and 57.8577 ms respectively. This
identifies a remaining synchronous terminal pause; the feature is not yet
accepted as fully responsive. Publication p95/p99 equal the maximum because
there are only three samples per fixture, not because the tail is established.

Preparation worst was 1.0133 ms; cancellation worst was 0.2702 ms. No hard
latency bound follows from these uncontrolled local observations. OS flush and
publication should be moved off the interactive thread with explicit lifetime,
cancellation-before-publication and completion ownership. Cross-platform
measurements and interaction latency remain pending.
