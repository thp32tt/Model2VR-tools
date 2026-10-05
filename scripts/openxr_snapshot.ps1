param([string]$Output = "openxr_snapshot.json")
$ErrorActionPreference="SilentlyContinue"
function Read-Key($path) {
  if(Test-Path $path) {
    $item=Get-ItemProperty $path
    $o=[ordered]@{}
    foreach($p in $item.PSObject.Properties) {
      if($p.Name -notmatch '^PS') { $o[$p.Name]=$p.Value }
    }
    return $o
  }
  return $null
}
$runtime=@{
  HKLM64 = Read-Key "HKLM:\SOFTWARE\Khronos\OpenXR\1"
  HKLM32 = Read-Key "HKLM:\SOFTWARE\WOW6432Node\Khronos\OpenXR\1"
  HKCU   = Read-Key "HKCU:\SOFTWARE\Khronos\OpenXR\1"
}
$layers=@{
  implicit64 = Read-Key "HKLM:\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit"
  explicit64 = Read-Key "HKLM:\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Explicit"
  implicit32 = Read-Key "HKLM:\SOFTWARE\WOW6432Node\Khronos\OpenXR\1\ApiLayers\Implicit"
  explicit32 = Read-Key "HKLM:\SOFTWARE\WOW6432Node\Khronos\OpenXR\1\ApiLayers\Explicit"
}
$gpus=Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,AdapterRAM,VideoModeDescription
$os=Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber,OSArchitecture
$steam=Read-Key "HKCU:\Software\Valve\Steam"
$result=[ordered]@{
  timestamp=(Get-Date).ToString("o")
  computer=$env:COMPUTERNAME
  os=$os
  gpu=$gpus
  openxr_runtime=$runtime
  openxr_layers=$layers
  steam=$steam
  env=@{
    XR_RUNTIME_JSON=$env:XR_RUNTIME_JSON
    STEAMVR_RUNTIME=$env:STEAMVR_RUNTIME
  }
}
$result | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 $Output
$result | ConvertTo-Json -Depth 8
