# Artifact Signing

Lean Beeftext's routine pull-request and Windows QA artifacts remain unsigned. Authenticode signing is confined to the manual `Lean Beeftext signed production candidate` workflow and does not publish a release, create a tag, or change a branch.

## Production signing boundary

The production workflow signs only Lean-owned executable files:

- the one built `LeanBeeftext.exe`, reused byte-for-byte by installed and portable packages;
- Inno Setup's generated uninstaller; and
- the final `Lean-Beeftext-Setup-1.0.0.exe` installer.

Lean Beeftext does not re-sign Qt, Microsoft runtime, OpenSSL, or other third-party DLLs. Their existing vendor signatures, if any, are left unchanged.

The workflow must be dispatched manually with the exact reviewed 40-character commit SHA. It checks out that commit without persisted GitHub credentials, runs the complete test suite and packaging checks, and fails instead of producing an unsigned fallback.

## Authentication and tools

The signing job has only:

```yaml
permissions:
  contents: read
  id-token: write
```

It runs behind the protected `production-signing` GitHub environment. `azure/login` exchanges GitHub's short-lived OIDC identity for a Microsoft Entra session. There is no PFX, exported private key, client secret, or other long-lived signing credential in the repository or workflow.

The protected environment supplies the Artifact Signing endpoint, account name, and certificate profile. The production workflow uses Microsoft's pinned `Azure/artifact-signing-action` for the application executable. That action installs Microsoft's `ArtifactSigning` PowerShell module; the small `Installer/Invoke-ArtifactSigning.ps1` bridge uses the same module and already-authenticated Azure CLI session when Inno invokes its documented `SignTool` integration.

The wrapper:

- requires one explicit existing regular-file path and rejects wildcards;
- canonicalizes the path, requires its parent to be the repository's dedicated `Installer\_output` directory, and rejects links or reparse points;
- validates the DOS and PE signatures in the file itself instead of trusting a filename extension (Inno 7.1.0 signs its generated uninstaller as a temporary PE such as `uninst.e32.tmp`);
- reads endpoint/account/profile only from the protected environment;
- allows only the OIDC-backed Azure CLI credential path;
- signs with SHA-256 and requests an RFC 3161 SHA-256 timestamp from `http://timestamp.acs.microsoft.com`;
- refuses to append to an already valid signature; and
- returns failure unless the bytes changed and Windows reports a valid signature, signer certificate, and timestamp certificate.

Inno Setup 7.1.0 receives the wrapper as the named `leanartifact` SignTool. The production-only `SignTool=leanartifact` and `SignedUninstaller=yes` directives cause Inno to sign both its generated uninstaller and final Setup executable through that same service. Routine unsigned QA compilation does not define `ProductionSigning`, so it does not invoke the production signer.

## Ordering and verification

The release-candidate order is:

1. verify and build the exact reviewed clean source once;
2. run the full automated suite;
3. stage installed and portable payloads from that one executable;
4. sign and verify the installed staging copy of `LeanBeeftext.exe`;
5. copy those exact signed bytes into portable staging;
6. regenerate both payload manifests and build the portable ZIP;
7. compile the installer through Inno's supported signed-uninstaller/SignTool path;
8. verify the final installer;
9. install and reinstall the candidate, then verify the deployed application and `unins000.exe`;
10. exercise the existing data-preserving uninstall checks; and
11. generate distribution hashes and signing provenance from the final signed bytes.

Every required Lean-owned PE must have `Get-AuthenticodeSignature` status `Valid`, a signer certificate, and an RFC 3161 timestamp certificate. The application signature establishes the signer subject for that run; the portable application, installer, installed application, and generated uninstaller must report the same subject. The workflow records subjects and thumbprints for audit rather than hard-coding a certificate subject that can change as Artifact Signing rotates certificates.

`SHA256SUMS.txt` inside each payload is regenerated after the signed application bytes and signing provenance are final. `DISTRIBUTION_SHA256SUMS.txt` is generated only after the final installer and portable ZIP exist. Pre-signing hashes are not release hashes.

Routine Windows CI parses every tracked PowerShell script with PowerShell's own language parser. The production and smoke workflows repeat that preflight before Azure authentication so executable signing code cannot reach credentials with a latent parser error.

The manual `Azure signing smoke test` retains the known-good direct Microsoft action as a baseline and additionally compiles a disposable installer with pinned Inno Setup 7.1.0. That compile uses the same repository wrapper and literal Inno `$q` / `$f` SignTool command as production, with `SignedUninstaller=yes`. The workflow verifies the disposable Setup and installed `unins000.exe` signatures, signer identity, and timestamp certificates, then silently uninstalls and removes the disposable test identity. It does not build or publish a Lean Beeftext release candidate.

## What Authenticode does and does not establish

Authenticode gives Windows verifiable publisher identity and protects signed-file integrity. The timestamp allows Windows to evaluate a signature after the short-lived signing certificate expires. It does not prove that the application is secure, and it does not guarantee that Microsoft Defender SmartScreen will never warn; reputation can still take time to develop.

This candidate workflow uploads private CI artifacts for manual QA. It does not create a GitHub Release or otherwise publish them as a final 1.0.0 release.
