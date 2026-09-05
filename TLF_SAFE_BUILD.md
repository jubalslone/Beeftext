# Lean Beeftext (TLF Safe Build)

This is a deliberately restricted Beeftext build for ordinary law-firm text expansion. It is not intended to be feature-compatible with upstream Beeftext.

## Security boundary

A combo may generate text, ask the user for text, apply the allowlisted date/text transformations below, and reposition the caret within text it just inserted. A combo may not read the clipboard or environment, execute a process, synthesize arbitrary keys or shortcuts, introduce delays, or contact the upstream updater.

The variable allowlist is:

- `#{date}`, `#{time}`, and `#{dateTime}`
- custom `#{dateTime:...}` formatting and validated time shifts
- `#{combo:...}`, `#{upper:...}`, `#{lower:...}`, and `#{trim:...}`
- `#{input:...}`
- one or more exact, case-sensitive `#{cursor}` markers, with the last marker setting the final caret position

Everything else, including PowerShell, environment, clipboard, Discord clipboard, key, shortcut, delay, malformed date-shift, and unknown variables, remains visible as literal text.

## Restricted text output

Substitution does not use the clipboard or a synthetic paste chord. Deletion, UTF-16 Unicode insertion, permitted line breaks, and bounded cursor-left movement are submitted as one serial Windows input transaction while Beeftext's keyboard hook is disabled.

Preferences > Behavior provides two **Multiline snippets** modes:

- **Show line breaks as visible `\n` text** is the default. It preserves the behavior of earlier Lean Beeftext builds and prevents a snippet line break from affecting the target application.
- **Allow real line breaks** is an opt-in compatibility mode for ordinary multiline Beeftext snippets. CR, LF, and CRLF are normalized to one logical line break. Repeated and trailing line breaks are preserved.

The persisted setting key is `AllowRealLineBreaksInSnippets`. Missing or invalid stored values select the visible-`\n` default.

Upstream Beeftext normally normalizes line endings to CRLF and pastes the snippet. Its typing fallback generates an unmodified Return for each LF because Windows Unicode input does not reliably create a line break in common targets. Lean Beeftext never restores the clipboard path. In compatibility mode, only a normalized line break in the fully evaluated text may generate an unmodified Return; `#{key:enter}`, shortcut, delay, and other control syntax remain literal. Windows controls cannot distinguish that Return from a user pressing Enter, so a real line break may submit a single-line form. Enable compatibility mode only where that behavior is acceptable.

Tabs remain visible as `\t`, and other control characters as `\uXXXX`, in both modes. Valid UTF-16 surrogate pairs, including emoji, are preserved. Unpaired surrogates are made visible.

Cursor handling runs once after nested combos, input, and text transformations have finished. This matches upstream v16's useful composition rule: all exact `#{cursor}` markers are removed and the last exact marker sets the final caret position. An uppercase transformation changes the marker to literal `#{CURSOR}` and therefore cannot move the caret; lowercase and trim output is handled normally if it still contains an exact marker.

Cursor movement is enabled only when all text after the last exact marker is printable ASCII, plus normalized line breaks when compatibility mode is enabled. If that suffix is unsafe or ambiguous to navigate, every exact marker remains literal and no cursor key is sent. Each normalized line break counts as one inserted caret position, keeping every cursor-left event bounded within the newly inserted suffix. This intentionally avoids upstream's case-insensitive removal quirk and preserves Lean Beeftext's exact, case-sensitive control boundary.

Windows may reject synthetic input into a higher-integrity (for example, administrator-elevated) target because of UIPI. Lean Beeftext reports this failure; running Beeftext elevated is not recommended.

Windows 11 Notepad may corrupt rapid `KEYEVENTF_UNICODE` input while its spellcheck or autocorrect features are enabled. Disable those Notepad features when using Lean Beeftext. Microsoft Word has been verified without that adjustment.

## Portable data boundary

The packaged build contains `Portable.bin`. In portable mode, settings, combos, logs, and backups live under the package-local `Data` folder. The combo-list and custom-backup location controls are unavailable, and updater cleanup cannot delete a path from old settings.

## Updates and builds

Upstream update checks and custom PowerShell configuration are disabled. Do not install an upstream Beeftext update over this build. Review upstream changes, merge or cherry-pick them into this fork, and rebuild.

The Windows workflow checks out the exact source commit, refuses dirty tracked source, builds committed sources without rewriting them, runs the restricted parser tests, and records the source commit in `BUILD_INFO.txt`. Verify `SHA256SUMS.txt` after distribution.

Pull-request and routine QA artifacts remain unsigned. The disabled-by-default production signing design and external setup are documented in `ARTIFACT_SIGNING.md`; enabling it requires a protected GitHub Environment, reviewed Azure configuration, and an explicit flag. It has no silent unsigned fallback.

Preferences controls use their Qt style and current font metrics to establish minimum heights when the dialog opens and after relevant font, language, or style changes. This avoids clipping at Windows display or text scaling without imposing a fixed pixel height or changing the application font.

The light and dark themes use Qt palettes for their surface, text, selection, link, and disabled-state colors. Standard controls keep the platform style for checkbox/radio indicators, combo and spin controls, buttons, and keyboard focus cues. The small remaining stylesheets are limited to typography that conveys meaning and the custom frameless combo picker.

The selected application theme also sets Qt's application color-scheme hint. This keeps main-window, group, combo, dynamically created, and tray popup menus aligned with Beeftext's explicit Light or Dark choice even when it differs from the Windows theme, without replacing native menu metrics or interaction behavior.

Combo portability has one user-facing home: **Combos > Import Combos…** and **Combos > Export Combos…**. Export opens one choice dialog: **Selected combos (N)** is available when the table has a selection, and **All combos (N)** is available whenever combos exist. The menu command itself is disabled when there are no combos.

Lean exports UTF-8, human-readable JSON with a `.txt` extension and the default filename `Lean-Beeftext-Combos.txt`. Schema version 1 is:

```json
{
  "format": "lean-beeftext-combos",
  "version": 1,
  "groups": [],
  "combos": []
}
```

The `groups` and `combos` records contain the same complete group and combo fields used by the current Beeftext data model. An all-combos export includes every group, including empty groups; a selected-combos export includes each selected combo and every group it references. Lean `.txt` imports preserve those relationships. An imported group is matched to an existing group by UUID, then by exact name; otherwise the group is created from the bundle. The destination-group chooser is used only for legacy imports or a Lean bundle without group metadata.

Legacy upstream Beeftext `.json` and `.csv` combo exports remain accepted through the same Import command and keep their existing destination-group behavior. The `.txt` format is the primary file-picker choice. Malformed or unsupported files are rejected before the live combo/group data is changed.

Lean exposes no `.btbackup` backup/restore commands, automatic-backup controls, Preferences export/import UI, or JSON settings-backup path. Internal backup code and old settings may remain for storage compatibility, but they are not a parallel user portability workflow. Preferences continue to persist automatically through the normal settings store and are never included in Lean combo exports.

Help, documentation, variables, import-format, translation-help, issue, release, and project-source links for this restricted build point to `https://github.com/jubalslone/Beeftext`. Until Lean-specific documentation pages are published there, general documentation links intentionally open the repository root rather than upstream instructions that describe unavailable features. Links to upstream Beeftext remain only when explicitly labeled as attribution or legacy compatibility information.

The pre-documentation build deliberately omits **Getting Started**, because it has no Lean-specific destination distinct from **Project Documentation**. Branding remains intentionally deferred: the current menu and dialog still say **About Beeftext**. A later branding pass will change this to **About Lean Beeftext** and update the name, icon, maintained-fork wording, repository link, original-author credit, upstream attribution, and licensing/provenance details together.

## Windows QA checklist

1. Extract the artifact to a normal writable folder. Confirm `Portable.bin`, `BUILD_INFO.txt`, and `SHA256SUMS.txt` are present, then launch `Beeftext.exe` without elevation.
2. Leave **Show line breaks as visible `\n` text** selected. Trigger snippets containing LF, CRLF, repeated blank lines, and a trailing line break. Confirm every logical break appears as literal `\n` and no form is submitted.
3. Select **Allow real line breaks**, restart Beeftext, and confirm the selection persists. Trigger the same snippets in Word, Notepad with spellcheck/autocorrect disabled, an Outlook-style editor, and a browser textarea. Confirm their real line breaks, blank lines, and trailing line break.
4. In real-line-break mode, test `Before#{cursor}After`, a newline before `#{cursor}`, and a newline after it. Type `X` and confirm the caret remains within the text just inserted.
5. Test two direct cursor markers and a child combo containing one cursor referenced twice by a parent. Confirm all exact markers disappear and the caret uses the last marker. Repeat in both multiline modes. Confirm an uppercased child leaves `#{CURSOR}` literal and performs no cursor movement.
6. Test a multiline snippet containing `#{input:...}` and multiline nested combos used through `#{combo:...}`, `#{upper:...}`, `#{lower:...}`, and `#{trim:...}`. Confirm the input text and line breaks follow the selected mode.
7. In both modes, include `#{clipboard}`, `#{envVar:USERNAME}`, `#{powershell:C:\test.ps1}`, `#{shortcut:Win+R}`, `#{key:enter}`, and `#{delay:500}`. Confirm every token appears literally, no clipboard content appears, no window opens, no command runs, and no programmed control action occurs.
8. Regression-test `leanv` → `Lean Beeftext is alive.`, `#{date}`, `#{time}`, `#{dateTime}`, `#{input:Name}`, and emoji insertion.
9. In both modes, test a tab and another control character. Confirm visible `\t` or `\uXXXX` text appears.
10. Hold Ctrl or Alt while triggering a combo for more than one second. Confirm insertion is refused, the keyword remains intact, and no modifier remains stuck.
11. Restart after changing the multiline preference and confirm `Data/Settings.ini` contains the persisted setting and all application data remains under `Data`.
12. At Windows display scaling 100%, 125%, and 150%, open every Preferences pane in both the light and dark themes. Check every checkbox, radio button, combo box, shortcut field, spin box, and button—especially Combos defaults/manual shortcuts and the Advanced delay control—for complete text, unmistakable checked state, distinct enabled/disabled state, clear selection, and visible keyboard focus. Confirm there are no backup/restore or Preferences Export/Import controls. If Windows text size is separately enlarged, repeat at that setting.
13. With Windows Dark, select Beeftext Light; with Windows Light, select Beeftext Dark. In each combination, inspect File, Groups, Combos, Advanced, Help, dynamic/context, and tray menus. Confirm the Beeftext theme wins and normal, hovered/selected, disabled, separator, shortcut, and keyboard-focus states remain readable and distinct.
14. Confirm File and Preferences contain no backup, restore, or settings-export commands. Confirm the Combos portability section contains one **Import Combos…** and one **Export Combos…** command, with no separate selected/all export actions. With no combos, confirm Export is disabled. With one and then several selected combos, confirm the export dialog shows the correct **Selected combos (N)** count and also offers **All combos (N)**. Export each scope, confirm the filename defaults to `Lean-Beeftext-Combos.txt`, and confirm the `.txt` file is readable JSON containing only `format`, `version`, `groups`, and `combos` at its root.
15. Import a Lean `.txt` export and confirm combo fields, group relationships, and empty groups round-trip. Confirm existing groups are reused by UUID or exact name and new groups are created. Import representative upstream `.json` and `.csv` exports into a chosen group. Try malformed `.txt`, unsupported, and `.btbackup` files and confirm each fails clearly without partially importing data.
16. Open Project Documentation, Release Notes, Report Bug, every About-dialog link, About Variables link, Supported file formats link, and Other languages link. Confirm Lean project/help links use `jubalslone/Beeftext`; confirm every retained upstream link is visibly labeled as upstream attribution or legacy Beeftext v7.2. Confirm Help has no Getting Started item and About remains intentionally unbranded for this pass.
17. Close the main window and confirm the tray app remains running. Then choose **Exit** and confirm `Beeftext.exe` terminates.
18. With a network monitor if available, launch the app and open Preferences. Confirm there is no request to the Beeftext update service.
