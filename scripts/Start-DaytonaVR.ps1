# Daytona USA Quest 3 VR display launcher.
# Starts the user's existing Model 2 Emulator ONE time and attaches the
# independent OpenXR D3D11 host by process ID. Never hooks FFB or writes logs.
param([string]$Emulator = "")
$ErrorActionPreference = "Stop"
$hostBinary = Join-Path $PSScriptRoot "Model2OpenXRHost.exe"
if (-not (Test-Path -LiteralPath $hostBinary -PathType Leaf)) {
    Write-Host "ERROR: Model2OpenXRHost.exe must be beside this launcher."
    exit 2
}
if ([string]::IsNullOrWhiteSpace($Emulator)) {
    $Emulator = Read-Host "Full path to original Model 2 emulator EXE"
}
$Emulator = $Emulator.Trim().Trim('"')
try {
    $Emulator = (Resolve-Path -LiteralPath $Emulator -ErrorAction Stop).ProviderPath
} catch {
    Write-Host "ERROR: Emulator executable was not found."
    exit 3
}
if ([IO.Path]::GetExtension($Emulator).ToLowerInvariant() -ne ".exe") {
    Write-Host "ERROR: Select an emulator EXE, not a game ROM."
    exit 4
}
Write-Host ""
Write-Host "Daytona USA: starting ORIGINAL Model 2 Emulator (once)."
Write-Host "VR mode: head-tracked OpenXR 2D screen, NOT native 3D stereo."
Write-Host "Open Daytona USA inside the emulator, keep its window visible."
Write-Host "No probe/CSV logging, DirectInput changes or added FFB outputs."
Write-Host "Connect Quest 3 via an active Windows OpenXR runtime first."
Write-Host ""
try {
    $game = Start-Process -FilePath $Emulator -WorkingDirectory (
        Split-Path -Parent $Emulator) -PassThru -ErrorAction Stop
} catch {
    Write-Host "ERROR: Failed to start original emulator: $_"
    exit 5
}
Write-Host "Original emulator PID: $($game.Id)"
Write-Host "Starting OpenXR display for exactly this process..."
& $hostBinary --pid "$($game.Id)"
$code = $LASTEXITCODE
if ($null -eq $code) { $code = 6 }
if ($code -ne 0) {
    Write-Host "OpenXR display returned $code. Emulator was left running."
    exit $code
}
exit 0
