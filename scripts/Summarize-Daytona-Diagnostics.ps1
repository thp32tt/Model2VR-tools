param(
 [Parameter(Mandatory=$true)][string]$LogDir,
 [Parameter(Mandatory=$true)][string]$LauncherLog,
 [Parameter(Mandatory=$true)][string]$Stamp
)
$ErrorActionPreference="Stop"
$dir=[IO.Path]::GetFullPath($LogDir)
if(!(Test-Path -LiteralPath $dir)){New-Item -Path $dir -ItemType Directory -Force|Out-Null}
$report=Join-Path $dir ("daytona_vr_diagnostics_"+$Stamp+".txt")
$lines=New-Object System.Collections.Generic.List[string]
$lines.Add("DAYTONA VR ONE-RUN DIAGNOSTICS")
$lines.Add("local_time="+(Get-Date).ToString("yyyy-MM-dd HH:mm:ss"))
$lines.Add("original_emulator_and_ffb=UNCHANGED")
$lines.Add("HMD_6DOF_CALIBRATION=UNVERIFIED")
$launch=Get-Item -LiteralPath $LauncherLog -ErrorAction SilentlyContinue
$cutoff=if($launch){$launch.CreationTime.AddSeconds(-5)}else{(Get-Date).AddMinutes(-60)}
$csv=Get-ChildItem -LiteralPath $dir -Filter 'daytona_vr_startup_*.csv' -File -ErrorAction SilentlyContinue|Where-Object {$_.LastWriteTime -ge $cutoff}|Sort-Object LastWriteTime -Descending|Select-Object -First 1
$hostLog=Get-ChildItem -LiteralPath $dir -Filter 'daytona_vr_host_*.log' -File -ErrorAction SilentlyContinue|Where-Object {$_.LastWriteTime -ge $cutoff}|Sort-Object LastWriteTime -Descending|Select-Object -First 1
$lines.Add("csv="+$(if($csv){$csv.FullName}else{"MISSING"}))
$lines.Add("host_log="+$(if($hostLog){$hostLog.FullName}else{"MISSING"}))
function Pair([string]$raw){
 $u=[UInt64]::Parse($raw)
 $hi=[UInt64]($u -shr 32)
 $lo=[UInt64]($u -band 4294967295)
 return "$hi/$lo"
}
$reasons=@("NO_EYE_SOURCE","NO_VERIFIED_WORLD_SCENE","INVALID_DEVICE_FRAME","POSE_NOT_ADMITTED","POSE_STALE","D3D9_READBACK_FAILED","IDENTICAL_EYE_PIXELS","IPC_PUBLISH_FAILED","ORIGINAL_PRESENT_FAILED")
if($csv){
 $rows=@(Import-Csv -LiteralPath $csv.FullName)
 $lines.Add("csv_rows="+$rows.Count)
 foreach($type in @("probe_ready","model2_d3d9_device_hook_result","model2_vr_original_draw_families","model2_vr_producer_frame","model2_vr_diag_up_source","model2_vr_diag_source_gates","model2_vr_diag_source_drops","model2_vr_scene_gate","model2_vr_scene_reject_reason","model2_vr_diag_first_scene_reject","model2_vr_diag_scene_reject_total")){
   $matches=@($rows|Where-Object {$_.event -eq $type})
   $lines.Add("event="+$type+" count="+$matches.Count)
   foreach($e in @($matches|Select-Object -Last 12)){$lines.Add("  qpc="+$e.qpc+" a0="+$e.a0+" a1="+$e.a1+" a2="+$e.a2+" a3="+$e.a3)}
 }
 $last=@($rows|Where-Object {$_.event -eq "model2_vr_producer_frame"}|Select-Object -Last 1)
 if($last.Count){
   $mask=[UInt32]$last[0].a1
   $names=New-Object System.Collections.Generic.List[string]
   for($bit=0;$bit -lt $reasons.Count;$bit++){if(($mask -band (1 -shl $bit)) -ne 0){$names.Add($reasons[$bit])}}
   $lines.Add("last_present="+$last[0].a0)
   $lines.Add("producer_reason_mask="+$mask)
   $lines.Add("producer_reason="+$(if($mask -eq 0){"PAIR_PUBLISHED"}else{$names -join ","}))
 }
 $g=@($rows|Where-Object {$_.event -eq "model2_vr_diag_source_gates"}|Select-Object -Last 1)
 if($g.Count){
   $lines.Add("source_shape/expected_caller="+(Pair $g[0].a1))
   $lines.Add("source_shader/state_pass="+(Pair $g[0].a2))
   $lines.Add("source_stereo_shift/total_draw_attempts="+(Pair $g[0].a3))
 }
 $reasonLabels=@("NONE","DEVICE_QUERY","VIEWPORT","DEPTH","BLEND_ALPHA","STENCIL","SCISSOR","WRITE_MASK","BIAS","CLIP_PLANE","CULL","SRGB")
 $firstFailures=@($rows|Where-Object {$_.event -eq "model2_vr_diag_first_scene_reject"}|Select-Object -Last 12)
 foreach($failure in $firstFailures){
   $reasonCode=[int]$failure.a1
   $reasonLabel=if($reasonCode -ge 0 -and $reasonCode -lt $reasonLabels.Count){$reasonLabels[$reasonCode]}else{"UNKNOWN"}
   $vpWidth=[UInt64]::Parse($failure.a2) -shr 32
   $vpHeight=[UInt64]::Parse($failure.a2) -band 4294967295
   $lines.Add("first_scene_reject_code="+$reasonCode+" reason="+$reasonLabel+" caller_rva="+$failure.a0+" viewport="+$vpWidth+"x"+$vpHeight)
 }
 $drops=@($rows|Where-Object {$_.event -eq "model2_vr_diag_source_drops"}|Select-Object -Last 1)
 if($drops.Count){$lines.Add("draw_reject_pose="+$drops[0].a1+" resource="+$drops[0].a2+" eye="+$drops[0].a3)}
}
if($hostLog){
 $lines.Add("---- OpenXR host tail (120 lines) ----")
 foreach($line in @(Get-Content -LiteralPath $hostLog.FullName -Tail 120)){$lines.Add($line)}
}
if($launch){
 $lines.Add("---- Launcher ----")
 foreach($line in @(Get-Content -LiteralPath $launch.FullName -Tail 60)){$lines.Add($line)}
}
$lines|Set-Content -LiteralPath $report -Encoding UTF8
Write-Host ("Detailed Daytona VR report: "+$report)
