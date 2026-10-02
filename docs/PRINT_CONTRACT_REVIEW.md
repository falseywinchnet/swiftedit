# SwiftEdit response to candidate P1 print/preview contract

Reviewed 2026-10-01 against File Manager
`orchestrator/spec/contracts/GUI_PRINT_DEVELOPMENT.md` (candidate semantic draft).
This is a consumer reconciliation response, not an implemented API, SDK claim,
owner-selected numeric policy or completion of printing.

## Accept for the candidate boundary

SwiftEdit owns source bytes, document/projection identity, selected ranges,
plain/rendered interpretation and title. GUI.Forms owns page geometry/shaping,
immutable page leases, preview, owned native dialog and spool submission. Neither
side may change editor source, history, selection or save state while printing.
The immutable logical plan must be shared by preview and submission; preview zoom
must not repaginate. Native Print is sufficient explicit intent for that exact
snapshot and final ticket, with no extra confirmation invented by SwiftEdit.

Accept asynchronous production on named executors, bounded outstanding work,
stale-result refusal, old-page retention on replacement failure, and nonblocking
close with owners retained until callbacks/work retire. Accept distinct native
dialog cancellation, cancellation before submission, OS spool acceptance and
partial/unknown outcomes. Never automatically retry an ambiguous submission or
describe spool acceptance as physical printing success.

Native default paper/imageable geometry is preferable to guessing Letter/A4.
One copy, no automatic headers/footers and monochrome source text are reasonable
initial defaults. Twelve-point monospace and 36-point margins remain candidate
defaults subject to native ticket/imageable-area validation, not owner mandates.

## Consumer feasibility and bounded counters

1. **Editable source:** `SessionCopy` can collect owned exact bytes while checking
   document identity/revision before each 64 KiB step. No live Session is called
   from a worker. Publish a print snapshot only after complete collection; edits
   during collection refuse/cancel it. A completed owned copy can outlive edits
   or close and stays labelled with its captured revision. Its initial whole-copy
   reserve is synchronous today; this is not a hard-latency guarantee. The GUI
   still uses its older Document/TextBox path and needs a matching capture adapter
   until Session migration; existing Session code alone does not implement GUI
   printing. Projection and source storage require separately reported capacity.

2. **Paged source:** the current PagedFile/read handle is not an immutable snapshot
   guarantee. Keep `snapshot_unavailable` until a validated owned snapshot/copy
   strategy is implemented. This is an implementation gap, not an accepted product
   limit that removes large-document printing from the remaining work.

3. **Mode:** propose that invoking Print starts with the explicitly active source
   or rendered view mode, and that the owned UI exposes that choice. A rendered
   view must not silently print source merely because plain mode is the service
   default. Distinguish service default from consumer intent. The separate Markdown
   layout preview remains an owned window/dialog, with no source mutation.

4. **Selection:** default to whole document. A selection job must copy ordered
   source ranges under one stamp; never infer them later from a live control.
   For discontiguous ranges, represent boundaries explicitly in the projection
   and tag any consumer-chosen separator as synthetic (no source-byte extent).
   Do not concatenate unrelated endpoints into an invented word or claim the
   separator is document content. Exact selection presentation remains a consumer
   policy to document during implementation, not a newly inferred owner answer.

5. **Projection:** preserve source CRLF as one logical break, standalone CR/LF,
   empty/trailing lines and literal marker text. Invalid bytes and control labels
   use exact source extents distinct from projected UTF-8 positions. Literal
   strings resembling a control label remain ordinary text. Tabs need a declared
   consumer tab-stop rule applied to provider geometry; replacing them with a
   fixed space count before font/layout resolution is insufficient. Please make
   typed tabs or the equivalent exact tab-policy boundary explicit in P1.

6. **Rendered content:** bounded paragraphs/styles are a useful first milestone,
   but tables, tasks, code blocks and other supported Markdown/CSV view content
   cannot silently disappear or fall back to source. Typed unsupported-content
   refusal is acceptable in an interim profile; it does not complete rendered
   printing. Track the missing block types independently of a plain-print pass.

7. **Limits:** 64 MiB preflight, 64 KiB paragraphs, page/glyph/font/spool limits are
   proposed provider admission bounds. They are not owner-chosen editing limits.
   Report the named exceeded resource before submission and preserve the previous
   preview/source. A long paragraph cannot be split at an arbitrary byte/grapheme
   merely to satisfy a quota. No numeric default or bounded input by itself proves
   responsiveness, cancellability or full long-line support.

## Required evidence before SwiftEdit integration is called available

Require an independently installed matching SDK with capability/limit queries;
the provider owns the exact native backend subset. Apple silicon/macOS26 is the
owner's immediate dogfood platform. Keep Windows/Linux support explicit rather
than inheriting it from portable model tests.

Use the source cases generated by `Prepare-DocumentView-Fixtures.py` for control
collisions, invalid bytes, CRLF, long contexts and large-file admission. Add print
oracles only after font bytes, page geometry and projection version are frozen.
Verify preview/print plan equality, zoom invariance, final native ticket geometry,
selection snapshots, stale completions, busy/reservation failure, owner close and
source loss. Test fake-spool failure/cancel/unknown receipts before any native
submission. Native dialog ownership, cancellation, page-range/settings fidelity
and observed spool acceptance each need their own evidence. Do not print physical
test pages without a coordinated fixture destination.

Status: consumer semantic direction accepted with the counters above; no source
adapter, native preview, print job or installed print service is implemented here.
