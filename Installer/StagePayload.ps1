param(
	[Parameter(Mandatory = $true)]
	[ValidateSet('Installed', 'Portable')]
	[string]$Mode,

	[Parameter(Mandatory = $true)]
	[string]$BuildDirectory,

	[Parameter(Mandatory = $true)]
	[string]$Destination,

	[string]$SourceCommit = '',
	[string]$Repository = 'jubalslone/Beeftext',
	[string]$WorkflowRun = 'local'
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($SourceCommit)) {
	$SourceCommit = (git rev-parse HEAD).Trim()
}

$releaseExecutable = Get-ChildItem -Path $BuildDirectory -Recurse -File -Filter Beeftext.exe |
	Where-Object { $_.FullName -match '[\\/]Release[\\/]' } |
	Select-Object -First 1
if (-not $releaseExecutable) {
	throw "Could not locate the Release Beeftext.exe under $BuildDirectory."
}

if (Test-Path -LiteralPath $Destination) {
	Remove-Item -LiteralPath $Destination -Recurse -Force
}
New-Item -ItemType Directory -Path $Destination -Force | Out-Null
Copy-Item -LiteralPath $releaseExecutable.FullName -Destination (Join-Path $Destination 'LeanBeeftext.exe')

$translationDirectory = Join-Path $releaseExecutable.DirectoryName 'translations'
if (Test-Path -LiteralPath $translationDirectory -PathType Container) {
	Copy-Item -LiteralPath $translationDirectory -Destination $Destination -Recurse
}

& windeployqt --release --compiler-runtime --dir $Destination (Join-Path $Destination 'LeanBeeftext.exe')
if ($LASTEXITCODE -ne 0) {
	throw "windeployqt failed with exit code $LASTEXITCODE."
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsInstall = & $vswhere -latest -products * -property installationPath
if (-not $vsInstall) {
	throw 'Could not locate the Visual Studio installation.'
}
$redistRoot = Join-Path $vsInstall 'VC/Redist/MSVC'
$redistVersion = Get-ChildItem -Path $redistRoot -Directory |
	Where-Object { $_.Name -match '^\d+(\.\d+)+$' } |
	Sort-Object { [version]$_.Name } -Descending |
	Select-Object -First 1
if (-not $redistVersion) {
	throw "Could not locate a numeric Visual C++ runtime version under $redistRoot."
}
$crtDirectory = Get-ChildItem -Path (Join-Path $redistVersion.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' |
	Select-Object -First 1
if (-not $crtDirectory) {
	throw 'Could not locate the x64 Visual C++ runtime directory.'
}
Copy-Item -Path (Join-Path $crtDirectory.FullName '*.dll') -Destination $Destination

foreach ($runtimeDll in @('MSVCP140.dll', 'VCRUNTIME140.dll', 'VCRUNTIME140_1.dll')) {
	if (-not (Test-Path -LiteralPath (Join-Path $Destination $runtimeDll))) {
		throw "The staged payload is missing $runtimeDll."
	}
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
foreach ($document in @(
	'LICENSE',
	'LICENSE.GPL-3.0.txt',
	'LICENSE.Qt-LGPL-3.0.txt',
	'README.md',
	'SECURITY_MODEL.md',
	'THIRD_PARTY_NOTICES.md',
	'ARTIFACT_SIGNING.md',
	'INSTALLER.md'
)) {
	$source = Join-Path $repositoryRoot $document
	if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
		throw "Required payload document is missing: $document"
	}
	Copy-Item -LiteralPath $source -Destination $Destination
}

if ($Mode -eq 'Portable') {
	'Do not delete this file. It keeps Lean Beeftext in portable mode.' |
		Set-Content -LiteralPath (Join-Path $Destination 'Portable.bin') -Encoding ascii
}
else {
	foreach ($beacon in @('Portable.bin', 'PortableApps.bin')) {
		if (Test-Path -LiteralPath (Join-Path $Destination $beacon)) {
			throw "Installed payload must not contain $beacon."
		}
	}
}

$qtVersion = (& qmake -query QT_VERSION).Trim()
@(
	'Product: Lean Beeftext 1.0.0'
	"Distribution: $Mode"
	"Repository: $Repository"
	"Source commit: $SourceCommit"
	"Workflow run: $WorkflowRun"
	"Qt version: $qtVersion"
	"Built UTC: $((Get-Date).ToUniversalTime().ToString('o'))"
) | Set-Content -LiteralPath (Join-Path $Destination 'BUILD_INFO.txt') -Encoding utf8

$checksumPath = Join-Path $Destination 'SHA256SUMS.txt'
$checksumFullPath = [IO.Path]::GetFullPath($checksumPath)
$manifestLines = @(Get-ChildItem -Path $Destination -Recurse -File |
	Where-Object { $_.FullName -ne $checksumFullPath } |
	Sort-Object FullName |
	ForEach-Object {
		$relativePath = [IO.Path]::GetRelativePath($Destination, $_.FullName).Replace('\', '/')
		$hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
		"$hash  $relativePath"
	})
$manifestLines | Set-Content -LiteralPath $checksumPath -Encoding ascii

if ($Mode -eq 'Installed') {
	foreach ($beacon in @('Portable.bin', 'PortableApps.bin')) {
		if (Get-ChildItem -Path $Destination -Recurse -File -Filter $beacon) {
			throw "Installed payload contains forbidden portable beacon $beacon."
		}
	}
}
