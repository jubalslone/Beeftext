# Lean Beeftext

Lean Beeftext is a privacy-focused, security-conscious text expander for Windows, based on Beeftext 16.0. It keeps Beeftext's fast local combo workflow while deliberately avoiding features that read sensitive system data, execute scripts, or automate arbitrary keyboard actions.

Type a short keyword and Lean Beeftext expands it into the text you use every day: signatures, addresses, boilerplate, dates, form language, and other frequently repeated text.

Lean Beeftext is built on a project we genuinely like. Beeftext provides a fast, practical local text-expansion workflow; Lean Beeftext adapts that foundation for environments where privacy, predictability, and a narrower execution surface matter more than extensibility.

## Why Lean Beeftext?

[Beeftext](https://github.com/xmichelo/Beeftext) is a capable open-source text expander. Lean Beeftext keeps its practical local workflow while choosing a deliberately narrower execution model for privacy-sensitive workflows.

Lean Beeftext supports:

- Local text expansion
- Groups, in-window combo search, and Quick Search
- Dates and times
- Input prompts
- Nested combos
- Uppercase, lowercase, and trim transformations
- Caret repositioning with `#{cursor}`
- Optional real multiline snippets
- Emoji shortcodes
- Combo import and export
- Portable mode

Malformed, unknown, or deliberately blocked variable syntax remains visible as literal text instead of executing.

## Creating a Combo

Open Lean Beeftext, choose **Combos > New**, then enter a keyword and snippet. For example, the keyword `myaddress` could expand to:

```text
123 Example Street
Des Moines, IA 50309
```

Save the combo, then type its keyword in a normal text field. Triggering can be automatic or manual, depending on your Preferences.

**Search** (`Ctrl+F`) filters and manages combos in the main window. **Quick Search** uses its separate global shortcut to open a small searchable overlay while you work in another application, then inserts the combo you choose.

## Variables

Lean Beeftext supports the following variables:

- `#{date}` — the current local date.
- `#{time}` — the current local time.
- `#{dateTime}` — the current local date and time.
- `#{dateTime:FORMAT}` — a custom date/time format, such as `#{dateTime:yyyy-MM-dd}`.
- `#{dateTime:OFFSET:FORMAT}` — a custom format with a date/time offset, such as `#{dateTime:+1d:yyyy-MM-dd}` or `#{dateTime:+1w-2d:yyyy-MM-dd}`.
- `#{combo:keyword}` — the snippet belonging to another combo.
- `#{upper:keyword}` — another combo's snippet converted to uppercase.
- `#{lower:keyword}` — another combo's snippet converted to lowercase.
- `#{trim:keyword}` — another combo's snippet with leading and trailing whitespace removed.
- `#{input:Name}` — asks for text at expansion time using the prompt label `Name`.
- `#{cursor}` — sets the caret position after expansion.

Date/time formats use Qt date/time pattern letters. Date/time offsets may combine signed units:

- `y` = years
- `M` = months
- `w` = weeks
- `d` = days
- `h` = hours
- `m` = minutes
- `s` = seconds
- `z` = milliseconds

Malformed, unknown, and blocked variables remain visible as literal text. Lean Beeftext does not allow combo variables to read the clipboard, read environment variables, execute PowerShell, generate arbitrary key events, generate arbitrary keyboard shortcuts, or introduce programmed delays. For example, `#{clipboard}`, `#{envVar:USERNAME}`, `#{powershell:C:\test.ps1}`, `#{key:enter}`, `#{shortcut:Win+R}`, and `#{delay:500}` stay literal.

## Cursor Placement

An exact, case-sensitive `#{cursor}` marker is removed after nested combos, input, and text transformations have finished. If a snippet contains more than one exact marker, all exact markers are removed and the last one determines the final caret position.

Caret movement is allowed only when the text after the final marker can be counted predictably and the movement stays within the newly inserted text. If that suffix is ambiguous—for example, because it contains non-ASCII text—the markers remain literal and no caret movement is sent. Transforming a marker to another case, such as `#{CURSOR}`, makes it ordinary literal text.

## Multiline Snippets

Preferences > Behavior offers two modes:

- **Show line breaks as visible `\n` text** is the default. CR, LF, and CRLF line endings display as `\n` and do not cause a line break in the destination.
- **Allow real line breaks** is opt-in. Ordinary snippet text, safe evaluated text, input results, and nested-combo results may produce real line breaks.

Real line breaks can act like Enter in the destination application. In chat boxes, forms, and single-line fields, that can submit content or trigger another action. Use the visible-`\n` mode wherever that behavior is not acceptable.

## Import and Export

Combo portability has one home:

- **Combos > Import Combos…**
- **Combos > Export Combos…**

Export asks whether to include the currently selected combos or all combos. Lean Beeftext exports a self-identifying, versioned bundle as human-readable UTF-8 JSON stored in a `.txt` file. The default filename is `Lean-Beeftext-Combos.txt`; groups and all combo fields needed for a faithful round trip are included, but Preferences are not.

Import accepts:

- Lean Beeftext `.txt` JSON bundles
- Legacy upstream Beeftext `.json` exports
- Legacy upstream Beeftext `.csv` exports

The text extension makes Lean bundles easy to inspect and often easier to share. There is no separate user-facing combo backup/restore workflow or Preferences export/import workflow.

## Compatibility Notes

### Windows 11 Notepad

Windows 11 Notepad can corrupt rapid Unicode insertion while spellcheck or autocorrect is enabled. Disable those Notepad features when using Lean Beeftext if this occurs. This issue has not appeared in our Microsoft Office testing.

### Elevated Applications

Windows may prevent a normal desktop application from inserting text into a higher-integrity, administrator-elevated application. This is a Windows UIPI boundary. Run Lean Beeftext and the destination at the same normal integrity level; running Lean Beeftext elevated is not recommended.

## Themes

Lean Beeftext follows the Windows light or dark appearance setting. If you change the Windows appearance setting while Lean Beeftext is already running, restart Lean Beeftext to apply the change consistently.

## Install or Use Portable

The normal Windows installer installs Lean Beeftext under `Program Files\Lean Beeftext` and creates a Start Menu shortcut. A desktop shortcut is optional and unchecked by default.

Installed Lean Beeftext stores restorable user data in `Lean Beeftext` beneath the Windows Documents known folder, including `Settings.ini`, `comboList.json`, user translations/configuration, and migration recovery copies. If OneDrive Known Folder Move redirects Documents, that folder naturally follows the redirected Documents location; Lean Beeftext does not configure OneDrive. Diagnostic logs and last-use caches remain in Lean-specific LocalAppData to avoid unnecessary document-sync churn.

On the first installed launch, Lean Beeftext can detect a registered upstream Beeftext installation or a strongly identified portable copy, import only combos and groups, and verify the saved result. If the exact selected upstream copy is running, Lean can request a graceful close and continue the same migration; it never force-terminates the source. After an installed-source import, Lean recommends manually uninstalling upstream Beeftext because running both text expanders can cause duplicate or conflicting expansions; Lean does not invoke the upstream uninstaller. Safe cleanup of a strongly identified portable copy remains a separate opt-in action and occurs only after a recovery snapshot, parse, atomic save, fresh reload, and content validation all succeed. Differing libraries are never silently merged, and legacy upstream AppData is preserved for recovery.

Uninstall removes the application and its shortcuts but preserves the entire Documents data folder, so uninstalling and reinstalling does not discard user data.

The portable package remains available. It contains `Portable.bin` and keeps settings, combos, logs, and other application data under its local `Data` folder. Keep that package in a writable location and move the folder together with the executable. Portable Lean Beeftext never uses installed Documents storage and never runs the installed migration assistant.

See [INSTALLER.md](INSTALLER.md) for exact storage paths, installer identity, migration safeguards, and the unattended installation contract.

## Security Model

Lean Beeftext intentionally limits combo evaluation and text insertion. It does not restore clipboard insertion, PowerShell execution, environment-variable reads, arbitrary key/shortcut/delay variables, Discord clipboard behavior, or upstream updater activity. Modifiers held during a substitution cause the operation to fail closed, and keyboard-hook restoration is covered by regression tests.

This is a design boundary, not a claim of formal certification or absolute security. See [SECURITY_MODEL.md](SECURITY_MODEL.md) for technical details and QA expectations.

## Project Status

Lean Beeftext 1.0.0 is based on Beeftext 16.0. Functional Windows QA covers the restricted substitution model, multiline modes, combo portability, portable isolation, and the current Windows user interface. The repository now builds both a normal Inno Setup installer and a portable package from the same tested commit. Network updater implementation and release signing remain future hardening work.

## Upstream Project

Lean Beeftext is an unofficial fork of [Beeftext by Xavier Michelon](https://github.com/xmichelo/Beeftext), maintained by Jubal Slone. It exists because Beeftext is a strong, practical open-source text expander, and this fork explores a narrower, privacy-focused direction built on that foundation.

Upstream Beeftext translations and other retained project assets remain credited to their upstream authors and contributors.

## AI-Assisted Development

Development of the Lean Beeftext fork has been assisted by OpenAI coding and language tools. Design decisions, review, testing, and release responsibility remain with the project maintainer.

## License

Lean Beeftext remains available under the [MIT License](LICENSE), preserving Xavier Michelon's upstream copyright and license notice and identifying the fork maintainer's modifications.

The application also distributes components under their own licenses, including Qt under LGPLv3 and the MIT-licensed XMiLib and emojilib projects. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for attribution, component status, license links, and packaging notes.

## Building

The supported CI build uses:

- Windows and Visual Studio 2022
- CMake
- Qt 6.8 LTS for 64-bit MSVC 2022
- Inno Setup 7.1.0 for the installed distribution
- The recursively checked-out XMiLib and emojilib submodules

From a Visual Studio 2022 developer environment with Qt available to CMake:

```powershell
git submodule update --init --recursive
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The GitHub Actions Windows workflow performs the same configure, Release build, complete test run, clean-source checks, shared installed/portable staging, provenance recording, and packaged-file checksum generation. It compiles the installer, checks the installed payload cannot activate portable mode, and exercises silent install, same-version reinstall, and data-preserving uninstall. It produces both a portable QA artifact and `Lean-Beeftext-Setup-1.0.0.exe` from the exact same source commit.

Project links: [repository](https://github.com/jubalslone/lean-beeftext), [issues](https://github.com/jubalslone/lean-beeftext/issues), and [releases](https://github.com/jubalslone/lean-beeftext/releases).
