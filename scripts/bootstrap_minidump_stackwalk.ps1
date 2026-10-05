param([string]$Destination = "$PSScriptRoot\..\external-tools\minidump")
$ErrorActionPreference="Stop"
$dest=[System.IO.Path]::GetFullPath($Destination)
New-Item -ItemType Directory -Force -Path $dest | Out-Null
$url="https://github.com/rust-minidump/rust-minidump/releases/download/v0.27.0/minidump-stackwalk-x86_64-pc-windows-msvc.zip"
$expected="f6f2d7f1665843c4a270cd13fcd1458fed3b19013ea5dd909e12bc3599b959f4"
$zip=Join-Path $dest "minidump-stackwalk.zip"
Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $zip
$actual=(Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant()
if($actual -ne $expected){ throw "SHA256 mismatch: $actual" }
Expand-Archive -Path $zip -DestinationPath $dest -Force
Write-Host "minidump-stackwalk v0.27.0 verified: $actual"
