param(
    [ValidateSet('Backup','Migrate','Restore')][string]$Action='Backup',
    [string]$MySQLBin='C:/Program Files/MySQL/MySQL Server 5.7/bin',
    [string]$Database='smart_monitor_db',
    [string]$Server='127.0.0.1',
    [string]$User='root',
    [string]$OutputDirectory='',
    [string]$InputFile='',
    [switch]$AllowRestore
)
$ErrorActionPreference='Stop'
if($Database -notmatch '^[A-Za-z0-9_]+$'){throw 'Invalid database name'}
$root=Split-Path $PSScriptRoot -Parent
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'artifacts/backups'}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$mysql=Join-Path $MySQLBin 'mysql.exe'
$dump=Join-Path $MySQLBin 'mysqldump.exe'
$savedPassword=$env:MYSQL_PWD
if($env:SMART_MONITOR_DB_PASSWORD){$env:MYSQL_PWD=$env:SMART_MONITOR_DB_PASSWORD}
$argsCommon=@("--host=$Server","--user=$User",'--default-character-set=utf8mb4')
function Backup-Database {
    $target=Join-Path $OutputDirectory ($Database+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'.sql')
    & $dump @argsCommon --single-transaction --routines --events --hex-blob "--result-file=$target" $Database
    if($LASTEXITCODE -ne 0){throw 'Database backup failed'}
    $hash=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    [IO.File]::WriteAllText($target+'.sha256',$hash)
    return $target
}
function Invoke-SqlFile([string]$file) {
    if(!(Test-Path -LiteralPath $file)){throw "Missing SQL file: $file"}
    # 使用 mysql 自身 source，避免 PowerShell 管道改变 UTF-8 SQL 内容。
    $sqlPath=(Resolve-Path -LiteralPath $file).Path.Replace('\','/')
    & $mysql @argsCommon "--database=$Database" "--execute=source $sqlPath"
    if($LASTEXITCODE -ne 0){throw 'SQL execution failed; backup retained'}
}
try {
    switch($Action){
        'Backup' { Backup-Database }
        'Migrate' {
            $null=Backup-Database
            Invoke-SqlFile (Join-Path $root 'database/project_completion_migration.sql')
            Write-Output 'Migration completed; existing media retained.'
        }
        'Restore' {
            if(!$AllowRestore){throw 'Restore requires -AllowRestore and an explicit target database'}
            if(!$InputFile){throw 'Restore requires -InputFile'}
            $null=Backup-Database
            Invoke-SqlFile $InputFile
            Write-Output 'Restore completed.'
        }
    }
} finally {$env:MYSQL_PWD=$savedPassword}
