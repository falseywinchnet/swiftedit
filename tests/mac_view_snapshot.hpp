#pragma once
// Test-only AppKit capture of this process's uniquely titled native window.
// Called on the UI thread after idle measurement. Throws on missing/ambiguous
// windows or capture/write failure; does not inspect another process's desktop.
void capture_native_view(const char *title, const char *evidence_name);
// Routes a synthetic Control+Shift+/ event through this process's visible
// AppKit window. No global keyboard injection or physical-keyboard claim.
void send_native_query_wildcard(const char *title);
