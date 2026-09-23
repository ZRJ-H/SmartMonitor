param([Parameter(Mandatory=$true)][int]$ProcessId,[int]$Minutes=60,[int]$IntervalSeconds=60,[string]$OutputFile='')
$ErrorActionPreference='Stop'
if($Minutes -lt 1 -or $IntervalSeconds -lt 1){throw 'Durations must be positive'}
$root=Split-Path $PSScriptRoot -Parent
if(!$OutputFile){$OutputFile="$root/artifacts/verification/soak-$(Get-Date -Format yyyyMMdd-HHmmss).csv"}
New-Item -ItemType Directory -Force (Split-Path $OutputFile -Parent) | Out-Null
$deadline=(Get-Date).AddMinutes($Minutes)
do{
    $process=Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    if(!$process){throw "Process $ProcessId exited; evidence retained at $OutputFile"}
    [pscustomobject]@{timestamp=(Get-Date).ToString('o');processId=$ProcessId;workingSetBytes=$process.WorkingSet64;privateBytes=$process.PrivateMemorySize64;cpuSeconds=$process.CPU;handles=$process.HandleCount;responding=$process.Responding} |
        Export-Csv -LiteralPath $OutputFile -Append -NoTypeInformation -Encoding utf8
    Start-Sleep -Seconds $IntervalSeconds
}while((Get-Date) -lt $deadline)
Write-Output $OutputFile
