param(
	[Parameter(Mandatory = $true)]
	[string]$FilePath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ($FilePath.IndexOfAny([char[]]'*?') -ge 0) {
	throw 'The signing target must be one explicit file path; wildcards are not allowed.'
}

$resolvedPaths = @(Resolve-Path -LiteralPath $FilePath -ErrorAction Stop)
if ($resolvedPaths.Count -ne 1) {
	throw "The signing target did not resolve to exactly one path: $FilePath"
}

$target = Get-Item -LiteralPath $resolvedPaths[0].Path -Force
if (-not $target.PSIsContainer -and $target.Extension -ieq '.exe') {
	$targetPath = $target.FullName
}
else {
	throw "The signing target must be one existing .exe file: $FilePath"
}

foreach ($name in @(
	'ARTIFACT_SIGNING_ENDPOINT',
	'ARTIFACT_SIGNING_ACCOUNT',
	'ARTIFACT_SIGNING_PROFILE'
)) {
	$value = [Environment]::GetEnvironmentVariable($name)
	if ([string]::IsNullOrWhiteSpace($value)) {
		throw "Required signing configuration is missing: $name"
	}
}

$module = @(Get-Module -ListAvailable -Name ArtifactSigning |
	Where-Object Version -eq ([version]'0.1.8'))
if ($module.Count -ne 1) {
	throw 'ArtifactSigning PowerShell module 0.1.8 is not installed exactly once.'
}
Import-Module $module[0].Path -Force

$beforeHash = (Get-FileHash -LiteralPath $targetPath -Algorithm SHA256).Hash
$beforeSignature = Get-AuthenticodeSignature -LiteralPath $targetPath
if ($beforeSignature.Status -eq 'Valid') {
	throw "Refusing to add another signature to an already valid target: $targetPath"
}

$signingParameters = @{
	Endpoint = $env:ARTIFACT_SIGNING_ENDPOINT
	CodeSigningAccountName = $env:ARTIFACT_SIGNING_ACCOUNT
	CertificateProfileName = $env:ARTIFACT_SIGNING_PROFILE
	Files = $targetPath
	FileDigest = 'SHA256'
	TimestampRfc3161 = 'http://timestamp.acs.microsoft.com'
	TimestampDigest = 'SHA256'
	ExcludeEnvironmentCredential = $true
	ExcludeWorkloadIdentityCredential = $true
	ExcludeManagedIdentityCredential = $true
	ExcludeSharedTokenCacheCredential = $true
	ExcludeVisualStudioCredential = $true
	ExcludeVisualStudioCodeCredential = $true
	ExcludeAzurePowerShellCredential = $true
	ExcludeAzureDeveloperCliCredential = $true
	ExcludeInteractiveBrowserCredential = $true
}

# azure/login established the Azure CLI session through GitHub OIDC. All other
# DefaultAzureCredential sources are excluded above, so no long-lived secret is used.
Invoke-ArtifactSigning @signingParameters | Write-Host

$afterHash = (Get-FileHash -LiteralPath $targetPath -Algorithm SHA256).Hash
if ($afterHash -eq $beforeHash) {
	throw "Artifact Signing did not change the target bytes: $targetPath"
}

$signature = Get-AuthenticodeSignature -LiteralPath $targetPath
if ($signature.Status -ne 'Valid') {
	throw "Authenticode verification failed for $targetPath: $($signature.Status)"
}
if (-not $signature.SignerCertificate) {
	throw "Authenticode verification found no signer certificate: $targetPath"
}
if (-not $signature.TimeStamperCertificate) {
	throw "Authenticode verification found no RFC 3161 timestamp certificate: $targetPath"
}

Write-Host "Signed target: $targetPath"
Write-Host "Signed SHA-256: $($afterHash.ToLowerInvariant())"
Write-Host "Signer subject: $($signature.SignerCertificate.Subject)"
Write-Host "Timestamp subject: $($signature.TimeStamperCertificate.Subject)"
