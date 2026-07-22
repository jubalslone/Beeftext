# TLF Safe Build

This branch is a restricted Beeftext build intended for ordinary law-firm text expansion.

## Disabled features

- Automatic and manual upstream update checks
- PowerShell variables (`#{powershell:...}`)
- Environment-variable expansion (`#{envVar:...}`)
- Clipboard-based variables (`#{clipboard}` and `#{discordemoji}`)
- Key, shortcut, and delay control fragments (`#{key:...}`, `#{shortcut:...}`, and `#{delay:...}`)

Blocked variables remain visible as literal text so unsafe or incompatible imported combos fail visibly.

## Features retained

- Plain-text snippets
- Date and time variables
- Reusable combo variables
- Uppercase, lowercase, and trim combo transformations
- User-input prompts
- Local combo storage and backup

## Update process

Do not install upstream Beeftext updates over this build. Review upstream changes, merge or cherry-pick them into this fork, rebuild, and distribute the resulting internal package.

## Validation checklist

1. Confirm an ordinary text combo expands correctly.
2. Confirm `#{powershell:C:\\test.ps1}` remains literal and does not launch PowerShell.
3. Confirm `#{shortcut:Win+R}` remains literal and does not open the Run dialog.
4. Confirm `#{key:enter}` remains literal and does not press Enter.
5. Confirm `#{clipboard}` remains literal and does not paste clipboard contents.
6. Confirm update controls are hidden and no request is made to `beeftext.org` during startup.
