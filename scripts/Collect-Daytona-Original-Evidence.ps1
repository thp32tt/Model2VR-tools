# Daytona original x86 source evidence collector (local/private ONLY).
# Seven USER-LABELLED, separate original emulator runs. No automatic scene
# classification, ROM inspection, game patching, network requests or uploads.
[CmdletBinding()]
param(
  [string]$PackageRoot = $PSScriptRoot,
  [string]$ExistingSessionFolder = "",
  [string]$ResumeSessionFolder = "",
  [ValidateRange(1,3600)][int]$AuditTimeoutSeconds = 900
)
$ErrorActionPreference = "Stop"
$labels = @("race_a","race_b","title","menu","sky","hud","aux")
$PackageRoot = [IO.Path]::GetFullPath($PackageRoot)
if ($ResumeSessionFolder -and $ExistingSessionFolder) {
  throw "Choose either -ResumeSessionFolder or -ExistingSessionFolder, not both."
}
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
if ($ResumeSessionFolder) {
  $destination = [IO.Path]::GetFullPath($ResumeSessionFolder)
  if (!(Test-Path -LiteralPath $destination -PathType Container) -or
      !([IO.Path]::GetFileName($destination) -like "private_original_evidence_*")) {
    throw "PRIVATE SESSION RESUME FAIL-CLOSED: expected a saved original evidence archive."
  }
} else {
  $folder = "private_original_evidence_" + (Get-Date).ToUniversalTime().ToString("yyyyMMdd_HHmmss_fff")
  $destination = Join-Path $logRoot $folder
  New-Item -ItemType Directory -Path $destination -ErrorAction Stop | Out-Null
}
# Invalidate a previous successful receipt BEFORE reading or prompting for
# another original session. If stage 1 or a later preflight fails, an old
# manifest/candidate must never remain under its active success filename.
# Keep prior files for local forensics; do not delete private evidence.
if ($ResumeSessionFolder) {
  foreach ($oldSummary in @("PRIVATE_MANIFEST.json","PRIVATE_WORLD_CANDIDATES.json")) {
    $oldPath = Join-Path $destination $oldSummary
    if (Test-Path -LiteralPath $oldPath -PathType Leaf) {
      $suffix = (Get-Date).ToUniversalTime().ToString("yyyyMMdd_HHmmss_fff") +
                "_" + [guid]::NewGuid().ToString("N").Substring(0,8)
      Rename-Item -LiteralPath $oldPath -NewName ("PRIVATE_PRIOR_" + $suffix + "_" + $oldSummary) -ErrorAction Stop
    }
  }
}
$copies = @{}
$hashes = @{}
$knownPaths = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$knownDigests = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$knownNonces = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
$reports = @{}
$reportHashes = @{}
$noncesByLabel = @{}
# Resolve Python before asking the user to run seven original x86 games.
# Prefer the actual Python 3 interpreter. On GitHub Windows runners,
# 'py' can resolve to a Python executable/shim that does not accept -3.
# Fall back to the official launcher only on systems without python.exe.
$pythonLauncher = Get-Command python -ErrorAction SilentlyContinue
$pythonPrefix = @()
if ($null -eq $pythonLauncher) {
  $pythonLauncher = Get-Command py -ErrorAction SilentlyContinue
  if ($null -ne $pythonLauncher -and
      [IO.Path]::GetFileNameWithoutExtension($pythonLauncher.Source) -ieq "py") {
    $pythonPrefix = @("-3")
  }
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
    # Drain stdout and stderr CONCURRENTLY. Reading one to completion
    # before the other can deadlock when Python fills the other pipe.
    $stdoutTask = $child.StandardOutput.ReadToEndAsync()
    $stderrTask = $child.StandardError.ReadToEndAsync()
    # Python may hang on a malformed/huge original CSV. Never trap the
    # operator indefinitely; no manifest or next session on timeout.
    if (!$child.WaitForExit($AuditTimeoutSeconds * 1000)) {
      try { $child.Kill() } catch { }
      [void]$child.WaitForExit(5000)
      throw "PRIVATE AUDIT TIMEOUT FAIL-CLOSED: Python audit exceeded $AuditTimeoutSeconds seconds. Private archive kept."
    }
    $child.WaitForExit()
    $childOutput = $stdoutTask.GetAwaiter().GetResult()
    $childError = $stderrTask.GetAwaiter().GetResult()
    if ($childOutput) { Write-Host $childOutput }
    if ($childError) {
      # Avoid flooding the interactive Windows terminal or exposing
      # unbounded original-game diagnostic text on audit errors.
      $diagnostic = $childError.Trim()
      if ($diagnostic.Length -gt 2048) {
        $diagnostic = $diagnostic.Substring(0,2048) + " ... [truncated PRIVATE audit stderr]"
      }
      Write-Warning $diagnostic
    }
    $pythonExitCode = $child.ExitCode
  } finally {
    if (!$child.HasExited) {
      try { $child.Kill() } catch { }
    }
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
# Never accept a previously cached PRIVATE_SOURCE report as the result of
# a fresh CSV preflight. Only a uniquely named pending output produced by
# this invocation may become the active report after the audit succeeds.
# A per-stage original x86 raw CSV can change WHILE the Python physical
# Draw/Scene/Present auditor reads it. Checking only after all seven runs
# wastes the remaining operator sessions and briefly promotes an audit
# whose exact source bytes were never pinned. Refuse before promoting a
# PRIVATE_SOURCE JSON or requesting the next original game session.
function Assert-PrivatePreflightCsvUnchanged([string]$label,
                                            [string]$csvPath,
                                            [string]$expectedSha256) {
  try {
    $afterAuditSha256 = (Get-FileHash -LiteralPath $csvPath -Algorithm SHA256 -ErrorAction Stop).Hash
  } catch {
    throw "PRIVATE SESSION PREFLIGHT SOURCE MUTATION FAIL-CLOSED: unreadable original CSV for $label; local archive preserved."
  }
  if ($afterAuditSha256 -cne $expectedSha256) {
    throw "PRIVATE SESSION PREFLIGHT SOURCE MUTATION FAIL-CLOSED: original x86 CSV changed during $label audit; local archive preserved."
  }
}
function New-PendingPrivateSourcePath([string]$label) {
  return Join-Path $destination ("PRIVATE_PENDING_SOURCE_" + $label + "_" +
    [guid]::NewGuid().ToString("N") + ".json")
}

Write-Host "LOCAL / PRIVATE: Daytona original evidence capture. No uploads."
Write-Host "Always close the original emulator BEFORE pressing Enter. Each"
Write-Host "label needs a genuinely separate run; labels are user assertions only."
foreach ($label in $labels) {
  $target = Join-Path $destination (SessionCsvName $label)
  $preflight = Join-Path $destination ("PRIVATE_SOURCE_" + $label + ".json")
  if ($ResumeSessionFolder) {
    if (Test-Path -LiteralPath $target -PathType Leaf) {
      # Resume must re-audit actual archived original x86 CSV, not
      # trust cached JSON or user-labelled source names.
      $resumeSource = Get-Item -LiteralPath $target -ErrorAction Stop
      $resumeHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
      # Invalidate the prior cached source BEFORE invoking Python. A
      # silent zero-exit or an audit timeout cannot reuse yesterday's JSON.
      # Preserve the old report under an unmistakably inactive name.
      if (Test-Path -LiteralPath $preflight -PathType Leaf) {
        $staleName = "PRIVATE_STALE_SOURCE_" + $label + "_" +
          [guid]::NewGuid().ToString("N") + ".json"
        Rename-Item -LiteralPath $preflight -NewName $staleName -ErrorAction Stop
      }
      $pendingPreflight = New-PendingPrivateSourcePath $label
      $resumeExit = Invoke-PrivateAudit -auditArguments @(
        $analyzer,"--preflight",$target,"--output",$pendingPreflight)
      Assert-PrivatePreflightCsvUnchanged $label $target $resumeHash
      if ($resumeSource.Length -ge 32 -and $resumeExit -eq 0 -and
          (Test-Path -LiteralPath $pendingPreflight -PathType Leaf)) {
        $resumeReport = Get-Content -LiteralPath $pendingPreflight -Raw -Encoding UTF8 |
          ConvertFrom-Json
        $resumeNonce = [string]$resumeReport.original_capture_run.nonce_hex
        if ($resumeNonce -cnotmatch '^0x[0-9a-f]{16}$' -or
            $resumeNonce -ceq "0x0000000000000000" -or
            !$knownPaths.Add([IO.Path]::GetFullPath($target)) -or
            !$knownDigests.Add($resumeHash) -or
            !$knownNonces.Add($resumeNonce)) {
          throw "PRIVATE SESSION RESUME FAIL-CLOSED: duplicate or missing original source identity for $label."
        }
        # Move the newly audited file into the active slot only AFTER
        # validating the original physical source process identity.
        $noncesByLabel[$label] = $resumeNonce
        Move-Item -LiteralPath $pendingPreflight -Destination $preflight -ErrorAction Stop
        $copies[$label] = $target
        $hashes[$label] = $resumeHash
        $reports[$label] = $preflight
        $reportHashes[$label] = (Get-FileHash -LiteralPath $preflight -Algorithm SHA256).Hash
        Write-Host ("Resumed original PRIVATE " + $label + " (fresh physical CSV audit PASS; no game rerun)")
        continue
      }
      # Preserve incomplete or invalid diagnostic CSV/JSON and ask for
      # a fresh original game run only at THIS stage.
      Write-Warning "Archived $label source failed resume preflight; new original run required."
      foreach ($failed in @($target,$preflight)) {
        if (Test-Path -LiteralPath $failed -PathType Leaf) {
          $suffix = (Get-Date).ToUniversalTime().ToString("yyyyMMdd_HHmmss_fff")
          $failedName = "PRIVATE_FAILED_" + $label + "_" + $suffix + "_" +
                        [IO.Path]::GetFileName($failed)
          Rename-Item -LiteralPath $failed -NewName $failedName -ErrorAction Stop
        }
      }
    } elseif (Test-Path -LiteralPath $preflight -PathType Leaf) {
      throw "PRIVATE SESSION RESUME FAIL-CLOSED: orphaned JSON without original x86 CSV for $label."
    }
  }
  if ($ExistingSessionFolder) {
    $candidate = Join-Path $ExistingSessionFolder (SessionCsvName $label)
    if (!(Test-Path -LiteralPath $candidate -PathType Leaf)) {
      throw "Missing original session file: $(SessionCsvName $label) (source folder unchanged)."
    }
    $source = Get-Item -LiteralPath $candidate -ErrorAction Stop
  } else {
    # A timestamp alone cannot prove a new original emulator process:
    # existing logs can be retouched, and the newest of two new logs can be
    # an unrelated/crashed process. Snapshot path identity before prompting.
    $stageStarted = (Get-Date).ToUniversalTime()
    $previousCsvPaths = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    Get-ChildItem -LiteralPath $logRoot -Filter "daytona_vr_startup_*.csv" -File |
      ForEach-Object {
        [void]$previousCsvPaths.Add([IO.Path]::GetFullPath($_.FullName))
      }
    Write-Host ""
    Write-Host "[$label] Run exactly ONE NEW Daytona emulator session in this stage."
    Write-Host "For title/menu/sky/hud/aux, visit the actual requested scene."
    Write-Host "Exit the emulator completely so the CSV is flushed."
    [void](Read-Host "Press Enter only after the original emulator exits")
    $freshSources = @(Get-ChildItem -LiteralPath $logRoot -Filter "daytona_vr_startup_*.csv" -File |
      Where-Object {
        $_.LastWriteTimeUtc -ge $stageStarted -and
        !$previousCsvPaths.Contains([IO.Path]::GetFullPath($_.FullName))
      })
    if ($freshSources.Count -eq 0) {
      throw "No NEW original x86 source CSV for $label. Relaunch the emulator and retry."
    }
    if ($freshSources.Count -ne 1) {
      throw "PRIVATE SESSION CAPTURE AMBIGUOUS: $($freshSources.Count) new x86 logs for $label; expected exactly one separate original emulator process."
    }
    $source = $freshSources[0]
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
  Copy-Item -LiteralPath $originalPath -Destination $target -ErrorAction Stop
  $copiedHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
  if ($copiedHash -ne $digest) { throw "Source copy changed during $label; close original game." }
  $copies[$label] = $target
  $hashes[$label] = $digest
  # Verify physical Draw/Scene/Present and run identity before prompting
  # for the next private original EXE session. Cache its complete private
  # report instead of repeating the expensive CSV parse at final triage.
  $pendingPreflight = New-PendingPrivateSourcePath $label
  $preflightExit = Invoke-PrivateAudit -auditArguments @(
    $analyzer,"--preflight",$target,"--output",$pendingPreflight)
  Assert-PrivatePreflightCsvUnchanged $label $target $digest
  if ($preflightExit -ne 0 -or
      !(Test-Path -LiteralPath $pendingPreflight -PathType Leaf)) {
    throw "PRIVATE SESSION PREFLIGHT FAIL-CLOSED: $label original Draw/Present evidence invalid. Archive kept: $destination"
  }
  $sourceReport = Get-Content -LiteralPath $pendingPreflight -Raw -Encoding UTF8 |
    ConvertFrom-Json
  $nonce = [string]$sourceReport.original_capture_run.nonce_hex
  if ($nonce -notmatch '^0x[0-9a-f]{16}$' -or
      $nonce -eq "0x0000000000000000" -or
      !$knownNonces.Add($nonce)) {
    throw "PRIVATE SESSION PREFLIGHT FAIL-CLOSED: duplicate or missing original process nonce for $label. Archive kept: $destination"
  }
  $noncesByLabel[$label] = $nonce
  Move-Item -LiteralPath $pendingPreflight -Destination $preflight -ErrorAction Stop
  $reports[$label] = $preflight
  $reportHashes[$label] = (Get-FileHash -LiteralPath $preflight -Algorithm SHA256).Hash
  Write-Host ("Validated and archived PRIVATE " + $label + " (" +
              $source.Length + " bytes; original process nonce distinct)")
}
# A preflighted stage may sit on disk while the user completes six other
# genuine emulator runs. Cached JSON is only trustworthy for the *same*
# original raw CSV and report bytes that were independently audited.
# Rehash all seven before comparison and again before publishing success.
function Assert-PrivateArchiveIntegrity {
  foreach ($auditedLabel in $labels) {
    $csvPath = $copies[$auditedLabel]
    $reportPath = $reports[$auditedLabel]
    if (!$csvPath -or !$reportPath -or
        !(Test-Path -LiteralPath $csvPath -PathType Leaf) -or
        !(Test-Path -LiteralPath $reportPath -PathType Leaf)) {
      throw "PRIVATE SESSION INTEGRITY FAIL-CLOSED: missing archived CSV or audit for $auditedLabel."
    }
    try {
      $csvHash = (Get-FileHash -LiteralPath $csvPath -Algorithm SHA256).Hash
      $reportHash = (Get-FileHash -LiteralPath $reportPath -Algorithm SHA256).Hash
    } catch {
      throw "PRIVATE SESSION INTEGRITY FAIL-CLOSED: unreadable archived evidence for $auditedLabel."
    }
    if ($csvHash -cne $hashes[$auditedLabel] -or
        $reportHash -cne $reportHashes[$auditedLabel]) {
      throw "PRIVATE SESSION INTEGRITY FAIL-CLOSED: original CSV/audit changed after physical preflight for $auditedLabel."
    }
  }
}
Assert-PrivateArchiveIntegrity
$argsForPython = @($analyzer,"--race",$reports["race_a"],"--race",$reports["race_b"])
foreach ($label in @("title","menu","sky","hud","aux")) {
  $argsForPython += @("--control",$label,$reports[$label])
}
# Never let the external analyzer write an active candidate directly. A
# zero-exit Python process may produce no JSON, truncated JSON, or output
# that spuriously claims World3D/Quest validation. Validate a GUID-unique
# pending receipt before any visible success filenames are published.
$candidateOutput = Join-Path $destination "PRIVATE_WORLD_CANDIDATES.json"
$pendingCandidate = Join-Path $destination ("PRIVATE_PENDING_COMPARE_" +
  [guid]::NewGuid().ToString("N") + ".json")
$argsForPython += @("--output",$pendingCandidate)
function Move-PrivateFailedCandidate([string]$path) {
  if (Test-Path -LiteralPath $path -PathType Leaf) {
    $failedName = "PRIVATE_FAILED_COMPARE_" +
                  (Get-Date).ToUniversalTime().ToString("yyyyMMdd_HHmmss_fff") +
                  "_" + [guid]::NewGuid().ToString("N").Substring(0,8) +
                  "_PRIVATE_WORLD_CANDIDATES.json"
    Rename-Item -LiteralPath $path -NewName $failedName -ErrorAction Stop
  }
}
$pythonExitCode = -1
$comparisonError = $null
try {
  $pythonExitCode = Invoke-PrivateAudit -auditArguments $argsForPython
} catch {
  # Even a timed-out or failed-to-launch comparator may have already
  # written a partial pending candidate. Always reach quarantine.
  $comparisonError = $_.Exception.Message
}
# The external comparison process (or another local writer) may change
# cached PRIVATE inputs while the seven-session report is being assembled.
$integrityError = $null
try { Assert-PrivateArchiveIntegrity } catch { $integrityError = $_.Exception.Message }
$receiptError = $null
if ($pythonExitCode -eq 0 -and $null -eq $comparisonError -and
    $null -eq $integrityError -and
    (Test-Path -LiteralPath $pendingCandidate -PathType Leaf)) {
  try {
    $candidate = Get-Content -LiteralPath $pendingCandidate -Raw -Encoding UTF8 |
      ConvertFrom-Json -ErrorAction Stop
    $controls = @($candidate.control_types)
    if ($candidate.schema -ne 1 -or
        $candidate.source_kind -cne "private_cross_phase_original_draw_candidate_triage" -or
        $candidate.race_session_count -ne 2 -or
        $candidate.nonrace_session_count -ne 5 -or
        $controls.Count -ne 5 -or
        ($controls -join ",") -cne "aux,hud,menu,sky,title" -or
        $candidate.complete_negative_controls -isnot [bool] -or
        $candidate.complete_negative_controls -ne $true -or
        $candidate.world_pass_proof -cne "NOT_ESTABLISHED" -or
        $candidate.camera_axes_and_meters_per_unit -cne "NOT_CALIBRATED" -or
        $candidate.adapter_auto_enable -isnot [bool] -or
        $candidate.adapter_auto_enable -ne $false -or
        $candidate.hmd_runtime_validation -cne "UNTESTED" -or
        $candidate.candidates -isnot [array] -or
        $candidate.rejected -isnot [array]) {
      throw "Invalid original PRIVATE candidate-only source receipt."
    }
    foreach ($entry in $candidate.candidates) {
      if ($null -eq $entry -or
          $entry.world_pass_authorized -isnot [bool] -or
          $entry.world_pass_authorized -ne $false) {
        throw "Original PRIVATE candidate attempted World3D authorization."
      }
    }
  } catch {
    $receiptError = $_.Exception.Message
  }
}
if ($pythonExitCode -ne 0 -or $null -ne $comparisonError -or
    $null -ne $integrityError -or $null -ne $receiptError -or
    !(Test-Path -LiteralPath $pendingCandidate -PathType Leaf)) {
  Move-PrivateFailedCandidate $pendingCandidate
  # No active result from this run may survive, including an unexpected
  # competing writer in a resumed local archive.
  Move-PrivateFailedCandidate $candidateOutput
  if ($null -ne $integrityError) { throw $integrityError }
  if ($null -ne $comparisonError) {
    throw "PRIVATE SESSION COMPARE FAIL-CLOSED: $comparisonError Archive kept for local inspection: $destination"
  }
  if ($null -ne $receiptError) {
    throw "PRIVATE SESSION COMPARE RECEIPT INVALID FAIL-CLOSED: $receiptError Archive kept: $destination"
  }
  throw "Private source evidence FAIL-CLOSED. Archive kept for local inspection: $destination"
}
# Publish the final receipt only after a complete validated seven-session
# comparison; the manifest is the LAST active success filename. Stage its
# bytes separately so a crash during serialization leaves no partial receipt.
$manifest = [ordered]@{
  schema = 1
  source = "user-labelled_original_daytona_x86_private_sessions"
  scene_labels_verified_by = "user_only_not_automatic"
  world_pass_proof = "NOT_ESTABLISHED"
  hmd_runtime_validation = "UNTESTED"
  output_directory = $destination
  source_sha256 = $hashes
  # Bind the exact independently preflighted report bytes and physical
  # process identity, not only the original x86 CSV input hashes.
  # A PRIVATE receipt verifier can reject later cached JSON substitution.
  source_report_sha256 = $reportHashes
  original_process_nonce_by_stage = $noncesByLabel
}
$pendingManifest = Join-Path $destination ("PRIVATE_PENDING_MANIFEST_" +
  [guid]::NewGuid().ToString("N") + ".json")
$finalManifest = Join-Path $destination "PRIVATE_MANIFEST.json"
try {
  Assert-PrivateArchiveIntegrity
  $manifest.candidate_sha256 = (Get-FileHash -LiteralPath $pendingCandidate -Algorithm SHA256).Hash
  $json = ConvertTo-Json -InputObject $manifest -Depth 5
  [IO.File]::WriteAllText($pendingManifest,
    $json + [Environment]::NewLine,(New-Object System.Text.UTF8Encoding($false)))
  Move-Item -LiteralPath $pendingCandidate -Destination $candidateOutput -ErrorAction Stop
  Move-Item -LiteralPath $pendingManifest -Destination $finalManifest -ErrorAction Stop
} catch {
  Move-PrivateFailedCandidate $pendingCandidate
  Move-PrivateFailedCandidate $candidateOutput
  if (Test-Path -LiteralPath $pendingManifest -PathType Leaf) {
    Rename-Item -LiteralPath $pendingManifest -NewName (
      "PRIVATE_FAILED_MANIFEST_" + [guid]::NewGuid().ToString("N") + ".json") -ErrorAction Stop
  }
  throw "PRIVATE SESSION PUBLICATION FAIL-CLOSED: no successful candidate/manifest receipt. Archive kept: $destination"
}
Write-Host ""
Write-Host "Local candidate triage written. A candidate NEVER authorizes World3D."
Write-Host "Independent original CPU camera, Quest3 pose and XR calibration remain unproven."
Write-Host "Private output: $destination"
