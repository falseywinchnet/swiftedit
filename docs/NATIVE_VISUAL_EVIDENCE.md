# Native visual evidence

The native CSV regression now creates an AppKit view PNG on macOS after its
settled-idle observation. The fixture shows numeric formula results, a division
by zero error, a referenced formula, literal text and triple-backtick code
shading. It uses the installed public GUI.Forms SDK and production CsvView.

The test-only Objective-C++ adapter finds exactly one visible window with the
fixture's unique title in this process's NSApplication window list. It captures
only that window's content view, using AppKit's bitmap caching API. It uses no
private provider header, global input, screen-recording request or other
application's window. Temporary Objective-C objects use ARC and a scoped
autorelease pool; no native pointers survive the callback.

PNG encoding and writing occur after the idle metrics have been sampled.
Missing/ambiguous windows and capture/write failures fail the native test.
Artifacts remain in a newly created PID-specific build directory and are
uploaded even when a subsequent build/test step fails. Existing evidence is
never overwritten by a rerun using the same directory.

This is a native view redraw into a bitmap, not a compositor screenshot.
Successful capture alone does not establish correct pixels, keyboard behavior,
desktop placement or acceptable latency. The artifact still requires inspection.
It is also a direct CsvView fixture rather than the full editor menu topology.
Rendered Markdown and other platforms still require separate visual evidence.

Apple documents the capture semantics in
[cacheDisplayInRect:toBitmapImageRep:](https://developer.apple.com/documentation/appkit/nsview/cachedisplay(in:to:)?language=objc).

Initial implementation: the local Windows Release build passed all 20 headless
tests in 4.99 seconds. The spelling audit covered 103 C++/Objective-C++ files
with zero findings. Ownership/failure review checked UI-thread admission,
unique window selection, scoped native lifetime, post-measurement capture and
failure propagation through shutdown. macOS compilation, artifact generation
and visual inspection remain pending native CI.
No local native window launch is required or claimed.
