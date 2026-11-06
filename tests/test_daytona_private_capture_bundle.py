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

    def test_original_exe_is_never_patched_or_uploaded(self):
        for forbidden in (
            "Invoke-WebRequest", "Invoke-RestMethod", "Start-BitsTransfer",
            "New-PSSession", "Copy-Item -LiteralPath $PackageRoot",
            "Set-Content -LiteralPath $originalPath", "Remove-Item",
            "github.com", "git push", "World3D=true", "world_pass_proof = \"ESTABLISHED\"",
        ):
            self.assertNotIn(forbidden,self.ps)
        self.assertIn("Get-FileHash -LiteralPath $originalPath -Algorithm SHA256",self.ps)
        self.assertIn("Get-FileHash -LiteralPath $target -Algorithm SHA256",self.ps)
        self.assertIn("scene_labels_verified_by",self.ps)
        self.assertIn('"NOT_ESTABLISHED"',self.ps)
        self.assertIn('"UNTESTED"',self.ps)
        self.assertIn("Copy-Item -LiteralPath $originalPath -Destination $target",self.ps)

    def test_seven_distinct_original_process_sessions_and_bounded_analysis(self):
        self.assertIn('("race_a","race_b","title","menu","sky","hud","aux")',self.ps)
        self.assertIn('Get-ChildItem -LiteralPath $logRoot -Filter "daytona_vr_startup_*.csv"',self.ps)
        self.assertIn('$knownPaths.Add($originalPath)',self.ps)
        self.assertIn('$knownDigests.Add($digest)',self.ps)
        self.assertIn('Where-Object { $_.LastWriteTimeUtc -ge $started }',self.ps)
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
        self.assertIn('Rename-Item -LiteralPath $candidateOutput', self.ps)
        self.assertIn('$pythonExitCode -ne 0', self.ps)

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
