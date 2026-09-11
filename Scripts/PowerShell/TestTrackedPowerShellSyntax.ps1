[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Push-Location $repositoryRoot
try {
	$scripts = @(git ls-files -- '*.ps1')
	if ($LASTEXITCODE -ne 0) {
		throw 'Could not enumerate tracked PowerShell scripts.'
	}
	if ($scripts.Count -eq 0) {
		throw 'No tracked PowerShell scripts were found.'
	}

	$failures = [Collections.Generic.List[string]]::new()
	foreach ($script in $scripts) {
		$tokens = $null
		$errors = $null
		$path = [IO.Path]::GetFullPath((Join-Path $repositoryRoot $script))
		[System.Management.Automation.Language.Parser]::ParseFile(
			$path,
			[ref]$tokens,
			[ref]$errors) | Out-Null
		foreach ($parseError in @($errors)) {
			$failures.Add(('{0}:{1}:{2}: {3}' -f
				$script,
				$parseError.Extent.StartLineNumber,
				$parseError.Extent.StartColumnNumber,
				$parseError.Message))
		}
	}

	if ($failures.Count -ne 0) {
		$failures | ForEach-Object { Write-Error $_ -ErrorAction Continue }
		throw "PowerShell syntax validation found $($failures.Count) parser error(s)."
	}

	Write-Host "PowerShell syntax validation passed for $($scripts.Count) tracked script(s)."
}
finally {
	Pop-Location
}
