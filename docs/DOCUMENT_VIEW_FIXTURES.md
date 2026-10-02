# Installed document-view adoption fixtures

Run tools/Prepare-DocumentView-Fixtures.py with a new output directory. It refuses
to overwrite an existing directory and writes seven deterministic source files
plus a manifest containing sizes, SHA256 hashes, source probes and scenario names.
Generated files belong in .build, not the repository. No private layout engine,
provider development headers or geometry expectations are introduced.

Fixtures cover literal control-label collisions versus actual NUL/bidi controls,
invalid UTF-8 bytes, mixed newlines, literal CRCR metadata markers, combining text,
CRLF and emoji split across64KiB boundaries, a grapheme longer than64KiB, a100000-
byte logical line, and a16777227-byte read-only file. Same-sized revision A/B
sources support late-result/token invalidation tests. The manifest describes the
sequence; executing it requires the forthcoming installed token/ownership API.

Two fresh generations produced matching manifests and every byte probe passed.
The existing shared Session CLI opened the large fixture read-only (info size
16777227,dirty0,read_only1) and returned the exact final11 bytes, last page +CRLF,
at offset16777216. This proves fixture/source-model facts, not DocumentView
availability, rendering, cancellation or stale-token cleanup. Future consumer
validation must exercise those behaviors against an independently installed SDK.

Long-context cases must verify the provider's documented bounded outcome without
inventing a grapheme boundary, collapsing bytes or silently changing source.
Do not call a permitted refusal full long-line rendering support.
