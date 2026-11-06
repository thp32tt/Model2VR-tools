# Daytona original x86 source evidence collector (local/private ONLY).
# Seven USER-LABELLED, separate original emulator runs. No automatic scene
# classification, ROM inspection, game patching, network requests or uploads.
[CmdletBinding()]
param(
  [string]$PackageRoot = $PSScriptRoot,
  [string]$ExistingSessionFolder = ""
)
$ErrorActionPreference = "Stop"
$labels = @("race_a","race_b","title","menu","sky","hud","aux")
$PackageRoot = [IO.Path]::GetFullPath($PackageRoot)
$logRoot = Join-Path $PackageRoot "logs"
$analyzer = Join-Path $PackageRoot "model2_world_pass_evidence.py"
$drawAuditor = Join-Path $PackageRoot "model2_draw_source_report.py"
if (!(Test-Path -LiteralPath $analyzer -PathType Leaf) -or
    !(Test-Path -LiteralPath $drawAuditor -PathType Leaf)) {
  throw "Missing PRIVATE source analyzers; use the complete matched VR release ZIP."
}
if (!(Test-Path -LiteralPath $logRoot -PathType Container)) {
  New-Item -ItemType Directory -Force -Path $logRoot | Out-Null
}
$folder = "private_original_evidence_" + (Get-Date).ToUniversalTime().ToString("yyyyMMdd_HHmmss_fff")
$destination = Join-Path $logRoot $folder
New-Item -ItemType Directory -Path $destination -ErrorAction Stop | Out-Null
$copies = @{}
$hashes = @{}
$knownPaths = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$knownDigests = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$knownNonces = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$reports = @{}
# Resolve Python before asking the user to run seven original x86 games.
$pythonLauncher = Get-Command py -ErrorAction SilentlyContinue
$pythonPrefix = @("-3")
if ($null -eq $pythonLauncher) {
  $pythonLauncher = Get-Command python -ErrorAction SilentlyContinue
  $pythonPrefix = @()
}
if ($null -eq $pythonLauncher) {
  throw "Python 3 is required for per-session PRIVATE original source audit."
}
function Invoke-PrivateAudit([string[]]$auditArguments) {
  $launchArgs = @($pythonPrefix) + @($auditArguments)
  $startInfo = New-Object System.Diagnostics.ProcessStartInfo
  $startInfo.FileName = $pythonLauncher.Source
  $startInfo.UseShellExecute = $false
  $startInfo.RedirectStandardOutput = $true
  $startInfo.RedirectStandardError = $true
  # Original private captures live only on this PC. Paths are Windows
  # filenames without embedded quotes and never require network requests.
  $startInfo.Arguments = (($launchArgs | ForEach-Object {
    [string]::Concat([char]34,[string]$_,[char]34)
  }) -join ' ')
  $child = [System.Diagnostics.Process]::Start($startInfo)
  if ($null -eq $child) { throw "Could not launch Python 3 for PRIVATE source audit." }
  try {
    $childOutput = $child.StandardOutput.ReadToEnd()
    $childError = $child.StandardError.ReadToEnd()
    $child.WaitForExit()
    if ($childOutput) { Write-Host $childOutput }
    if ($childError) { Write-Warning $childError.Trim() }
    $pythonExitCode = $child.ExitCode
  } finally {
    $child.Dispose()
  }
  return $pythonExitCode
}
# AUX is a reserved Windows DOS device basename even with .csv extension.
# Label "aux" remains the same audit class, disk filename is auxiliary.csv.
function SessionCsvName([string]$label) {
  if ($label -eq "aux") { return "auxiliary.csv" }
  return $label + ".csv"
}

Write-Host "LOCAL / PRIVATE: Daytona original evidence capture. No uploads."
Write-Host "Always close the original emulator BEFORE pressing Enter. Each"
Write-Host "label needs a genuinely separate run; labels are user assertions only."
foreach ($label in $labels) {
  if ($ExistingSessionFolder) {
    $candidate = Join-Path $ExistingSessionFolder (SessionCsvName $label)
    if (!(Test-Path -LiteralPath $candidate -PathType Leaf)) {
      throw "Missing original session file: $(SessionCsvName $label) (source folder unchanged)."
    }
    $source = Get-Item -LiteralPath $candidate -ErrorAction Stop
  } else {
    $started = (Get-Date).ToUniversalTime().AddSeconds(-2)
    Write-Host ""
    Write-Host "[$label] Run a NEW Daytona emulator session in this stage."
    Write-Host "For title/menu/sky/hud/aux, visit the actual requested scene."
    Write-Host "Exit the emulator completely so the CSV is flushed."
    [void](Read-Host "Press Enter only after the original emulator exits")
    $source = Get-ChildItem -LiteralPath $logRoot -Filter "daytona_vr_startup_*.csv" -File |
      Where-Object { $_.LastWriteTimeUtc -ge $started } |
      Sort-Object LastWriteTimeUtc -Descending |
      Select-Object -First 1
    if ($null -eq $source) {
      throw "No NEW original x86 source CSV for $label. Relaunch the emulator and retry."
    }
  }
  $originalPath = [IO.Path]::GetFullPath($source.FullName)
  if (!$knownPaths.Add($originalPath)) {
    throw "Reused original source path for $label; seven independent sessions required."
  }
  if ($source.Length -lt 32) {
    throw "Empty or truncated original source evidence for $label."
  }
  $digest = (Get-FileHash -LiteralPath $originalPath -Algorithm SHA256).Hash
  if (!$knownDigests.Add($digest)) {
    throw "Duplicate original source bytes for $label; independent sessions required."
  }
  $target = Join-Path $destination (SessionCsvName $label)
  Copy-Item -LiteralPath $originalPath -Destination $target -ErrorAction Stop
  $copiedHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
  if ($copiedHash -ne $digest) { throw "Source copy changed during $label; close original game." }
  $copies[$label] = $target
  $hashes[$label] = $digest
  # Verify physical Draw/Scene/Present and run identity before prompting
  # for the next private original EXE session. Cache its complete private
  # report instead of repeating the expensive CSV parse at final triage.
  $preflight = Join-Path $destination ("PRIVATE_SOURCE_" + $label + ".json")
  $preflightExit = Invoke-PrivateAudit -auditArguments @(
    $analyzer,"--preflight",$target,"--output",$preflight)
  if ($preflightExit -ne 0 -or
      !(Test-Path -LiteralPath $preflight -PathType Leaf)) {
    throw "PRIVATE SESSION PREFLIGHT FAIL-CLOSED: $label original Draw/Present evidence invalid. Archive kept: $destination"
  }
  $sourceReport = Get-Content -LiteralPath $preflight -Raw -Encoding UTF8 |
    ConvertFrom-Json
  $nonce = [string]$sourceReport.original_capture_run.nonce_hex
  if ($nonce -notmatch '^0x[0-9a-f]{16}$' -or
      $nonce -eq "0x0000000000000000" -or
      !$knownNonces.Add($nonce)) {
    throw "PRIVATE SESSION PREFLIGHT FAIL-CLOSED: duplicate or missing original process nonce for $label. Archive kept: $destination"
  }
  $reports[$label] = $preflight
  Write-Host ("Validated and archived PRIVATE " + $label + " (" +
              $source.Length + " bytes; original process nonce distinct)")
}
$manifest = [ordered]@{
  schema = 1
  source = "user-labelled_original_daytona_x86_private_sessions"
  scene_labels_verified_by = "user_only_not_automatic"
  world_pass_proof = "NOT_ESTABLISHED"
  hmd_runtime_validation = "UNTESTED"
  output_directory = $destination
  source_sha256 = $hashes
}
$json = ConvertTo-Json -InputObject $manifest -Depth 5
[IO.File]::WriteAllText((Join-Path $destination "PRIVATE_MANIFEST.json"),
  $json + [Environment]::NewLine,(New-Object System.Text.UTF8Encoding($false)))
$argsForPython = @($analyzer,"--race",$reports["race_a"],"--race",$reports["race_b"])
foreach ($label in @("title","menu","sky","hud","aux")) {
  $argsForPython += @("--control",$label,$reports[$label])
}
$argsForPython += @("--output",(Join-Path $destination "PRIVATE_WORLD_CANDIDATES.json"))
$pythonExitCode = Invoke-PrivateAudit -auditArguments $argsForPython
if ($pythonExitCode -ne 0) {
  throw "Private source evidence FAIL-CLOSED. Archive kept for local inspection: $destination"
}
Write-Host ""
Write-Host "Local candidate triage written. A candidate NEVER authorizes World3D."
Write-Host "Independent original CPU camera, Quest3 pose and XR calibration remain unproven."
Write-Host "Private output: $destination"
