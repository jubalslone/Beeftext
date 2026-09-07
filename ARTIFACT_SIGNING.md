# Artifact Signing Status

Lean Beeftext's routine Windows pull-request and QA artifacts are unsigned. The application and its documentation do not claim otherwise.

Authenticode signing is reserved for a separately reviewed production-release process. That process must:

- build the exact reviewed source commit;
- run the complete automated test suite;
- verify that the build did not change tracked source;
- sign only the Lean Beeftext executable through a protected external signing service;
- validate the publisher identity and timestamp on the signed executable;
- package the signed executable without re-signing third-party libraries;
- record source provenance and generate checksums from the final packaged bytes; and
- stop rather than publish an unsigned fallback if signing or verification fails.

The repository contains a disabled, manually gated workflow skeleton for future production-signing work. Its external identity, access, billing, and approval configuration is intentionally not documented here and must be reviewed outside the public repository before that workflow is enabled.

Code signing establishes publisher identity and participates in normal Windows trust and reputation mechanisms. It does not prove that software is secure, guarantee immediate SmartScreen reputation, or eliminate every first-run warning.

No GitHub Release, installer, production signature, or 1.0.0 tag is created by the current branding QA workflow.
