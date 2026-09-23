param([string]$Root=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$inputs=& git -C $Root -c core.quotepath=false ls-files --cached --others --exclude-standard
if($LASTEXITCODE -ne 0){throw 'Cannot fingerprint source inputs'}
$lines=foreach($relative in ($inputs | Sort-Object -Unique)){
    $path=Join-Path $Root $relative
    if(Test-Path -LiteralPath $path -PathType Leaf){$relative.Replace('\','/')+':'+(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
}
$bytes=[Text.Encoding]::UTF8.GetBytes(($lines -join [char]10))
$sha=[Security.Cryptography.SHA256]::Create()
try{[BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-','')}finally{$sha.Dispose()}
