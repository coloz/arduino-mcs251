# Compatibility entry point. Packaging itself is implemented by native stcxx.
[CmdletBinding()]
param(
    [ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version,
    [string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot
$PlatformVersion = (Select-String -LiteralPath (Join-Path $RepoRoot 'platform.txt') -Pattern '^version=(.+)$').Matches[0].Groups[1].Value
if (-not $Version) { $Version = $PlatformVersion }
if ($Version -ne $PlatformVersion) { throw 'Package version differs from platform.txt.' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $RepoRoot 'dist' }
$Driver = Join-Path $RepoRoot 'tools/stcxx-driver/stcxx.exe'
if (-not (Test-Path -LiteralPath $Driver -PathType Leaf)) {
    throw 'Build the native driver first: node scripts/build-native-driver.mjs'
}
$Archive = Join-Path $OutputDirectory "arduino-mcs251-$Version.zip"
& $Driver package-platform $RepoRoot $Archive
if ($LASTEXITCODE -ne 0) { throw "Native packaging failed with exit code $LASTEXITCODE" }
$Report = Get-Content -LiteralPath "$Archive.json" -Raw -Encoding UTF8 | ConvertFrom-Json
[pscustomobject]@{
    version = $Version
    archiveFileName = $Report.archiveFileName
    size = $Report.size
    checksum = "SHA-256:$($Report.sha256)"
    path = [IO.Path]::GetFullPath($Archive)
}
