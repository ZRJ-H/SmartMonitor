# 复用隔离数据库、真实媒体编解码和模型，验证三层负载及评审交互。
param([int]$Jobs=4,[string]$ModelPath='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$ModelPath){$ModelPath=Join-Path $root 'models/yolov8n.onnx'}
if(!(Test-Path -LiteralPath $ModelPath -PathType Leaf)){throw '需要本地 YOLO ONNX 模型，使用 -ModelPath 指定'}
$oldModel=$env:SMART_MONITOR_TEST_MODEL
$oldCapture=$env:SMART_MONITOR_UI_CAPTURE_DIR
$oldPipeline=$env:SMART_MONITOR_PIPELINE
try {
    $env:SMART_MONITOR_PIPELINE="full"
    $env:SMART_MONITOR_TEST_MODEL=(Resolve-Path -LiteralPath $ModelPath).Path
    $env:SMART_MONITOR_UI_CAPTURE_DIR=Join-Path $root 'artifacts/verification/review-ui'
    & "$PSScriptRoot/build-and-test.ps1" -Jobs $Jobs
    # 干净 PATH + 隔离数据库保护：即使入口将来回归，也不能连生产库。
    $probeVariables=@('PATH','QT_PLUGIN_PATH','QT_QPA_PLATFORM','SMART_MONITOR_DB_DRIVER','SMART_MONITOR_DB_NAME','SMART_MONITOR_AUTOSTART')
    $saved=@{}
    foreach($name in $probeVariables){$saved[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
    try {
        $env:PATH="$env:SystemRoot/System32"
        $env:QT_PLUGIN_PATH=''
        $env:QT_QPA_PLATFORM='offscreen'
        $env:SMART_MONITOR_DB_DRIVER='QSQLITE'
        $env:SMART_MONITOR_DB_NAME=':memory:'
        $env:SMART_MONITOR_AUTOSTART='0'
        $app=Join-Path $root '.workflow/build/day15_pro/bin/day20_smart_monitor.exe'
        $probe=Start-Process -FilePath $app -ArgumentList '--runtime-check' -WorkingDirectory $root -WindowStyle Hidden -PassThru
        if(!$probe.WaitForExit(20000)){$probe.Kill();throw 'Runtime check did not exit'}
        if($probe.ExitCode -ne 0){throw "Runtime check failed: $($probe.ExitCode)"}
        @{exitCode=$probe.ExitCode;checkedAt=(Get-Date).ToString('o');isolatedDatabase=$true;cleanPath=$true;sha256=(Get-FileHash $app -Algorithm SHA256).Hash} |
            ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $root 'artifacts/verification/review-runtime.json')
        Write-Output 'Runtime check passed and exited without starting business windows.'
    } finally {
        foreach($name in $probeVariables){[Environment]::SetEnvironmentVariable($name,$saved[$name],'Process')}
    }
} finally {
    $env:SMART_MONITOR_TEST_MODEL=$oldModel
    $env:SMART_MONITOR_UI_CAPTURE_DIR=$oldCapture
    $env:SMART_MONITOR_PIPELINE=$oldPipeline
}
