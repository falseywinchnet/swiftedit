# Notepad repository instructions

Current owner direction authorizes implementation now; historical Text Editor
interview-first gates are not a stop condition for this standalone repository.
Read docs/DECISIONS.md before changing text/save semantics. Unanswered questions
remain provisional until the owner answers the future product interview.

Work in this repository with explicit working directories. No workers/subagents
or worktrees. Do not modify Plan Paint or File Manager/GUI.Forms source without
coordinated provider ownership. Consume installed public packages only. Preserve
the one-document, traditional-menu, owned-dialog topology. No browser runtime,
ribbon, tabs, IDE or sidebar. Tests mutate only their unique disposable fixtures.

Build and headless tests are authorized. Coordinate desktop launch/automation
because the owner is dogfooding other applications. Do not publish remotely
without checking the destination. Record measured test evidence honestly.
