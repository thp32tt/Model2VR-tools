param(
  [string]$Destination = "$PSScriptRoot\..\external-tools"
)
$ErrorActionPreference = "Stop"
$dest = [System.IO.Path]::GetFullPath($Destination)
New-Item -ItemType Directory -Force -Path $dest | Out-Null

$tools = @(
  @{
    Name="texconv.exe"
    Url="https://github.com/microsoft/DirectXTex/releases/download/may2026/texconv.exe"
    Sha256="dcfdec10244e02cf5037fba089c55fb7e1326b1c8181742d77d15fa5cb5eef06"
  },
  @{
    Name="texdiag.exe"
    Url="https://github.com/microsoft/DirectXTex/releases/download/may2026/texdiag.exe"
    Sha256="411c303c98ba73e4423376f717ac139347dd749bf80a7fc1a22368ab1088ff56"
  },
  @{
    Name="texassemble.exe"
    Url="https://github.com/microsoft/DirectXTex/releases/download/may2026/texassemble.exe"
    Sha256="324721a80cf954eccfd888a6c587f6602146f043a01d0181af25884c549e8f46"
  }
)
foreach($t in $tools) {
  $out = Join-Path $dest $t.Name
  Invoke-WebRequest -UseBasicParsing -Uri $t.Url -OutFile $out
  $actual=(Get-FileHash -Algorithm SHA256 $out).Hash.ToLowerInvariant()
  if($actual -ne $t.Sha256) { throw "SHA256 mismatch for $($t.Name): $actual" }
  Write-Host "$($t.Name) OK $actual"
}
@{
  directxtex_release="may2026"
  source="microsoft/DirectXTex"
  files=$tools
} | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 (Join-Path $dest "tool-manifest.json")
Write-Host "External tools ready: $dest"
