# Microsoft Artifact Signing release plan

This is a production-signing plan for Lean Beeftext. It does not claim that the current unsigned QA builds are signed or secure, and it does not create an Azure account, certificate profile, paid service, credential, release, or tag.

Authenticode provides a verifiable publisher identity and participates in normal Windows trust and reputation mechanisms. It does not prove that an application is secure, guarantee immediate SmartScreen reputation, or eliminate every first-run warning.

## Pipeline boundary

The normal `windows-build.yml` pull-request workflow remains unsigned and continues to produce portable QA artifacts. Production signing is isolated in `artifact-signing-release.yml`, which is manual-only and disabled until the protected `artifact-signing-production` GitHub Environment contains a complete reviewed configuration and `ARTIFACT_SIGNING_ENABLED` is exactly `true`.

That workflow has no unsigned fallback. Its order is:

1. Require a full 40-character source commit and complete signing configuration.
2. Check out that exact commit and submodules, then verify source identity and tracked cleanliness.
3. Build and run the complete test suite.
4. Verify the build did not rewrite tracked source.
5. Copy only the newly built `Beeftext.exe` to an isolated signing directory.
6. Exchange GitHub's short-lived OIDC token for Azure access and sign that executable with SHA-256 plus Microsoft's RFC 3161 timestamp service.
7. Require valid Authenticode trust, the exact configured publisher subject, a timestamp certificate, and a successful `SignTool verify /pa /all /v` result.
8. Package the verified executable with unchanged third-party Qt and Microsoft runtime DLLs. The ZIP/archive itself is never submitted for signing, and third-party DLLs are never re-signed.
9. Re-verify the packaged executable, record provenance, generate checksums from the final packaged bytes, recheck tracked source cleanliness, and only then upload the signed release-candidate artifact.

A future installer must be signed as its own executable after it is built and before final checksums and publication. It must not reuse an earlier executable's verification result. A future GitHub Release publication step should consume only the verified output of this job; it must never rebuild, silently skip signing, or publish from the unsigned PR workflow.

## External Azure and GitHub setup

Perform these steps outside the repository with the firm's Azure and GitHub administrators:

1. Confirm eligibility and budget. Artifact Signing requires a paid Azure subscription; Microsoft says free, trial, and sponsored subscriptions are unsupported. Treat Artifact Signing account creation as the billing point: the service is priced per account, and Microsoft says the monthly SKU charge is not prorated. Review current pricing before creation.
2. Register the `Microsoft.CodeSigning` resource provider, then create a resource group and an Artifact Signing account in a supported region. Basic is sufficient for one profile of each type and the lower signing quota; Premium increases the profile and signature quotas.
3. Complete Public Trust organization identity validation using the firm's exact legal and billing information and a monitored firm-domain email address. Microsoft says validation can take 1–20 business days or longer. Create a Public Trust certificate profile only after validation succeeds.
4. Create a dedicated Microsoft Entra application and service principal for GitHub Actions. Add a federated identity credential restricted to this repository's `artifact-signing-production` GitHub Environment. Do not create a client secret or upload a certificate/private key.
5. Assign only `Artifact Signing Certificate Profile Signer` to that service principal, scoped to the one production certificate profile. Do not grant Owner or Contributor to the workflow identity.
6. Create the protected GitHub Environment `artifact-signing-production`. Require an authorized reviewer and restrict deployment branches or tags according to the firm's release policy. Store the three Azure identifiers as environment secrets named `AZURE_CLIENT_ID`, `AZURE_TENANT_ID`, and `AZURE_SUBSCRIPTION_ID`.
7. Store the non-secret service configuration as environment variables named `ARTIFACT_SIGNING_ENDPOINT`, `ARTIFACT_SIGNING_ACCOUNT_NAME`, `ARTIFACT_SIGNING_CERTIFICATE_PROFILE`, and `EXPECTED_AUTHENTICODE_SUBJECT`. Copy the expected subject exactly from the validated certificate profile. Finally set `ARTIFACT_SIGNING_ENABLED` to `true` after a workflow review. No values belong in source control.
8. Manually dispatch the signed workflow with the reviewed source commit. Compare `BUILD_INFO.txt`, `SHA256SUMS.txt`, the workflow logs, and the downloaded executable's Digital Signatures properties before any release publication.

Microsoft's current documentation is the authority for setup and pricing:

- [Artifact Signing overview](https://learn.microsoft.com/en-us/azure/artifact-signing/overview)
- [Set up Artifact Signing](https://learn.microsoft.com/en-us/azure/artifact-signing/quickstart)
- [Artifact Signing roles](https://learn.microsoft.com/en-us/azure/artifact-signing/tutorial-assign-roles)
- [Signing integrations and timestamp guidance](https://learn.microsoft.com/en-us/azure/artifact-signing/how-to-signing-integrations)
- [Artifact Signing billing and unenrollment FAQ](https://learn.microsoft.com/en-us/azure/artifact-signing/faq)
- [GitHub OIDC for Azure](https://docs.github.com/en/actions/how-tos/secure-your-work/security-harden-deployments/oidc-in-azure)
- [Microsoft's Artifact Signing GitHub Action](https://github.com/Azure/artifact-signing-action)

The workflow pins `azure/login` and `azure/artifact-signing-action` to reviewed commit SHAs. Re-review their source, release notes, transitive actions, and input names before deliberately updating either pin.

## Disable or cancel

To stop new signed builds immediately, set `ARTIFACT_SIGNING_ENABLED` to anything other than exact lowercase `true`, disable the workflow, or remove the GitHub Environment's access. Also remove the Entra federated credential and the certificate-profile role assignment when the workflow is retired.

Microsoft documents full unenrollment as deleting the Artifact Signing account, unregistering `Microsoft.CodeSigning`, and separately deleting the shared identity validation when it is no longer used by another account. Account deletion removes its certificate profiles and stops certificate renewal and future signing, but does not invalidate files that were already signed. Confirm billing closure in Azure Cost Management and Billing; do not assume deleting only the GitHub configuration cancels the Azure service.
