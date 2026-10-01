# Development limitations

The expanded owner objectives and feature-by-feature implementation status are
in SWIFTEDIT_OBJECTIVES.md. The new CLI is a command protocol, not the promised
full-screen terminal, and the GUI has not yet migrated to its byte-faithful model.
CSV now has a native table view, formula entry/results and Convert to Value in
the menu and context menu. Its source still shares the current GUI text-widget
limits; broad desktop and responsiveness validation remain in progress. Grid
paste inserts plain clipboard text into the selected cell; rectangular clipboard
import and column resizing are not implemented.

- Windows is the first implementation. Other platform file adapters are not
  implemented or claimed.
- The current GUI.Forms multiline provider has a 1 MiB UTF-8 document limit and
  4096-byte logical-line limit. Files beyond either limit are refused intact.
  This is a toolkit development bound, not a final product decision.
- Scrolling currently uses the mouse wheel and caret reveal, without visible
  scrollbars. Tabs use four-space stops. Home/End address the visual row;
  Ctrl+Home/End address the document. Full bidi visual caret navigation, IME
  composition, accessibility text ranges and native screen-reader behavior
  require further provider validation.
- Font selection currently chooses a bundled toolkit role, size, bold and italic;
  it is not an arbitrary installed-font-family chooser.
- Case-insensitive search folds only ASCII A-Z. Literal non-ASCII text searches
  exactly. Query characters can be flagged as one-grapheme wildcards by right-click.
  Native Ctrl+? requires the provider's Windows key normalization update; normalized
  key events are covered by headless tests. Search orchestration remains synchronous
  and needs cooperative yielding/cancellation. No regex, whole-word or selection-only modes.
- No printing/page setup, Characters dialog, syntax
  colors, settings persistence, recovery files, installer, file association or
  OS-default changes.
- External changes are checked during saving. There is no filesystem watcher,
  automatic reload, merge view or force-overwrite action.
- Save As may ask for overwrite twice: the picker confirms its observation;
  the writer obtains fresh byte/identity evidence and confirms that destination
  before writing. This conservative behavior avoids silently treating an old
  picker observation as a durable reservation.
- The save adapter refuses hard links, reparse paths and alternate stream paths.
  Existing security/stream metadata uses Windows ReplaceFile behavior, but a
  broad ACL/ADS/encryption/volume-failure corpus has not been measured. Power-loss
  recovery and the final name-replacement race are not eliminated.
- File Manager picker enumeration remains synchronous. Very large or slow
  directories may block that owned dialog; this is provider-owned work.
- The stable native title is `SwiftEdit`; a document-name strip carries the full
  path and unsaved marker. Dynamic native title support is not exposed by this
  SDK snapshot.

The owner interview now provides direction. These gaps remain implementation
work, not claims of a finished Malkuth release.
