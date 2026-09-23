param(
    [string]$QtRoot = 'D:/Develop/QT6/6.11.2/mingw_64',
    [string]$MinGWRoot = 'D:/Develop/QT6/Tools/mingw1310_64',
    [string]$OpenCVBin = 'E:/opencv/install/x64/mingw/bin',
    [string[]]$Projects = @(),
    [switch]$SkipApp,
    [int]$Jobs = 2
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$work = Join-Path $root '.workflow'
$snapshot = Join-Path $work 'source'
$report = Join-Path $root 'artifacts/verification'
New-Item -ItemType Directory -Force $snapshot,$report | Out-Null
$qmake = Join-Path $QtRoot 'bin/qmake.exe'
$make = Join-Path $MinGWRoot 'bin/mingw32-make.exe'
foreach ($tool in @($qmake,$make,(Join-Path $QtRoot 'bin/moc.exe'))) {
    if (!(Test-Path -LiteralPath $tool)) { throw "Missing tool: $tool" }
}
$sourceFingerprint = & "$PSScriptRoot/source-fingerprint.ps1" -Root $root
$gitCommit = & git -C $root rev-parse HEAD
$dirty = [bool](& git -C $root status --porcelain)
$runId = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$results = @()
@{ status='running'; runId=$runId; startedAt=(Get-Date).ToString('o') } | ConvertTo-Json | Set-Content -Encoding utf8 "$report/latest.json"
$savedPath = $env:PATH
$savedPlugin = $env:QT_PLUGIN_PATH
$savedPlatform = $env:QT_QPA_PLATFORM
try {
    $env:PATH = "$MinGWRoot/bin;$QtRoot/bin;$OpenCVBin;$savedPath"
    $env:QT_PLUGIN_PATH = "$QtRoot/plugins"
    $env:QT_QPA_PLATFORM = 'offscreen'
    # 只镜像版本控制中的输入，避免源目录遗留 moc 文件破坏影子构建的依赖图。
    $inputs = & git -C $root -c core.quotepath=false ls-files --cached --others --exclude-standard
    if ($LASTEXITCODE -ne 0) { throw 'Cannot enumerate source inputs' }
    # Remove only obsolete generated snapshot inputs, after checking the absolute boundary.
    $wanted = @{}
    foreach ($relative in $inputs) {
        if (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) {
            $wanted[$relative.Replace('\','/')] = $true
        }
    }
    $snapshotRoot = [IO.Path]::GetFullPath($snapshot).TrimEnd('\') + '\'
    foreach ($file in Get-ChildItem -LiteralPath $snapshot -File -Recurse) {
        if (!$file.FullName.StartsWith($snapshotRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Snapshot boundary violation' }
        $relative = $file.FullName.Substring($snapshotRoot.Length).Replace('\','/')
        if (!$wanted.ContainsKey($relative)) { Remove-Item -LiteralPath $file.FullName -Force }
    }
    foreach ($relative in ($inputs | Sort-Object -Unique)) {
        $from = Join-Path $root $relative
        if (!(Test-Path -LiteralPath $from -PathType Leaf)) { continue }
        $to = Join-Path $snapshot $relative
        New-Item -ItemType Directory -Force (Split-Path $to -Parent) | Out-Null
        if (!(Test-Path -LiteralPath $to) -or (Get-FileHash -LiteralPath $from).Hash -ne (Get-FileHash -LiteralPath $to).Hash) {
            Copy-Item -LiteralPath $from -Destination $to -Force
        }
    }
    if (!$Projects.Count) {
        $Projects = @(Get-ChildItem "$snapshot/tests/*_tests.pro" | ForEach-Object { 'tests/' + $_.Name })
        if (!$SkipApp) { $Projects += 'day15_pro.pro' }
    }
    $results = @()
    foreach ($project in $Projects) {
        $name = [IO.Path]::GetFileNameWithoutExtension($project)
        $build = Join-Path $work "build/$name"
        New-Item -ItemType Directory -Force $build | Out-Null
        Push-Location $build
        try {
            $destination = "$build/bin".Replace('\','/')
            $projectPath = Join-Path $snapshot $project
            & $qmake $projectPath -spec win32-g++ 'CONFIG+=release' 'CONFIG-=debug' -after "DESTDIR=$destination" *> "$report/$name-qmake.log"
            if ($LASTEXITCODE -ne 0) { throw "qmake failed: $name; see artifacts/verification" }
            & $make "-j$Jobs" *> "$report/$name-build.log"
            if ($LASTEXITCODE -ne 0) { Get-Content "$report/$name-build.log" -Tail 15; throw "Build failed: $name" }
            if ($project -like 'tests/*') {
                $test = Join-Path $destination "$name.exe"
                $testLog = Join-Path $report "$name-test.txt"
                $process = Start-Process -FilePath $test -ArgumentList @('-o',"$testLog,txt") -PassThru -WindowStyle Hidden
                if (!$process.WaitForExit(120000)) { $process.Kill(); throw "Test timeout: $name" }
                if (Test-Path $testLog) { Get-Content $testLog | Select-String 'Totals:|FAIL!|SKIP' | ForEach-Object { Write-Host "$name $_" } }
                if ($process.ExitCode -ne 0) { throw "Test failed: $name (exit $($process.ExitCode))" }
                $results += [pscustomobject]@{ project=$project; status='passed'; log=$testLog }
            } else {
                $results += [pscustomobject]@{ project=$project; status='built'; executable="$destination/day20_smart_monitor.exe"; sha256=(Get-FileHash -LiteralPath "$destination/day20_smart_monitor.exe" -Algorithm SHA256).Hash }
                Write-Host "$name built"
            }
        } finally { Pop-Location }
    }
    if ((& "$PSScriptRoot/source-fingerprint.ps1" -Root $root) -ne $sourceFingerprint) { throw 'Source changed during verification; rerun against a stable checkout' }
    @{ status='passed'; sourceFingerprint=$sourceFingerprint; gitCommit=$gitCommit; dirty=$dirty; runId=$runId; completedAt=(Get-Date).ToString('o'); results=$results } | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 "$report/latest.json"
} catch {
    @{ status='failed'; runId=$runId; error=$_.Exception.Message; completedProjects=$results } | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 "$report/latest.json"
    throw
} finally {
    $archive = Join-Path $report "runs/$runId"
    New-Item -ItemType Directory -Force $archive | Out-Null
    Copy-Item -LiteralPath "$report/latest.json" -Destination $archive
    Get-ChildItem -LiteralPath $report -File | Where-Object { $_.Extension -in '.log','.txt' } | Copy-Item -Destination $archive
    $env:PATH = $savedPath
    $env:QT_PLUGIN_PATH = $savedPlugin
    $env:QT_QPA_PLATFORM = $savedPlatform
}
