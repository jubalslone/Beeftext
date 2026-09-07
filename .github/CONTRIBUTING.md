# Contributing to Lean Beeftext

Thank you for helping improve Lean Beeftext. Before opening an issue, check the [existing issues](https://github.com/jubalslone/Beeftext/issues) and include the exact Lean Beeftext version, Windows version, portable/installed status, and reproducible steps.

Lean Beeftext deliberately has a narrower execution model than upstream Beeftext. Changes must preserve the boundaries in [SECURITY_MODEL.md](../SECURITY_MODEL.md), including literal handling of blocked variables, direct text insertion, modifier refusal, bounded cursor movement, portable containment, and disabled upstream updating.

For code changes:

- Start from the current development branch requested by the maintainer.
- Keep changes focused and preserve unrelated history.
- Add or update regression tests.
- Run the complete test suite and Windows Release build.
- Do not describe the application as absolutely secure or formally certified.
- Preserve Xavier Michelon's upstream authorship and MIT notice, and add accurate notices for new third-party dependencies.

For upstream Beeftext behavior, issues, or contributions unrelated to this fork, use the explicitly separate [upstream Beeftext project](https://github.com/xmichelo/Beeftext).
