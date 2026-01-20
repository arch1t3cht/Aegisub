#!/usr/bin/env powershell

param (
  [Parameter(Position = 0, Mandatory = $false)]
  [string]$BuildRoot = $null,
  [Parameter(Position = 1, Mandatory = $false)]
  [string]$SourceRoot = $null
)

$lastSvnRevision = 6962
$lastSvnHash = '16cd907fe7482cb54a7374cd28b8501f138116be'
$defineNumberMatch = [regex] '^#define\s+(\w+)\s+(\d+)$'
$defineStringMatch = [regex] "^#define\s+(\w+)\s+[`"']?(.+?)[`"']?$"
$semVerMatch = [regex] 'v?(\d+)\.(\d+).(\d+)(?:-(\w+))?'

$repositoryRootPath = Join-Path $PSScriptRoot .. | Resolve-Path
$sourceRootPath = if ($SourceRoot -ne $null -and $SourceRoot.Trim() -ne "") { Resolve-Path $SourceRoot } else { $repositoryRootPath }
$inGitRepo = $false
if (Get-Command git -ErrorAction SilentlyContinue) {
  $inGitRepo = [bool](git -C $repositoryRootPath rev-parse --is-inside-work-tree 2>$null)
}

if ($BuildRoot -eq $null -or $BuildRoot.Trim() -eq "")  {
  $BuildRoot = $repositoryRootPath
}

# support legacy in-tree builds
if ([System.IO.Path]::GetFullPath([System.IO.Path]::Combine((pwd).Path, $BuildRoot)) -eq
  [System.IO.Path]::GetFullPath([System.IO.Path]::Combine((pwd).Path, $repositoryRootPath))) {
    $BuildRoot = Join-Path $repositoryRootPath 'build'
  }
$gitVersionHeaderPath = Join-Path $BuildRoot 'git_version.h'

$version = @{}
if (Test-Path $gitVersionHeaderPath) {
  Get-Content $gitVersionHeaderPath | %{$_.Trim()} | ?{$_} | %{
    switch -regex ($_) {
      $defineNumberMatch {
        $version[$Matches[1]] = [int]$Matches[2];
      }
      $defineStringMatch {
        $version[$Matches[1]] = $Matches[2];
      }
    }
  }
}

function Get-ProjectVersion([string]$rootPath) {
  $mesonBuildPath = Join-Path $rootPath 'meson.build'
  if (!(Test-Path $mesonBuildPath)) { return $null }
  $m = Select-String -Path $mesonBuildPath -Pattern "version:\\s*'([^']+)'" -AllMatches | Select-Object -First 1
  if ($m -and $m.Matches.Count -gt 0) { return $m.Matches[0].Groups[1].Value }
  return $null
}

$projectVersion = Get-ProjectVersion $sourceRootPath

if ($inGitRepo) {
  $gitRevision = $lastSvnRevision + ((git -C $repositoryRootPath log --pretty=oneline "$($lastSvnHash)..HEAD" 2>$null | Measure-Object).Count)
  $gitBranch = git -C $repositoryRootPath symbolic-ref --short HEAD 2>$null
  $gitHash = git -C $repositoryRootPath rev-parse --short HEAD 2>$null
  $gitVersionString = $gitRevision, $gitBranch, $gitHash -join '-'
  $exactGitTag = git -C $repositoryRootPath describe --exact-match --tags 2>$null
}
else {
  $gitRevision = $lastSvnRevision
  $gitBranch = "nogit"
  $gitHash = "nogit"
  $gitVersionString = $gitRevision, $gitBranch, $gitHash -join '-'
  $exactGitTag = $null
}

if ($gitVersionString -eq $version['BUILD_GIT_VERSION_STRING']) {
  exit 0
}

if ($exactGitTag -match $semVerMatch) {
  $version['TAGGED_RELEASE'] = $true
  $version['RESOURCE_BASE_VERSION'] = $Matches[1..3]
  $version['INSTALLER_VERSION'] = $gitVersionString = ($Matches[1..3] -join '.') + @("-$($Matches[4])",'')[!$Matches[4]]
} else {
  if ($inGitRepo) {
    foreach ($rev in (git -C $repositoryRootPath rev-list --tags 2>$null)) {
      $tag = git -C $repositoryRootPath describe --exact-match --tags $rev 2>$null
      if ($tag -match $semVerMatch) {
        $version['TAGGED_RELEASE'] = $false
        $version['RESOURCE_BASE_VERSION'] = $Matches[1..3]
        $version['INSTALLER_VERSION'] = ($Matches[1..3] -join '.')
        break;
      }
    }
  }
  elseif ($projectVersion -match $semVerMatch) {
    $version['TAGGED_RELEASE'] = $false
    $version['RESOURCE_BASE_VERSION'] = $Matches[1..3]
    $version['INSTALLER_VERSION'] = ($Matches[1..3] -join '.') + '-nogit'
  }
}

if (!$version.ContainsKey('RESOURCE_BASE_VERSION')) {
  $version['TAGGED_RELEASE'] = $false
  $version['RESOURCE_BASE_VERSION'] = @('0', '0', '0')
  $version['INSTALLER_VERSION'] = '0.0.0-nogit'
}

$version['BUILD_GIT_VERSION_NUMBER'] = $gitRevision
$version['BUILD_GIT_VERSION_STRING'] = $gitVersionString

$version.GetEnumerator() | %{
  $type = $_.Value.GetType()
  $value = $_.Value
  $fmtValue = switch ($type) {
    ([string]) {"`"$value`""}
    ([int]) {$value.ToString()}
    ([bool]) {([int]$value).ToString()}
    ([object[]]) {$value -join ', '}
    default {
      Write-Host "no format known for type '$type' - trying default string conversion" -ForegroundColor Red
      {"`"$($value.ToString())`""}
    }
  }
  "`n#define $($_.Key) $($fmtValue)"
} | Out-File -FilePath $gitVersionHeaderPath -Encoding utf8
