"""Original Daytona seven-session evidence capture stays in the USER release.

Only synthetic/meta source assertions are checked in public CI. The game
content and private original EXE/runtime/scene CSV are never published.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]

class PrivateOriginalCaptureReleaseTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ps=(ROOT/"scripts/Collect-Daytona-Original-Evidence.ps1").read_text("utf-8")
        cls.cmd=(ROOT/"scripts/Collect-Daytona-Original-Evidence.cmd").read_text("utf-8")
        cls.workflow=(ROOT/".github/workflows/daytona-native-stereo.yml").read_text("utf-8")
        cls.docs=(ROOT/"docs/DAYTONA_NATIVE_STEREO.md").read_text("utf-8")


    def test_original_private_stage_checkpoint_is_incomplete_and_resumable(self):
        # Explicit one-stage handoff avoids seven consecutive original-game
        # runs. Every stage still requires local x86 CSV+nonce+fresh preflight.
        # Partial archives NEVER become World3D proof or a final receipt.
        self.assertIn('[string]$StopAfterStage = ""', self.ps)
        self.assertIn('$StopAfterStage -cnotin $labels', self.ps)
        self.assertIn('$partialCaptureStopped = $false', self.ps)
        self.assertIn('if ($StopAfterStage -ceq $label -and $label -cne "aux")', self.ps)
        self.assertEqual(self.ps.count('if ($StopAfterStage -ceq $label -and $label -cne "aux")'), 2)
        self.assertIn('PRIVATE CHECKPOINT SAVED - INCOMPLETE', self.ps)
        self.assertIn('CPU World3D stays AUTO-OFF', self.ps)
        self.assertLess(self.ps.index('if ($partialCaptureStopped) {'),
                        self.ps.index('Assert-PrivateArchiveIntegrity\n$argsForPython'))
        self.assertIn('Smoke test resumable single-stage PRIVATE original capture checkpoint', self.workflow)
        self.assertIn('StopAfterStage', self.cmd + self.ps)
        self.assertIn('-StopAfterStage', self.docs)

    def test_optional_race_readiness_is_only_for_real_race_capture_and_resume(self):
        # A readable genuine CSV with no same-Draw CPU/GPU world witnesses
        # should be rejected BEFORE spending five additional original game
        # sessions, if the operator opts in. No auto-authorization.
        self.assertIn('[switch]$RequireRaceSource', self.ps)
        self.assertEqual(self.ps.count('if ($RequireRaceSource -and'), 2)
        self.assertIn('$resumeArgs += "--require-race-source"', self.ps)
        self.assertIn('$preflightArgs += "--require-race-source"', self.ps)
        self.assertIn('$label -ceq "race_a" -or $label -ceq "race_b"', self.ps)
        self.assertIn('-RequireRaceSource', self.docs)
        self.assertIn('CPU World3D', self.docs)
        self.assertNotIn('world_pass_proof = "ESTABLISHED"', self.ps)

    def test_original_exe_is_never_patched_or_uploaded(self):
        for forbidden in (
            "Invoke-WebRequest", "Invoke-RestMethod", "Start-BitsTransfer",
            "New-PSSession", "Copy-Item -LiteralPath $PackageRoot",
            "Set-Content -LiteralPath $originalPath", "Remove-Item",
            "github.com", "git push", "World3D=true", "world_pass_proof = \"ESTABLISHED\"",
        ):
            self.assertNotIn(forbidden,self.ps)
        self.assertIn("$sha.ComputeHash($reader)",self.ps)
        self.assertIn("Get-FileHash -LiteralPath $pendingCopy -Algorithm SHA256",self.ps)
        self.assertIn("scene_labels_verified_by",self.ps)
        self.assertIn('"NOT_ESTABLISHED"',self.ps)
        self.assertIn('"UNTESTED"',self.ps)
        self.assertIn("$reader.CopyTo($archiveWriter)",self.ps)
        self.assertIn("$capture = Copy-ClosedOriginalSourceUnderLease $originalPath $target",self.ps)

    def test_original_private_archive_is_single_writer_before_receipt_revocation(self):
        # Two terminals may resume the same seven-stage archive. The
        # exclusive Windows file handle must precede ALL active-file moves.
        self.assertIn('$sessionLockPath = Join-Path $destination ".private_original_collection.lock"', self.ps)
        self.assertIn('[System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None', self.ps)
        self.assertIn('PRIVATE SESSION LOCK FAIL-CLOSED', self.ps)
        self.assertLess(self.ps.index('$sessionLockStream = [System.IO.FileStream]::new('),
                        self.ps.index('Rename-Item -LiteralPath $oldPath'))
        self.assertIn('$sessionLockStream.Dispose()', self.ps)
        self.assertIn('Reject concurrent original private archive collector before receipt revocation', self.workflow)
        self.assertIn('Where-Object { $_.Name -ne ".private_original_collection.lock" }', self.workflow)
        self.assertIn('-File |\n              Where-Object { $_.Name -ne ".private_original_collection.lock" }', self.workflow)

    def test_seven_distinct_original_process_sessions_and_bounded_analysis(self):
        self.assertIn('("race_a","race_b","title","menu","sky","hud","aux")',self.ps)
        self.assertIn('Get-ChildItem -LiteralPath $logRoot -Filter "daytona_vr_startup_*.csv"',self.ps)
        self.assertIn('$knownPaths.Add($originalPath)',self.ps)
        self.assertIn('$knownDigests.Add($digest)',self.ps)
        self.assertIn('$_.LastWriteTimeUtc -ge $stageStarted -and',self.ps)
        self.assertIn('model2_world_pass_evidence.py',self.ps)
        self.assertIn('model2_draw_source_report.py',self.ps)
        self.assertIn('"--race",$reports["race_a"],"--race",$reports["race_b"]',self.ps)
        self.assertIn('"--preflight",$target',self.ps)
        self.assertIn('$knownNonces.Add($nonce)',self.ps)
        self.assertIn('PRIVATE SESSION PREFLIGHT FAIL-CLOSED',self.ps)
        self.assertIn('PRIVATE_SOURCE_',self.ps)
        self.assertIn('Invoke-PrivateAudit -auditArguments $argsForPython',self.ps)
        self.assertIn('@("--control",$label,$reports[$label])',self.ps)
        self.assertIn('if ($pythonExitCode -ne 0 -or',self.ps)
        self.assertIn('System.Diagnostics.ProcessStartInfo',self.ps)
        self.assertIn('System.Diagnostics.Process]::Start',self.ps)
        self.assertIn('$child.ExitCode',self.ps)
        self.assertIn('PRIVATE_WORLD_CANDIDATES.json',self.ps)
        self.assertIn('PRIVATE_MANIFEST.json',self.ps)
        self.assertIn('return "auxiliary.csv"',self.ps)
        self.assertIn('SessionCsvName $label',self.ps)
        self.assertIn('-ExistingSessionFolder',self.docs)
        self.assertIn('-ResumeSessionFolder',self.docs)
        self.assertIn('$resumeExit = Invoke-PrivateAudit -auditArguments',self.ps)
        self.assertIn('PRIVATE SESSION RESUME FAIL-CLOSED',self.ps)
        self.assertIn('PRIVATE_FAILED_',self.ps)
        self.assertIn('Rename-Item -LiteralPath $failed',self.ps)
        self.assertIn('no game rerun',self.ps)
        self.assertGreater(self.ps.rfind('PRIVATE_MANIFEST.json'),
                           self.ps.index('$pythonExitCode = Invoke-PrivateAudit -auditArguments $argsForPython'))

    def test_original_manifest_binds_each_stage_report_hash_and_process_nonce(self):
        # The final local-only receipt must bind both x86 raw CSV and
        # exactly audited JSON, plus all independently nonced processes.
        self.assertIn('$noncesByLabel = @{}', self.ps)
        self.assertIn('$noncesByLabel[$label] = $resumeNonce', self.ps)
        self.assertIn('$noncesByLabel[$label] = $nonce', self.ps)
        self.assertIn('source_report_sha256 = $reportHashes', self.ps)
        self.assertIn('original_process_nonce_by_stage = $noncesByLabel', self.ps)
        self.assertLess(self.ps.index('Assert-PrivateArchiveIntegrity\n  $manifest.candidate_sha256'),
                        self.ps.index('Move-Item -LiteralPath $pendingManifest -Destination $finalManifest'))
        self.assertIn('verify_daytona_private_receipt.py', self.workflow)
        self.assertIn('PRIVATE RECEIPT BYTE-INTEGRITY PASS', (
            ROOT/"tools/verify_daytona_private_receipt.py").read_text("utf-8"))
        self.assertIn('Copy-Item tools/verify_daytona_private_receipt.py stage/', self.workflow)
        self.assertIn('if($verifierProcess.ExitCode -eq 0)', self.workflow)
        self.assertIn('PRIVATE original seven-stage receipt integrity check',
                      self.docs)

    def test_resumed_success_receipts_are_revoked_before_any_stage_preflight(self):
        # An archive may have become corrupt after an earlier successful run.
        # Old receipts cannot survive a resume that fails on its first stage.
        self.assertIn('PRIVATE_PRIOR_', self.ps)
        self.assertEqual(self.ps.count('PRIVATE_PRIOR_'), 1)
        self.assertLess(self.ps.index('PRIVATE_PRIOR_'),
                        self.ps.index('foreach ($label in $labels)'))
        self.assertIn('Rename-Item -LiteralPath $oldPath', self.ps)
        self.assertLess(self.ps.index('Rename-Item -LiteralPath $oldPath'),
                        self.ps.index('$resumeExit = Invoke-PrivateAudit'))
        self.assertIn('[guid]::NewGuid().ToString("N")', self.ps)
        self.assertIn('PRIVATE_PRIOR_', self.workflow)
        self.assertIn('Old manifest survived failed resume', self.workflow)
        self.assertIn('PRIVATE_FAILED_COMPARE_', self.ps)
        self.assertLess(self.ps.index('PRIVATE_FAILED_COMPARE_'),
                        self.ps.index('Local candidate triage written'))
        self.assertIn('Move-PrivateFailedCandidate $candidateOutput', self.ps)
        self.assertIn('$pythonExitCode -ne 0', self.ps)

    def test_original_x86_source_lease_covers_hash_copy_and_atomic_promotion(self):
        # A short closed-producer probe could not prevent a second Win32
        # process reopening the original logger between hash and Copy-Item.
        # The source is now read through one long-lived, read-only handle.
        start = self.ps.index("function Copy-ClosedOriginalSourceUnderLease")
        end = self.ps.index("# Never accept a previously cached", start)
        function = self.ps[start:end]
        self.assertIn("[System.IO.FileShare]::Read", function)
        self.assertIn("[System.IO.FileMode]::Open", function)
        self.assertIn("PRIVATE SESSION ORIGINAL SOURCE STILL OPEN FAIL-CLOSED", function)
        self.assertIn("$sha.ComputeHash($reader)", function)
        self.assertIn("$reader.Position = 0", function)
        self.assertIn("$reader.CopyTo($archiveWriter)", function)
        self.assertIn("$archiveWriter.Flush($true)", function)
        self.assertIn("[System.IO.FileMode]::CreateNew", function)
        self.assertIn("Move-Item -LiteralPath $pendingCopy -Destination $destinationPath", function)
        self.assertIn("$reader.Dispose()", function)
        self.assertLess(function.index("$sha.ComputeHash($reader)"),
                        function.index("$reader.CopyTo($archiveWriter)"))
        self.assertLess(function.index("$reader.CopyTo($archiveWriter)"),
                        function.index("Move-Item -LiteralPath $pendingCopy"))
        self.assertLess(function.index("Move-Item -LiteralPath $pendingCopy"),
                        function.index("$reader.Dispose()"))
        self.assertEqual(self.ps.count(
            "$capture = Copy-ClosedOriginalSourceUnderLease $originalPath $target"), 1)
        self.assertNotIn("Assert-OriginalProbeExited", self.ps)
        self.assertIn(
            "Reject original x86 CSV while emulator writer is alive",
            self.workflow)
        self.assertIn(
            "Verify original source capture under continuous read lease",
            self.workflow)

    def test_fresh_original_stage_only_accepts_one_new_path_not_a_recent_old_file(self):
        # A previously written CSV can have its LastWriteTime retouched or
        # dated in the future. Its identity must be captured BEFORE Read-Host.
        self.assertIn('$previousCsvPaths = New-Object', self.ps)
        self.assertIn('$previousCsvPaths.Add([IO.Path]::GetFullPath($_.FullName))',
                      self.ps)
        self.assertLess(self.ps.index('$previousCsvPaths.Add'),
                        self.ps.index('[void](Read-Host'))
        self.assertIn('!$previousCsvPaths.Contains([IO.Path]::GetFullPath($_.FullName))',
                      self.ps)
        self.assertIn('if ($freshSources.Count -eq 0)', self.ps)
        self.assertIn('if ($freshSources.Count -ne 1)', self.ps)
        self.assertIn('PRIVATE SESSION CAPTURE AMBIGUOUS', self.ps)
        self.assertNotIn('Select-Object -First 1', self.ps)
        self.assertIn('Reject previously existing original x86 CSV even if retouched',
                      self.workflow)

    def test_cached_original_csv_and_audit_are_pinned_until_publication(self):
        # In-place edits after preflight must invalidate the raw-source
        # receipt before comparing and again after the external analyzer.
        self.assertIn("$reportHashes = @{}", self.ps)
        self.assertIn("$reportHashes[$label] = (Get-FileHash -LiteralPath $preflight", self.ps)
        self.assertEqual(self.ps.count("$reportHashes[$label] ="), 2)
        self.assertIn("function Assert-PrivateArchiveIntegrity", self.ps)
        self.assertIn("Get-FileHash -LiteralPath $csvPath -Algorithm SHA256", self.ps)
        self.assertIn("Get-FileHash -LiteralPath $reportPath -Algorithm SHA256", self.ps)
        self.assertIn("$csvHash -cne $hashes[$auditedLabel]", self.ps)
        self.assertIn("$reportHash -cne $reportHashes[$auditedLabel]", self.ps)
        self.assertEqual(self.ps.count("Assert-PrivateArchiveIntegrity"), 4)
        self.assertLess(self.ps.index("Assert-PrivateArchiveIntegrity\n$argsForPython"),
                        self.ps.index("$pythonExitCode = Invoke-PrivateAudit"))
        self.assertGreater(self.ps.index("try { Assert-PrivateArchiveIntegrity }"),
                           self.ps.index("$pythonExitCode = Invoke-PrivateAudit"))
        self.assertIn("if ($null -ne $integrityError) { throw $integrityError }", self.ps)
        self.assertIn("Smoke test archive mutation after per-stage physical preflight", self.workflow)
        self.assertIn("PRIVATE SESSION INTEGRITY FAIL-CLOSED", self.workflow)
        self.assertIn("PRIVATE_FAILED_COMPARE_", self.workflow)
        self.assertIn('GetFileNameWithoutExtension($pythonLauncher.Source) -ieq "py"', self.ps)
        self.assertLess(self.ps.index('Get-Command python -ErrorAction SilentlyContinue'),
                        self.ps.index('Get-Command py -ErrorAction SilentlyContinue'))

    def test_private_audit_child_drains_two_pipes_and_has_bounded_lifetime(self):
        # A verbose failing original-game audit must not deadlock the
        # collector before it can reject the stage and keep its archive.
        self.assertIn("[ValidateRange(1,3600)][int]$AuditTimeoutSeconds = 900", self.ps)
        self.assertIn("$child.StandardOutput.ReadToEndAsync()", self.ps)
        self.assertIn("$child.StandardError.ReadToEndAsync()", self.ps)
        self.assertIn("$child.WaitForExit($AuditTimeoutSeconds * 1000)", self.ps)
        self.assertIn('PRIVATE AUDIT TIMEOUT FAIL-CLOSED', self.ps)
        self.assertIn('$child.Kill()', self.ps)
        self.assertIn('truncated PRIVATE audit stderr', self.ps)
        self.assertNotIn('StandardOutput.ReadToEnd()', self.ps)
        self.assertNotIn('StandardError.ReadToEnd()', self.ps)
        self.assertIn('Reject blocked or flooded original private Python auditors', self.workflow)

    def test_new_preflight_report_is_never_a_cached_success(self):
        # Both newly captured and resumed original CSVs need their own
        # unique pending JSON; no old PRIVATE_SOURCE JSON can masquerade
        # as a fresh Python auditor result after timeout or silent exit 0.
        self.assertIn("function New-PendingPrivateSourcePath", self.ps)
        self.assertEqual(self.ps.count("New-PendingPrivateSourcePath $label"), 2)
        self.assertIn('PRIVATE_PENDING_SOURCE_', self.ps)
        self.assertIn('PRIVATE_STALE_SOURCE_', self.ps)
        self.assertIn('Rename-Item -LiteralPath $preflight -NewName $staleName', self.ps)
        self.assertLess(self.ps.index('Rename-Item -LiteralPath $preflight -NewName $staleName'),
                        self.ps.index('$resumeExit = Invoke-PrivateAudit'))
        self.assertIn('"--preflight",$target,"--output",$pendingPreflight', self.ps)
        self.assertEqual(self.ps.count('Move-Item -LiteralPath $pendingPreflight -Destination $preflight'), 2)
        self.assertLess(self.ps.index('$resumeReport = Get-Content -LiteralPath $pendingPreflight'),
                        self.ps.index('Move-Item -LiteralPath $pendingPreflight -Destination $preflight'))
        self.assertLess(self.ps.index('$sourceReport = Get-Content -LiteralPath $pendingPreflight'),
                        self.ps.rindex('Move-Item -LiteralPath $pendingPreflight -Destination $preflight'))
        self.assertIn('Reject original resume cached preflight after silent zero-exit', self.workflow)

    def test_original_csv_is_pinned_immediately_after_each_preflight(self):
        # Late seven-session checks remain useful, but each changed raw
        # source must stop before another costly original game process run.
        self.assertIn('function Assert-PrivatePreflightCsvUnchanged',self.ps)
        self.assertIn('Get-FileHash -LiteralPath $csvPath -Algorithm SHA256 -ErrorAction Stop',self.ps)
        self.assertIn('PRIVATE SESSION PREFLIGHT SOURCE MUTATION FAIL-CLOSED',self.ps)
        self.assertEqual(self.ps.count('Assert-PrivatePreflightCsvUnchanged $label $target'),2)
        self.assertLess(self.ps.index('Assert-PrivatePreflightCsvUnchanged $label $target $resumeHash'),
                        self.ps.index('$resumeReport = Get-Content'))
        self.assertLess(self.ps.index('Assert-PrivatePreflightCsvUnchanged $label $target $digest'),
                        self.ps.index('$sourceReport = Get-Content'))
        self.assertIn('Reject raw original x86 CSV mutation during stage preflight',self.workflow)

    def test_timed_out_comparator_quarantines_partially_written_candidate(self):
        # Preflight success does not make a candidate safe when the final
        # Python process creates a JSON and then times out or cannot exit.
        self.assertIn("$comparisonError = $null", self.ps)
        self.assertIn("try {\n  $pythonExitCode = Invoke-PrivateAudit", self.ps)
        self.assertIn("$comparisonError = $_.Exception.Message", self.ps)
        self.assertIn("$null -ne $comparisonError -or", self.ps)
        self.assertLess(self.ps.index("$comparisonError = $_.Exception.Message"),
                        self.ps.index('Move-PrivateFailedCandidate $pendingCandidate'))
        self.assertIn('PRIVATE SESSION COMPARE FAIL-CLOSED', self.ps)
        self.assertIn('timeout_after_candidate', self.workflow)
        self.assertIn('Original timed-out comparison candidate not quarantined',
                      self.workflow)

    def test_private_cross_session_output_is_pending_and_cannot_authorize_world3d(self):
        # A zero-exit comparison must not manufacture an active JSON from
        # silence, a truncated candidate, or forged World3D/Quest authority.
        self.assertIn('PRIVATE_PENDING_COMPARE_', self.ps)
        self.assertIn('"--output",$pendingCandidate', self.ps)
        self.assertNotIn('"--output",$candidateOutput', self.ps)
        self.assertIn('ConvertFrom-Json -ErrorAction Stop', self.ps)
        self.assertIn('private_cross_phase_original_draw_candidate_triage', self.ps)
        self.assertIn('"aux,hud,menu,sky,title"', self.ps)
        self.assertIn('camera_axes_and_meters_per_unit -cne "NOT_CALIBRATED"', self.ps)
        self.assertIn('adapter_auto_enable -ne $false', self.ps)
        self.assertIn('world_pass_proof -cne "NOT_ESTABLISHED"', self.ps)
        self.assertIn('hmd_runtime_validation -cne "UNTESTED"', self.ps)
        self.assertIn('entry.world_pass_authorized -ne $false', self.ps)
        self.assertIn('PRIVATE SESSION COMPARE RECEIPT INVALID FAIL-CLOSED', self.ps)
        self.assertIn('$manifest.candidate_sha256 = (Get-FileHash -LiteralPath $pendingCandidate -Algorithm SHA256).Hash', self.ps)
        self.assertLess(self.ps.index('$manifest.candidate_sha256 ='),
                        self.ps.index('Move-Item -LiteralPath $pendingCandidate -Destination $candidateOutput'))
        self.assertIn('Move-PrivateFailedCandidate $pendingCandidate', self.ps)
        self.assertIn('Move-PrivateFailedCandidate $candidateOutput', self.ps)
        self.assertLess(self.ps.index('Move-Item -LiteralPath $pendingCandidate -Destination $candidateOutput'),
                        self.ps.index('Move-Item -LiteralPath $pendingManifest -Destination $finalManifest'))
        self.assertIn('Reject forged or incomplete private cross-session receipt', self.workflow)

    def test_private_partial_checkpoint_auditor_is_bundled_and_tested(self):
        self.assertIn('Copy-Item tools/audit_daytona_private_checkpoint.py stage/',
                      self.workflow)
        self.assertIn('test_daytona_private_checkpoint_audit.py -v',
                      self.workflow)
        self.assertIn('audit_daytona_private_checkpoint.py', self.docs)

    def test_extracted_bundle_contains_scripts_and_matching_python_modules(self):
        self.assertIn('powershell.exe -NoProfile -ExecutionPolicy Bypass -File',self.cmd)
        self.assertIn('"%~dp0Collect-Daytona-Original-Evidence.ps1"',self.cmd)
        for name in (
            "tools/model2_draw_source_report.py",
            "tools/model2_world_pass_evidence.py",
            "scripts/Collect-Daytona-Original-Evidence.ps1",
            "scripts/Collect-Daytona-Original-Evidence.cmd",
        ):
            self.assertIn("Copy-Item "+name+" stage/",self.workflow)
        self.assertIn("Get-FileHash -Algorithm SHA256",self.workflow)
        self.assertIn("Compress-Archive -Path stage/*",self.workflow)

if __name__ == "__main__":
    unittest.main()
