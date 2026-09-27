# Test media is downloaded only to the ignored artifacts directory.
param()
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$directory=Join-Path $root 'artifacts/test-media'
New-Item -ItemType Directory -Force $directory | Out-Null
$path=Join-Path $directory 'vtest.avi'
if(!(Test-Path -LiteralPath $path)){Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/opencv/opencv/4.12.0/samples/data/vtest.avi' -OutFile $path}
$expected='45CDDC9490BE69345CBDAB64CA583BE65987E864CA408038E648DB99E10516CF'
if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $expected){throw 'Unexpected sample video checksum'}
Write-Output $path
