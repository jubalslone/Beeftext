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

Uninstall removes the Program Files payload, shortcuts, and uninstall registration. Its confirmation and completion messages explicitly say that user data is kept. It intentionally does not delete `<Documents>\Lean Beeftext`, so reinstalling does not lose combos, settings, or migration recovery copies.

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

Those switches do not bypass UAC. Same-version reinstall is permitted; downgrade is refused. The stable AppId enables in-place upgrades without uninstalling first. Inno Restart Manager support requests a normal application close for an installed Lean instance and is configured not to force-close or unexpectedly relaunch Lean Beeftext. XMiLib implements Lean's single-instance key with `QSharedMemory`, not a compatible Windows named mutex, so Inno `AppMutex` is not used. Lean's uninstaller instead queries the built-in Windows process inventory and compares the full running executable path to the exact installed `LeanBeeftext.exe`; interactive uninstall offers Retry/Cancel until that process exits, while silent uninstall aborts deterministically if the exact process is running or its state cannot be checked safely. Setup cannot reliably discover an arbitrary portable executable path before staging, so users should close portable Lean Beeftext before installing; no process helper, service, forced termination, or third-party plugin is added for either case.

No network updater is implemented. A future updater is expected to download and cryptographically verify this same installer, obtain user approval, exit Lean Beeftext, run Setup, and relaunch Lean Beeftext.

## First-run compatible migration safety

Migration runs in the application as the signed-in user, only for installed Lean Beeftext, and only before a Lean combo library exists. It detects registered upstream Beeftext installations and strongly fingerprinted upstream or Lean portable copies through matching shortcuts, existing upstream-process evidence, or a shallow Desktop/Downloads search. The shallow scan checks only each root and its immediate child directories.

A normal Lean portable fingerprint requires `LeanBeeftext.exe`, `Portable.bin`, and a readable, non-empty `Data\comboList.json`. The existing `PortableApps.bin` and `Data\settings\comboList.json` layout is also recognized. The containing folder name is irrelevant to detection, while a lone executable or incomplete layout is insufficient.

Only combos and groups are imported. Upstream and portable Lean preferences are not adopted. Differing legacy libraries are presented as separate choices; byte-identical libraries may be grouped. Source labels distinguish `Portable Beeftext` from `Portable Lean Beeftext`.

The sequence is recovery snapshot, parse, safe conversion, atomic persistence, fresh reload, and normalized content/count comparison. Portable cleanup is unavailable until all steps succeed. If the exact selected upstream executable is running, Lean offers to request a graceful close through Windows Restart Manager and continues the same migration only after that exact process exits; it never force-terminates a source process.

After a selected running source closes gracefully, Lean revalidates its exact executable, installation/portable identity, combo-file path, and current SHA-256 before taking the recovery snapshot. The snapshot and import are made from one checked byte sequence, and the refreshed digest is recorded for any portable cleanup. A later source change therefore cannot authorize removal.

Lean does not automatically uninstall an installed upstream Beeftext application. After a successful installed-source import it explains that running both text expanders may cause duplicate or conflicting expansions, recommends manual uninstall, and can open Windows Installed Apps directly. Pre-release QA state that requested installed cleanup is retired without invoking an uninstaller or importing again. Legacy upstream user data under `%LOCALAPPDATA%\beeftext.org\Beeftext` is intentionally preserved as rollback/recovery material.

Portable cleanup uses the Recycle Bin, revalidates the fingerprint and content digest, and refuses Desktop, Downloads, Documents, the user profile, drive roots, Program Files, Windows, the active application directory, and any folder not clearly dedicated to Beeftext. Cleanup state is persisted before cleanup begins, so retrying cleanup cannot duplicate imported combos.
