param([switch]$NoBuild,[switch]$NoCapture,
    [ValidateSet("preview","record","full")][string]$Pipeline="full",
    [string]$QtRoot='D:/Develop/QT6/6.11.2/mingw_64',
    [string]$MinGWRoot='D:/Develop/QT6/Tools/mingw1310_64',
    [string]$OpenCVBin='E:/opencv/install/x64/mingw/bin')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$NoBuild){& "$PSScriptRoot/build-and-test.ps1" -Projects day15_pro.pro -QtRoot $QtRoot -MinGWRoot $MinGWRoot -OpenCVBin $OpenCVBin}
& "$PSScriptRoot/deploy-runtime.ps1" -OutputDir "$root/.workflow/build/day15_pro/bin" -QtRoot $QtRoot -MinGWRoot $MinGWRoot -OpenCVBin $OpenCVBin -CleanStart
$oldPath=$env:PATH;$oldPlugins=$env:QT_PLUGIN_PATH;$oldAuto=$env:SMART_MONITOR_AUTOSTART;$oldPipeline=$env:SMART_MONITOR_PIPELINE
try{
    $env:PATH="$root/bin;$MinGWRoot/bin;$QtRoot/bin;$OpenCVBin;$oldPath"
    $env:QT_PLUGIN_PATH="$root/bin;$QtRoot/plugins"
    $env:SMART_MONITOR_PIPELINE=$Pipeline
    if($NoCapture){$env:SMART_MONITOR_AUTOSTART='0'}
    Start-Process -FilePath "$root/.workflow/build/day15_pro/bin/day20_smart_monitor.exe" -WorkingDirectory $root -WindowStyle Hidden | Out-Null
}finally{$env:PATH=$oldPath;$env:QT_PLUGIN_PATH=$oldPlugins;$env:SMART_MONITOR_AUTOSTART=$oldAuto;$env:SMART_MONITOR_PIPELINE=$oldPipeline}
