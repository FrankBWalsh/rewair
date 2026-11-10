<#
  build_local_bridge.ps1 - Windows equivalent of scripts/build_local_bridge.zsh.
  Builds the web UI, syncs the Rewair app/platform into the WICED SDK and
  builds the OTA-installable application image with the SDK's Win32 tools.

  Expects the default layout (this repo beside third_party\wiced-emw3165);
  override with -SdkDir. Requires Node.js 18+.

  Usage:  pwsh -File scripts\build_local_bridge.ps1 [-SdkDir <path>]
  Output: <SdkDir>\build\rewair_local_bridge-AWAIR-FreeRTOS-LwIP-SDIO\binary\*.bin
#>
param([string]$SdkDir = '')

$ErrorActionPreference = 'Stop'
$repo   = Split-Path -Parent $PSScriptRoot
$sdk    = if ($SdkDir) { $SdkDir } else { Join-Path (Split-Path -Parent $repo) 'third_party\wiced-emw3165' }
$target = 'rewair.local_bridge-AWAIR-FreeRTOS-LwIP-SDIO'
$build  = 'rewair_local_bridge-AWAIR-FreeRTOS-LwIP-SDIO'

if (-not (Test-Path "$sdk\tools\common\Win32\make.exe")) { throw "WICED SDK not found at $sdk (use -SdkDir)" }

Write-Host '== web UI'
Push-Location "$repo\webui"
if (-not (Test-Path node_modules\.bin\vite.cmd)) { npm ci --no-audit --no-fund; if ($LASTEXITCODE) { throw 'npm ci failed' } }
npm run build; if ($LASTEXITCODE) { throw 'web UI build failed' }
Pop-Location

Write-Host '== sync into SDK'
foreach ($pair in @(@("$repo\wiced\apps\rewair", "$sdk\apps\rewair"), @("$repo\wiced\platforms\AWAIR", "$sdk\platforms\AWAIR"))) {
    if (Test-Path $pair[1]) { Remove-Item -Recurse -Force $pair[1] }
    Copy-Item -Recurse $pair[0] $pair[1]
}
Copy-Item "$repo\webui\webui.rwfs" "$sdk\apps\rewair\local_bridge\webui.bin" -Force

Write-Host '== firmware'
Push-Location $sdk
Remove-Item -Force -ErrorAction SilentlyContinue "build\$build\config.mk"
& .\tools\common\Win32\make.exe $target HOST_OS=Win32
if ($LASTEXITCODE) { Pop-Location; throw 'firmware build failed' }
Pop-Location

$bin = "$sdk\build\$build\binary\$build.bin"
$size = (Get-Item $bin).Length
$limit = 0x74000
if ($size -gt $limit) { throw "application image is $size bytes; limit is $limit" }
Write-Host "Built: $bin ($size / $limit bytes)"
