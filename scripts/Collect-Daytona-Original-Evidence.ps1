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
  Write-Host ("Archived PRIVATE " + $label + " (" + $source.Length + " bytes)")
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
$argsForPython = @($analyzer,"--race",$copies["race_a"],"--race",$copies["race_b"])
foreach ($label in @("title","menu","sky","hud","aux")) {
  $argsForPython += @("--control",$label,$copies[$label])
}
$argsForPython += @("--output",(Join-Path $destination "PRIVATE_WORLD_CANDIDATES.json"))
if (Get-Command py -ErrorAction SilentlyContinue) {
  & py -3 @argsForPython
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
  & python @argsForPython
} else {
  throw "Python 3 is required for local private source validation. Archive kept: $destination"
}
if ($LASTEXITCODE -ne 0) {
  throw "Private source evidence FAIL-CLOSED. Archive kept for local inspection: $destination"
}
Write-Host ""
Write-Host "Local candidate triage written. A candidate NEVER authorizes World3D."
Write-Host "Independent original CPU camera, Quest3 pose and XR calibration remain unproven."
Write-Host "Private output: $destination"
