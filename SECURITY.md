# Security Policy

Lean Beeftext deliberately keeps a narrow execution model, but no software is guaranteed to be free of security defects. The technical boundary and known limitations are documented in [SECURITY_MODEL.md](SECURITY_MODEL.md).

## Supported versions

Security fixes are currently intended for the latest public 1.0.x release. Users should update to the newest published release before reporting a problem that may already have been addressed.

## Reporting a vulnerability

Please do not publish exploit details, sensitive data, or a working proof of concept in a public issue.

If GitHub offers **Report a vulnerability** on this repository's Security tab, use that private reporting channel. Otherwise, contact the maintainer through the private contact options on [Jubal Slone's GitHub profile](https://github.com/jubalslone) and request a private channel for the report.

A useful report includes the affected Lean Beeftext version, Windows version, installed or portable mode, a concise description of the impact, and reproducible steps when they can be shared safely.

Ordinary bugs and feature requests that do not contain sensitive security information belong in the repository's normal issue tracker.

## Scope

Reports are especially useful when they show that Lean Beeftext crosses one of its documented boundaries, such as executing blocked variable syntax, reading data it should not read, escaping installed/portable storage isolation, bypassing migration or cleanup safeguards, or producing unintended privileged behavior.

Security reports should distinguish a defect from documented platform behavior. For example, Microsoft Defender SmartScreen reputation warnings for a newly signed application do not by themselves indicate a broken Authenticode signature, and Windows may block text insertion into higher-integrity applications by design.
