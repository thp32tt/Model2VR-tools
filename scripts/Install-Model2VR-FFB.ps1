# Install the separately-licensed Generic DD v2 plugin shipped with Model2VR.
# Invoke with -EmulatorExe C:\Model2\emulator_multicpu.exe.
# Existing FFBPlugin.ini and DLLs are preserved unless -ReplaceExisting is set.
param(
    [Parameter(Mandatory=$true)][string]$EmulatorExe,
    [switch]$ReplaceExisting
)
$ErrorActionPreference='Stop'
$emulator=Resolve-Path -LiteralPath $EmulatorExe -ErrorAction Stop
$target=Split-Path -Parent $emulator
$source=Join-Path $PSScriptRoot 'FFB'
$required=@('dinput8.dll','SDL2.dll','FFBPlugin.ini','FFBPluginGUI.exe','MetroFramework.dll')
foreach($item in $required) {
    if(-not (Test-Path -LiteralPath (Join-Path $source $item) -PathType Leaf)){
        throw "Integrated Generic DD v2 package missing: $item"
    }
}
$ini=Get-Content -LiteralPath (Join-Path $source 'FFBPlugin.ini') -Raw
if($ini -notmatch '(?m)^\s*GameId\s*=\s*25\s*$' -or
   $ini -notmatch '(?m)^\s*Model2ModernFFB\s*=\s*1\s*$' -or
   $ini -notmatch '(?m)^\s*\[Model2Modern\]\s*$') {
    throw 'Expected Model 2 Generic DD v2 profile was not found'
}
$existing=@($required | Where-Object {Test-Path -LiteralPath (Join-Path $target $_) -PathType Leaf})
if($existing.Count -ne 0 -and -not $ReplaceExisting) {
    throw ("Existing FFB files found: {0}. Use -ReplaceExisting for timestamped backup before installing." -f ($existing -join ', '))
}
$backup=$null
if($existing.Count){
    $backup=Join-Path $target ("Model2VR-FFB-backup-"+(Get-Date -Format 'yyyyMMdd-HHmmss'))
    New-Item -ItemType Directory -Force -Path $backup | Out-Null
    foreach($item in $existing) {
        Copy-Item -LiteralPath (Join-Path $target $item) -Destination (Join-Path $backup $item)
    }
}
Get-ChildItem -LiteralPath $source -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $target $_.Name) -Force
}
Write-Host "Model2VR Generic DD v2 files installed to $target"
if($backup){Write-Host "Old files were backed up to $backup"}
Write-Host 'Launch the emulator through Model2VRLoader.exe; F11 opens the desktop FFB panel.'
Write-Host 'FFBPluginGUI.exe controls wheel/pedal/button mappings. FFB itself remains single-owner.'
