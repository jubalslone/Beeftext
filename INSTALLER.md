# Lean Beeftext installer and storage

Lean Beeftext has two distributions: installed and portable. There is one installed product, not separate consumer, managed, or updater variants.

## Permanent identities

- Application/settings name: `Lean Beeftext`
- Organization name: `Jubal Slone`
- Single-instance identifier: `LeanBeeftextSingleInstanceIdentifier`
- Inno Setup AppId: `{499E5EE9-ECC6-455E-B78A-EDF581715A80}`

These values are compatibility identifiers and must not change with the product version. The legacy `beeftext.org` / `Beeftext` settings identity is read only by the first-run migration detector as evidence about an upstream installation.

## Installed distribution

The Inno Setup installer installs the application for the machine at:

```text
%ProgramFiles%\Lean Beeftext\
```

The normal installer requires elevation, creates a Start Menu shortcut, offers an unchecked desktop-shortcut task, and can launch Lean Beeftext as the original user after an interactive installation. It does not configure file associations, `PATH`, launch at login, an updater, a service, or a scheduled task.

Meaningful, restorable user data is stored beneath the Windows Documents known folder:

```text
<Documents>\Lean Beeftext\Settings.ini
<Documents>\Lean Beeftext\comboList.json
<Documents>\Lean Beeftext\Translations\
<Documents>\Lean Beeftext\Backups\
<Documents>\Lean Beeftext\Migration Backups\
```

`QStandardPaths::DocumentsLocation` resolves the known folder. If Windows/OneDrive Known Folder Move redirects Documents, Lean Beeftext naturally uses the redirected location; Lean Beeftext does not configure OneDrive.

Machine-local diagnostic and recency data is stored beneath the Windows LocalAppData known folder:

```text
%LOCALAPPDATA%\Lean Beeftext\log.txt
%LOCALAPPDATA%\Lean Beeftext\comboLastUse.json
%LOCALAPPDATA%\Lean Beeftext\emojiLastUse.json
```

The publisher name is intentionally not part of this filesystem path. Lean Beeftext resolves the Windows `FOLDERID_LocalAppData` known folder and appends exactly `Lean Beeftext`.

Uninstall removes the Program Files payload, shortcuts, and uninstall registration. It intentionally does not delete `<Documents>\Lean Beeftext`, so reinstalling does not lose combos, settings, or migration recovery copies.

## Portable distribution

Portable mode retains its established beacon and local storage contract:

```text
LeanBeeftext.exe
Portable.bin
Data\Settings.ini
Data\comboList.json
Data\Backup\
```

The PortableApps beacon/layout remains supported as before. Portable mode is self-contained and never runs installed first-run migration. The installed staging directory is rejected if it contains `Portable.bin` or `PortableApps.bin`.

## Build and unattended contract

`Installer/StagePayload.ps1` stages either distribution from the same compiled executable and creates `BUILD_INFO.txt` plus `SHA256SUMS.txt`. `Installer/LeanBeeftext.iss` consumes only `_staging\installed`.

Inno Setup 7.1.0 is the pinned compiler for CI. A future already-elevated updater or administrative process may invoke the same installer with:

```text
Lean-Beeftext-Setup-1.0.0.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
```

Those switches do not bypass UAC. Same-version reinstall is permitted; downgrade is refused. The stable AppId enables in-place upgrades without uninstalling first. Inno Restart Manager support requests a normal application close and is configured not to force-close or unexpectedly relaunch Lean Beeftext.

No network updater is implemented. A future updater is expected to download and cryptographically verify this same installer, obtain user approval, exit Lean Beeftext, run Setup, and relaunch Lean Beeftext.

## First-run upstream migration safety

Migration runs in the application as the signed-in user, only for installed Lean Beeftext, and only before a Lean combo library exists. It detects registered upstream Beeftext installations and strongly fingerprinted portable copies in running-process/shortcut locations or a shallow Desktop/Downloads search. An arbitrary file named `Beeftext.exe` is insufficient.

Only combos and groups are imported. Upstream preferences are not adopted. Differing legacy libraries are presented as separate choices; byte-identical libraries may be grouped.

The sequence is recovery snapshot, parse, safe conversion, atomic persistence, fresh reload, and normalized content/count comparison. Cleanup is unavailable until all steps succeed. Installed cleanup prefers a registered quiet uninstaller only when its executable is inside the verified upstream installation root; otherwise it uses an equally verified registered normal uninstaller, or declines automatic removal. Portable cleanup uses the Recycle Bin, revalidates the fingerprint and content digest, and refuses Desktop, Downloads, Documents, the user profile, drive roots, Program Files, Windows, the active application directory, and any folder not clearly dedicated to Beeftext. Cleanup state is persisted before cleanup begins, so retrying cleanup cannot duplicate imported combos.
