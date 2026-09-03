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

Substitution does not use the clipboard or a synthetic paste chord. Deletion, UTF-16 Unicode insertion, permitted line breaks, and bounded cursor-left movement are submitted as one serial Windows input transaction while Beeftext's keyboard hook is disabled.

Preferences > Behavior provides two **Multiline snippets** modes:

- **Show line breaks as visible `\n` text** is the default. It preserves the behavior of earlier Lean Beeftext builds and prevents a snippet line break from affecting the target application.
- **Allow real line breaks** is an opt-in compatibility mode for ordinary multiline Beeftext snippets. CR, LF, and CRLF are normalized to one logical line break. Repeated and trailing line breaks are preserved.

The persisted setting key is `AllowRealLineBreaksInSnippets`. Missing or invalid values—including preference files exported by earlier Lean builds—select the visible-`\n` default.

Upstream Beeftext normally normalizes line endings to CRLF and pastes the snippet. Its typing fallback generates an unmodified Return for each LF because Windows Unicode input does not reliably create a line break in common targets. Lean Beeftext never restores the clipboard path. In compatibility mode, only a normalized line break in the fully evaluated text may generate an unmodified Return; `#{key:enter}`, shortcut, delay, and other control syntax remain literal. Windows controls cannot distinguish that Return from a user pressing Enter, so a real line break may submit a single-line form. Enable compatibility mode only where that behavior is acceptable.

Tabs remain visible as `\t`, and other control characters as `\uXXXX`, in both modes. Valid UTF-16 surrogate pairs, including emoji, are preserved. Unpaired surrogates are made visible.

Cursor movement is enabled only when there is exactly one exact `#{cursor}` marker and all text after it is printable ASCII, plus normalized line breaks when compatibility mode is enabled. Otherwise the marker remains literal and no cursor key is sent. Each normalized line break counts as one inserted caret position, keeping every cursor-left event bounded within the newly inserted suffix.

Windows may reject synthetic input into a higher-integrity (for example, administrator-elevated) target because of UIPI. Lean Beeftext reports this failure; running Beeftext elevated is not recommended.

Windows 11 Notepad may corrupt rapid `KEYEVENTF_UNICODE` input while its spellcheck or autocorrect features are enabled. Disable those Notepad features when using Lean Beeftext. Microsoft Word has been verified without that adjustment.

## Portable data boundary

The packaged build contains `Portable.bin`. In portable mode, settings, combos, logs, and backups live under the package-local `Data` folder. The combo-list and custom-backup location controls are unavailable, imported preferences cannot redirect either path, and updater cleanup cannot delete a path from old settings.

## Updates and builds

Upstream update checks and custom PowerShell configuration are disabled. Do not install an upstream Beeftext update over this build. Review upstream changes, merge or cherry-pick them into this fork, and rebuild.

The Windows workflow checks out the exact source commit, refuses dirty tracked source, builds committed sources without rewriting them, runs the restricted parser tests, and records the source commit in `BUILD_INFO.txt`. Verify `SHA256SUMS.txt` after distribution.

## Windows QA checklist

1. Extract the artifact to a normal writable folder. Confirm `Portable.bin`, `BUILD_INFO.txt`, and `SHA256SUMS.txt` are present, then launch `Beeftext.exe` without elevation.
2. Leave **Show line breaks as visible `\n` text** selected. Trigger snippets containing LF, CRLF, repeated blank lines, and a trailing line break. Confirm every logical break appears as literal `\n` and no form is submitted.
3. Select **Allow real line breaks**, restart Beeftext, and confirm the selection persists. Trigger the same snippets in Word, Notepad with spellcheck/autocorrect disabled, an Outlook-style editor, and a browser textarea. Confirm their real line breaks, blank lines, and trailing line break.
4. In real-line-break mode, test `Before#{cursor}After`, a newline before `#{cursor}`, and a newline after it. Type `X` and confirm the caret remains within the text just inserted.
5. Test a multiline snippet containing `#{input:...}` and multiline nested combos used through `#{combo:...}`, `#{upper:...}`, `#{lower:...}`, and `#{trim:...}`. Confirm the input text and line breaks follow the selected mode.
6. In both modes, include `#{clipboard}`, `#{envVar:USERNAME}`, `#{powershell:C:\test.ps1}`, `#{shortcut:Win+R}`, `#{key:enter}`, and `#{delay:500}`. Confirm every token appears literally, no clipboard content appears, no window opens, no command runs, and no programmed control action occurs.
7. Regression-test `leanv` → `Lean Beeftext is alive.`, `#{date}`, `#{time}`, `#{dateTime}`, `#{input:Name}`, and emoji insertion.
8. In both modes, test a tab and another control character. Confirm visible `\t` or `\uXXXX` text appears.
9. Hold Ctrl or Alt while triggering a combo for more than one second. Confirm insertion is refused, the keyword remains intact, and no modifier remains stuck.
10. Restart after changing the multiline preference and confirm `Data/Settings.ini` contains the persisted setting and all application data remains under `Data`.
11. Close the main window and confirm the tray app remains running. Then choose **Exit** and confirm `Beeftext.exe` terminates.
12. With a network monitor if available, launch the app and open Preferences. Confirm there is no request to the Beeftext update service.
