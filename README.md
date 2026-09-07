# Lean Beeftext

Lean Beeftext is a privacy-focused, security-conscious text expander for Windows, based on Beeftext 16.0. It keeps Beeftext's fast local combo workflow while deliberately avoiding features that read sensitive system data, execute scripts, or automate arbitrary keyboard actions.

Type a short keyword and Lean Beeftext expands it into the text you use every day: signatures, addresses, boilerplate, dates, form language, and other frequently repeated text.

Lean Beeftext is built on a project we genuinely like. Beeftext provides a fast, practical local text-expansion workflow; Lean Beeftext adapts that foundation for environments where privacy, predictability, and a narrower execution surface matter more than extensibility.

## Why Lean Beeftext?

[Beeftext](https://github.com/xmichelo/Beeftext) is a capable open-source text expander. Lean Beeftext keeps its practical local workflow while choosing a deliberately narrower execution model for privacy-sensitive workflows.

Lean Beeftext supports:

- Local text expansion
- Groups, combo search, and the combo picker
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

## Portable Edition

The portable package contains `Portable.bin`. It stores settings, combos, logs, and other application data under its local `Data` folder, so keep the package in a writable location and move that folder together with the executable.

Portable mode is currently the available QA/distribution path. The project plans to provide a normal Windows installer, but no installer is included yet.

## Security Model

Lean Beeftext intentionally limits combo evaluation and text insertion. It does not restore clipboard insertion, PowerShell execution, environment-variable reads, arbitrary key/shortcut/delay variables, Discord clipboard behavior, or upstream updater activity. Modifiers held during a substitution cause the operation to fail closed, and keyboard-hook restoration is covered by regression tests.

This is a design boundary, not a claim of formal certification or absolute security. See [SECURITY_MODEL.md](SECURITY_MODEL.md) for technical details and QA expectations.

## Project Status

Lean Beeftext 1.0.0 is based on Beeftext 16.0. Functional Windows QA covers the restricted substitution model, multiline modes, combo portability, portable isolation, and the current Windows user interface. The installer, updater design, and final icon/assets are separate future projects.

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
- The recursively checked-out XMiLib and emojilib submodules

From a Visual Studio 2022 developer environment with Qt available to CMake:

```powershell
git submodule update --init --recursive
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The GitHub Actions Windows workflow performs the same configure, Release build, complete test run, clean-source checks, deployment, provenance recording, and packaged-file checksum generation. It produces a portable QA artifact; it does not build an installer.

Project links: [repository](https://github.com/jubalslone/Beeftext), [issues](https://github.com/jubalslone/Beeftext/issues), and [releases](https://github.com/jubalslone/Beeftext/releases).
