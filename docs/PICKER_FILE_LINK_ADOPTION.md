# File-link picker adoption probe

The installed0322371 SDK supports directory-link navigation but not file-link
opening. Provider candidate2d04310 failed native validation; do not advance the
pin or claim this capability until its corrected candidate is verified.

`notepad-editor-tests --picker-file-links` is an explicit consumer adoption
probe. It is compiled against installed public headers, uses the retained picker
controls, and is intentionally not yet registered in the ordinary CTest suite.
It creates one uniquely owned disposable directory containing a relative file
symlink and its target. It then:

1. Navigates the Open picker to the fixture and selects the visible alias.
2. Activates Open and checks that SwiftEdit displays the target's original text
   and records the canonical target path.
3. Edits and saves through the normal editor command.
4. Checks the target's exact bytes, clean document state and preserved symlink.

The probe requires actual symlink creation and never reports a skipped fixture
as a pass. Local Windows compilation succeeds, but this desktop's symlink
privilege limitation and the old SDK mean execution evidence is still missing.
The ordinary20 local suites passed in3.52s; this result does not cover the probe.

After provider native fixtures and archive receipts pass, verify the downloaded
SDK contents, update the dependency pin, and register this probe in CTest on
hosts that can execute real links. Run it on macOS and Linux and in the native
Windows runner before publishing the picker fix. The provider's own cases
remain responsible for profile/filter/root policy, broken/cyclic/stale aliases
and unchanged Save/Export refusal; this probe verifies SwiftEdit integration.
