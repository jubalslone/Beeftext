# Lean Beeftext (TLF Safe Build)

This is a deliberately restricted Beeftext build for ordinary law-firm text expansion. It is not intended to be feature-compatible with upstream Beeftext.

## Security boundary

A combo may generate text, ask the user for text, apply the allowlisted date/text transformations below, and reposition the caret within text it just inserted. A combo may not read the clipboard or environment, execute a process, synthesize arbitrary keys or shortcuts, introduce delays, or contact the upstream updater.

The variable allowlist is:

- `#{date}`, `#{time}`, and `#{dateTime}`
- custom `#{dateTime:...}` formatting and validated time shifts
- `#{combo:...}`, `#{upper:...}`, `#{lower:...}`, and `#{trim:...}`
- `#{input:...}`
- one exact, case-sensitive `#{cursor}` marker

Everything else, including PowerShell, environment, clipboard, Discord clipboard, key, shortcut, delay, malformed date-shift, and unknown variables, remains visible as literal text.

## Restricted text output

Substitution no longer uses the clipboard or a synthetic paste chord. Deletion, UTF-16 Unicode insertion, and bounded cursor-left movement are submitted as one serial Windows input transaction while Beeftext's keyboard hook is disabled.

For safety, line endings are inserted visibly as `\n`, tabs as `\t`, and other control characters as `\uXXXX`. Multiline expansion is therefore intentionally unavailable in this build: a line break must never become an accidental Enter/submit command in the target application. Valid UTF-16 surrogate pairs, including emoji, are preserved. Unpaired surrogates are made visible.

Cursor movement is enabled only when there is exactly one exact `#{cursor}` marker and all text after it is printable ASCII. Otherwise the marker remains literal and no cursor key is sent. This keeps every cursor-left event bounded within the newly inserted suffix.

Windows may reject synthetic input into a higher-integrity (for example, administrator-elevated) target because of UIPI. Lean Beeftext reports this failure; running Beeftext elevated is not recommended.

## Portable data boundary

The packaged build contains `Portable.bin`. In portable mode, settings, combos, logs, and backups live under the package-local `Data` folder. The combo-list and custom-backup location controls are unavailable, imported preferences cannot redirect either path, and updater cleanup cannot delete a path from old settings.

## Updates and builds

Upstream update checks and custom PowerShell configuration are disabled. Do not install an upstream Beeftext update over this build. Review upstream changes, merge or cherry-pick them into this fork, and rebuild.

The Windows workflow checks out the exact source commit, refuses dirty tracked source, builds committed sources without rewriting them, runs the restricted parser tests, and records the source commit in `BUILD_INFO.txt`. Verify `SHA256SUMS.txt` after distribution.

## Windows QA checklist

1. Extract the artifact to a normal writable folder. Confirm `Portable.bin`, `BUILD_INFO.txt`, and `SHA256SUMS.txt` are present, then launch `Beeftext.exe` without elevation.
2. Create and trigger `leanv` → `Lean Beeftext is alive.` Confirm the exact sentence replaces the keyword and no literal `v` remains.
3. Trigger `Today is #{date}` and a snippet containing `#{time}` and `#{dateTime}`. Confirm each produces text.
4. Trigger `Hello #{input:Name}.` Enter `Ada`; confirm `Hello Ada.` and confirm Cancel leaves the typed keyword untouched.
5. Trigger `Before#{cursor}After`; type `X`; confirm the result is `BeforeXAfter`. Also confirm repeated or mixed-case cursor markers remain literal and do not move outside the inserted text.
6. In one imported or manually typed snippet, include `#{clipboard}`, `#{envVar:USERNAME}`, `#{powershell:C:\test.ps1}`, `#{shortcut:Win+R}`, `#{key:enter}`, and `#{delay:500}`. Confirm every token appears literally, no clipboard content appears, no window opens, no command runs, and Enter is not pressed.
7. Trigger an emoji shortcode and a picker-selected emoji. Confirm the emoji is inserted correctly.
8. Trigger a snippet containing a real line break, tab, and control character if practical. Confirm visible `\n`, `\t`, or `\uXXXX` text appears and the target form is not submitted.
9. Hold Ctrl or Alt while triggering a combo for more than one second. Confirm Lean Beeftext reports that insertion was refused, does not erase the keyword, and no modifier remains stuck afterward.
10. Change settings, create a combo, and allow a backup. Restart and confirm all resulting application data is under `Data`; confirm no custom backup location control is offered.
11. Close the main window and confirm the tray app remains running. Then choose **Exit** from the tray menu and confirm `Beeftext.exe` terminates.
12. With a network monitor if available, launch the app and open Preferences. Confirm there is no request to the Beeftext update service.
