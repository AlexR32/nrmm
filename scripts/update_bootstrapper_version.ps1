#!/usr/bin/env pwsh
# Synchronises the bootstrapper's version resource with the top-level .version file.

[CmdletBinding()]
param(
    [string]$VersionFile,
    [string]$ResourceFile
)

$ErrorActionPreference = 'Stop'

$root = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
if (-not $VersionFile) { $VersionFile = Join-Path $root '..\.version' }
if (-not $ResourceFile) { $ResourceFile = Join-Path $root '..\bootstrapper\bootstrapper.rc' }

if (-not (Test-Path -LiteralPath $VersionFile)) {
    throw "Version file not found: $VersionFile"
}
if (-not (Test-Path -LiteralPath $ResourceFile)) {
    throw "Resource file not found: $ResourceFile"
}

$version = (Get-Content -LiteralPath $VersionFile -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+') {
    throw "Invalid .version '$version' (expected semantic version, e.g. 1.2.3)"
}

$core = ($version -split '[-+]')[0]
$parts = $core.Split('.')
$major = [int]$parts[0]
$minor = [int]$parts[1]
$patch = [int]$parts[2]
$build = if ($parts.Length -ge 4) { [int]$parts[3] } else { 0 }

$tuple = "$major,$minor,$patch,$build"
$text = "$major.$minor.$patch.$build"

$content = Get-Content -LiteralPath $ResourceFile -Raw
$original = $content

$number = '[0-9,\t ]+'
$content = [regex]::Replace($content, '([ \t]*FILEVERSION[ \t]+)' + $number, ('${1}' + $tuple))
$content = [regex]::Replace($content, '([ \t]*PRODUCTVERSION[ \t]+)' + $number, ('${1}' + $tuple))
$content = [regex]::Replace($content, '("FileVersion",[ \t]*")[^"]*(")', ('${1}' + $text + '${2}'))
$content = [regex]::Replace($content, '("ProductVersion",[ \t]*")[^"]*(")', ('${1}' + $text + '${2}'))

if ($content -ne $original) {
    [System.IO.File]::WriteAllText($ResourceFile, $content)
    Write-Host "Updated $ResourceFile to version $text"
} else {
    Write-Host "$ResourceFile is already at version $text"
}
