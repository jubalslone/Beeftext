# Lean Beeftext Security Model

This document records the restricted execution model, implementation boundary, automated checks, and manual QA expectations for Lean Beeftext 1.0.0. It does not claim certification or absolute security.

## Allowed Variable Boundary

A combo may produce ordinary text, prompt the user for text, evaluate the allowlisted date/text transformations below, and reposition the caret within text it just inserted.

The allowlist is:

- `#{date}`, `#{time}`, and `#{dateTime}`
- Custom `#{dateTime:...}` formatting and syntactically valid date/time offsets
- `#{combo:...}`, `#{upper:...}`, `#{lower:...}`, and `#{trim:...}`
- `#{input:...}`
- One or more exact, case-sensitive `#{cursor}` markers, with the last marker determining the final caret position

Everything else—including PowerShell, environment-variable, clipboard, Discord clipboard, key, shortcut, delay, malformed date/time offset, and unknown variables—remains visible as literal text.

## Restricted Text Output

Substitution does not use the clipboard or a synthetic paste shortcut. Deletion, UTF-16 Unicode insertion, permitted text line breaks, and bounded cursor-left movement are submitted as one serial Windows input transaction while Lean Beeftext's keyboard hook is disabled.

Tabs become visible `\t` text. Other control characters become visible `\uXXXX` text. Valid UTF-16 surrogate pairs, including emoji, are preserved; unpaired surrogates become visible code-unit text.

### Multiline behavior

Preferences > Behavior provides two **Multiline snippets** modes:

- **Show line breaks as visible `\n` text** is the default. It prevents a snippet line break from affecting the target application.
- **Allow real line breaks** is an opt-in compatibility mode. CR, LF, and CRLF normalize to one logical line break; repeated and trailing breaks are preserved.

The persisted key is `AllowRealLineBreaksInSnippets`. Missing or invalid values select the visible-`\n` default.

Upstream Beeftext 16 normally normalizes line endings to CRLF and pastes the snippet. Its typing fallback generates an unmodified Return for each LF because Windows Unicode input does not reliably create a line break in common targets. Lean Beeftext never restores that clipboard path. In compatibility mode, only a normalized line break present in fully evaluated text may generate an unmodified Return. `#{key:enter}`, shortcut, delay, and other control syntax stay literal.

Windows controls cannot distinguish the resulting Return from a user pressing Enter. A real line break can therefore submit a chat or single-line form. This is why real line breaks are opt-in and described as destination-sensitive text formatting rather than a programmable key feature.

### Cursor handling

Cursor processing happens once, after nested combos, input, and transformations. All exact `#{cursor}` markers are removed and the last exact marker sets the final caret position. An uppercase transformation changes the marker to literal `#{CURSOR}` and cannot move the caret; lowercase and trim output works normally if an exact marker remains.

Movement is enabled only when text after the last exact marker is printable ASCII, plus normalized line breaks in compatibility mode. If the suffix is unsafe or ambiguous, every exact marker stays literal and no cursor key is sent. Each normalized line break counts as one inserted position. Cursor-left events are therefore bounded to the newly inserted suffix.

### Modifier and hook safety

Insertion refuses to proceed while Ctrl, Alt, Shift, or Windows modifiers are held. The original keyword remains intact. A timeout prevents indefinite waiting, and error paths restore the keyboard hook. Restricted tests cover refusal, event planning, modifier release behavior, and hook restoration guards; manual QA checks for stuck modifiers.

## Installed and Portable Data Isolation

Installed Lean Beeftext stores `Settings.ini`, `comboList.json`, user configuration/translations, backups, and migration recovery copies beneath the Windows Documents known folder in `Lean Beeftext`. It uses a permanent Lean-only settings identity and does not use upstream Beeftext's normal runtime namespace. Logs and last-use caches use Lean-specific LocalAppData. Live combo-list persistence uses atomic file replacement to reduce the risk of a truncated file during interruption or synchronized-folder activity.

The portable package includes `Portable.bin`. Portable settings, combos, logs, and other application data live under package-local `Data`. The combo-list and custom-backup location controls are unavailable, updater cleanup cannot delete a path from old settings, and combo portability dialogs default to normal user-selected locations.

The public portability model is **Combos > Import Combos…** and **Combos > Export Combos…** only. Lean exports UTF-8, human-readable JSON in `Lean-Beeftext-Combos.txt` using this versioned root schema:

```json
{
  "format": "lean-beeftext-combos",
  "version": 1,
  "groups": [],
  "combos": []
}
```

The records retain the complete group/combo data used by the current model. An all-combos export includes every group, including empty groups. A selected-combos export includes selected combos and their referenced groups. Lean imports match groups by UUID, then exact name, and otherwise create them. A selected destination group is used for legacy imports or a Lean bundle without group metadata.

Legacy upstream Beeftext `.json` and `.csv` exports remain import-compatible. Malformed or unsupported inputs are validated before the live group/combo data changes. Lean exposes no `.btbackup` command, automatic-backup control, Preferences import/export UI, or settings-backup path. Preferences are never included in combo exports.

## Update Boundary

The current restricted build disables automatic and manual upstream update checks, download/install code paths, updater cleanup, and custom PowerShell configuration. The Inno installer provides no network updater, service, scheduled task, or privileged helper. Its unattended switches are only a future installer execution contract and do not bypass UAC or verify downloads. Upstream changes must be reviewed and deliberately integrated into this fork.

## Upstream Migration Boundary

First-run migration runs as the signed-in user and only in installed mode before a Lean combo library exists. It reads the upstream `beeftext.org` / `Beeftext` preferences only as detection clues and imports no upstream settings. A registered installed import candidate must have consistent upstream display, publisher, install-location, and `Beeftext.exe` evidence. Lean does not automatically uninstall an installed upstream application; it recommends manual uninstall after a verified import. A portable candidate must have `Beeftext.exe`, the expected portable beacon, the expected data layout, and readable combo data; a filename alone is insufficient.

Migration snapshots only the legacy combo JSON, parses it through the existing validated model (including safe plain-text conversion), writes the Lean copy atomically, reloads it into a fresh model, and compares normalized content and group/combo counts. Exact running-source closure is consented, graceful, and followed by source/digest refresh. Safe portable cleanup is gated on every validation step, uses the Recycle Bin, and revalidates the fingerprint/content digest; it refuses shared roots such as Desktop, Downloads, Documents, the user profile, drive roots, Program Files, Windows, and the active application directory. Persisted portable-cleanup state prevents a cleanup retry from importing duplicate combos. Installed upstream files and legacy user data are left untouched.

## Known Windows Boundaries

Windows may reject synthetic input into a higher-integrity administrator-elevated target because of UIPI. Lean Beeftext reports insertion failure. Running the application elevated is not recommended.

Windows 11 Notepad may corrupt rapid `KEYEVENTF_UNICODE` input when spellcheck or autocorrect is enabled. Disable those Notepad features if this occurs. This issue has not appeared in Microsoft Office testing performed for this fork.

## Build, Provenance, and Reproducibility

The Windows workflow checks out the exact requested commit and recursive submodules, verifies tracked source is clean before and after the build, builds Release sources, runs the complete restricted regression suite, stages installed and portable payloads through one script, records the source commit in `BUILD_INFO.txt`, and creates `SHA256SUMS.txt` for every packaged file. It pins and hash-verifies the Inno compiler, rejects portable beacons from installed staging, and smoke-tests install/reinstall/uninstall while verifying a synthetic Documents fixture survives.

Routine pull-request QA artifacts are unsigned. The separately gated production signing design is documented in `ARTIFACT_SIGNING.md`; it requires reviewed external Azure configuration and has no silent unsigned fallback.

## Windows QA Checklist

1. Test both distributions. Install with UAC and confirm the Program Files payload, Start Menu shortcut, optional unchecked desktop shortcut, and normal-user launch. Confirm `<Documents>\Lean Beeftext` is created and uninstall preserves it. Extract portable to a writable folder and confirm `Portable.bin`, `BUILD_INFO.txt`, `SHA256SUMS.txt`, `LICENSE`, `README.md`, `SECURITY_MODEL.md`, and `THIRD_PARTY_NOTICES.md` are present.
2. On fresh settings, confirm the application follows Windows Light and Windows Dark. Confirm explicit Lean Light while Windows is Dark and explicit Lean Dark while Windows is Light. Restart and confirm each saved override persists.
3. Confirm the Release menu order is **File | Combos | Groups | Help**, with no top-level Advanced menu. Confirm **Generate Cheat Sheet…** is at the bottom of Combos and **Open Diagnostic Log** is in Help.
4. Test every Preferences pane at 100%, 125%, and 150% display scaling in system, Light, and Dark modes. Confirm native controls, focus, disabled states, and layout remain clear.
5. In visible-`\n` mode, trigger LF, CRLF, repeated blank lines, and a trailing newline. Confirm visible `\n` output and no target action.
6. In real-line-break mode, repeat in Word, Notepad with spellcheck/autocorrect disabled, an Outlook-style editor, a browser textarea, and a single-line/chat test field. Confirm the documented destination behavior.
7. Test newlines before and after `#{cursor}`, repeated markers, a nested-combo marker, input, upper/lower/trim transformations, ASCII suffixes, Unicode suffix refusal, and emoji. Confirm movement remains within inserted text.
8. In both multiline modes, include `#{clipboard}`, `#{envVar:USERNAME}`, `#{powershell:C:\test.ps1}`, `#{shortcut:Win+R}`, `#{key:enter}`, and `#{delay:500}`. Confirm every token stays literal and causes no associated action or disclosure.
9. Hold Ctrl or Alt while triggering a combo. Confirm substitution is refused, the keyword remains, and no modifier becomes stuck.
10. Export one combo, several selected combos, and all combos. Confirm the selection counts, default `.txt` filename, readable versioned JSON, group preservation, and absence of settings. Import the Lean bundle and representative upstream `.json`/`.csv` files. Confirm malformed and unsupported inputs do not partially import.
11. Confirm there are no backup/restore or Preferences import/export commands. Verify portable settings and combo data remain under local `Data`.
12. Open every Help and About link. Confirm Lean-facing links target `jubalslone/lean-beeftext`, the Variables link ends in `#variables`, and upstream links are visibly labeled as upstream attribution.
13. Confirm Project Documentation, Release Notes, Report Bug, Open Diagnostic Log, and About Lean Beeftext work. Inspect About product/version, portable status, maintainer, upstream author, translator credit, license/notices links, AI wording, and build provenance.
14. Exercise installed first-run migration with synthetic/dispensable upstream installed and portable copies. Confirm differing libraries require a choice, a failed import leaves both sources untouched, a successful installed import recommends manual uninstall without launching it, portable removal goes to the Recycle Bin, shared-root removal is refused, and the full migration does not repeat on second launch.
15. With a network monitor if available, launch the application and open Preferences. Confirm there is no update-service request.

## Review Limitations

Automated tests validate the parser, event-plan boundaries, import/export helpers, source-level product configuration, and packaging rules. They cannot prove how every Windows application interprets injected Unicode or Return events. Manual target-application QA remains required for each release candidate and each operational environment.
