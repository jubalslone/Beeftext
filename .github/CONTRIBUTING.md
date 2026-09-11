# Contributing to Lean Beeftext

Thank you for helping improve Lean Beeftext.

For ordinary bugs and feature requests, check the [existing issues](https://github.com/jubalslone/lean-beeftext/issues) first. Bug reports are most useful when they include the exact Lean Beeftext version, Windows version, installed or portable mode, and reproducible steps.

Potential security vulnerabilities should not be reported with sensitive details in a public issue. Please follow [SECURITY.md](../SECURITY.md) instead.

Lean Beeftext deliberately has a narrower execution model than upstream Beeftext. Changes must preserve the boundaries in [SECURITY_MODEL.md](../SECURITY_MODEL.md), including literal handling of blocked variables, direct text insertion, modifier refusal, bounded cursor movement, portable containment, and disabled upstream updating. Feature requests that widen those boundaries need especially careful justification and review.

For code changes:

- Start from the current `master` branch and make changes on a focused topic branch.
- Keep changes focused and preserve unrelated history.
- Add or update regression tests when behavior changes.
- Run the complete test suite and Windows Release build.
- Do not describe the application as absolutely secure or formally certified.
- Preserve Xavier Michelon's upstream authorship and MIT notice, and add accurate notices for new third-party dependencies.

Pull requests should explain what changed, why the change belongs in Lean Beeftext, and what testing was performed.

For upstream Beeftext behavior, issues, or contributions unrelated to this fork, use the explicitly separate [upstream Beeftext project](https://github.com/xmichelo/Beeftext).
