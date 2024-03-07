#!/usr/bin/env powershell

param (
  [Parameter(Position = 0)]
  [string]$BuildRoot,
  [Parameter(Position = 1)]
  [string]$SourceRoot
)

$InstallerDir = Join-Path $SourceRoot "packages\win_installer" | Resolve-Path
$DepsDir = Join-Path $BuildRoot "installer-deps"
if (!(Test-Path $DepsDir)) {
	New-Item -ItemType Directory -Path $DepsDir
}

$Env:BUILD_ROOT = $BuildRoot
$Env:SOURCE_ROOT = $SourceRoot

Set-Location $DepsDir

# DepCtrl
if (!(Test-Path DependencyControl)) {
	git clone https://github.com/TypesettingTools/DependencyControl.git
	Set-Location DependencyControl
	git checkout v0.6.3-alpha
	Set-Location $DepsDir
}

# YUtils
if (!(Test-Path YUtils)) {
	git clone https://github.com/TypesettingTools/YUtils.git
}

# luajson
if (!(Test-Path luajson)) {
	git clone https://github.com/harningt/luajson.git
}

# Avisynth
if (!(Test-Path AviSynthPlus64)) {
	$avsReleases = Invoke-WebRequest "https://api.github.com/repos/AviSynth/AviSynthPlus/releases/latest" -UseBasicParsing | ConvertFrom-Json
	$avsUrl = $avsReleases.assets[0].browser_download_url
	Invoke-WebRequest $avsUrl -OutFile AviSynthPlus.7z -UseBasicParsing
	7z x AviSynthPlus.7z
	Rename-Item (Get-ChildItem -Filter "AviSynthPlus_*" -Directory) AviSynthPlus64
	Remove-Item AviSynthPlus.7z
}

# VSFilter
if (!(Test-Path VSFilter)) {
	$vsFilterDir = New-Item -ItemType Directory VSFilter
	Set-Location $vsFilterDir
	$vsFilterReleases = Invoke-WebRequest "https://api.github.com/repos/pinterf/xy-VSFilter/releases/latest" -UseBasicParsing | ConvertFrom-Json
	$vsFilterUrl = $vsFilterReleases.assets[0].browser_download_url
	Invoke-WebRequest $vsFilterUrl -OutFile VSFilter.7z -UseBasicParsing
	7z x VSFilter.7z
	Remove-Item VSFilter.7z
	Set-Location $DepsDir
}

### VapourSynth plugins

# L-SMASH-Works
if (!(Test-Path L-SMASH-Works)) {
	New-Item -ItemType Directory L-SMASH-Works
	$lsmasReleases = Invoke-WebRequest "https://api.github.com/repos/AkarinVS/L-SMASH-Works/releases/latest" -Headers $GitHeaders -UseBasicParsing | ConvertFrom-Json
	$lsmasUrl = "https://github.com/AkarinVS/L-SMASH-Works/releases/download/" + $lsmasReleases.tag_name + "/release-x86_64-cachedir-cwd.zip"
	Invoke-WebRequest $lsmasUrl -OutFile release-x86_64-cachedir-cwd.zip -UseBasicParsing
	Expand-Archive -LiteralPath release-x86_64-cachedir-cwd.zip -DestinationPath L-SMASH-Works
	Remove-Item release-x86_64-cachedir-cwd.zip
}

# BestSource
if (!(Test-Path BestSource)) {
	$bsDir = New-Item -ItemType Directory BestSource
	Set-Location $bsDir
	$basReleases = Invoke-WebRequest "https://api.github.com/repos/vapoursynth/bestsource/releases/latest" -Headers $GitHeaders -UseBasicParsing | ConvertFrom-Json
	$bsUrl = $basReleases.assets[0].browser_download_url
	Invoke-WebRequest $bsUrl -OutFile bestsource.7z -UseBasicParsing
	7z x bestsource.7z
	Remove-Item bestsource.7z
	Set-Location $DepsDir
}

# SCXVid
if (!(Test-Path SCXVid)) {
	$scxDir = New-Item -ItemType Directory SCXVid
	Set-Location $scxDir
	$scxReleases = Invoke-WebRequest "https://api.github.com/repos/dubhater/vapoursynth-scxvid/releases/latest" -Headers $GitHeaders -UseBasicParsing | ConvertFrom-Json
	$scxUrl = "https://github.com/dubhater/vapoursynth-scxvid/releases/download/" + $scxReleases.tag_name + "/vapoursynth-scxvid-v1-win64.7z"
	Invoke-WebRequest $scxUrl -OutFile vapoursynth-scxvid-v1-win64.7z -UseBasicParsing
	7z x vapoursynth-scxvid-v1-win64.7z
	Remove-Item vapoursynth-scxvid-v1-win64.7z
	Set-Location $DepsDir
}

# WWXD
if (!(Test-Path WWXD)) {
	New-Item -ItemType Directory WWXD
	$wwxdReleases = Invoke-WebRequest "https://api.github.com/repos/dubhater/vapoursynth-wwxd/releases/latest" -Headers $GitHeaders -UseBasicParsing | ConvertFrom-Json
	$wwxdUrl = "https://github.com/dubhater/vapoursynth-wwxd/releases/download/" + $wwxdReleases.tag_name + "/libwwxd64.dll"
	Invoke-WebRequest $wwxdUrl -OutFile WWXD/libwwxd64.dll -UseBasicParsing
}


# ffi-experiments
if (!(Test-Path ffi-experiments)) {
	Get-Command "moonc" # check to ensure Moonscript is present
	git clone https://github.com/TypesettingTools/ffi-experiments.git
	Set-Location ffi-experiments
	meson build -Ddefault_library=static
	meson compile -C build
	Set-Location $DepsDir
}

# VC++ redistributable
if (!(Test-Path VC_redist)) {
	$redistDir = New-Item -ItemType Directory VC_redist
	Invoke-WebRequest https://aka.ms/vs/16/release/VC_redist.x64.exe -OutFile "$redistDir\VC_redist.x64.exe" -UseBasicParsing
}

# TODO dictionaries

# localization
Set-Location $BuildRoot
meson compile aegisub-gmo

# Invoke InnoSetup
$IssUrl = Join-Path $InstallerDir "aegisub_depctrl.iss"
iscc $IssUrl
