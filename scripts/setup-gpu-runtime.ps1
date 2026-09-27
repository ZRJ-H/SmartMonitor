# Reproducible optional Windows GPU runtime. No system PATH or driver changes.
param([string]$CacheDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) 'artifacts/gpu-runtime'))
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force $CacheDirectory | Out-Null
$packages=@(
    @{Name='ort-1.22.0';File='onnxruntime-directml-1.22.0.zip';Url='https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime.directml/1.22.0/microsoft.ml.onnxruntime.directml.1.22.0.nupkg';Hash='29F9872D786236B79AA83F94482F3A17C14297E4833768D6D0ED4883EE732E60'},
    @{Name='dml-1.15.4';File='directml-1.15.4.zip';Url='https://api.nuget.org/v3-flatcontainer/microsoft.ai.directml/1.15.4/microsoft.ai.directml.1.15.4.nupkg';Hash='4E7CB7DDCE8CF837A7A75DC029209B520CA0101470FCDF275C1F49736A3615B9'}
)
foreach($package in $packages){
    $archive=Join-Path $CacheDirectory $package.File
    if(!(Test-Path -LiteralPath $archive)){Invoke-WebRequest -Uri $package.Url -OutFile $archive}
    if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $package.Hash){throw "Runtime checksum mismatch: $archive"}
    Expand-Archive -LiteralPath $archive -DestinationPath (Join-Path $CacheDirectory $package.Name) -Force
}
$destination=Join-Path $CacheDirectory 'bin'
New-Item -ItemType Directory -Force $destination | Out-Null
Copy-Item "$CacheDirectory/ort-1.22.0/runtimes/win-x64/native/*.dll" $destination -Force
Copy-Item "$CacheDirectory/dml-1.15.4/bin/x64-win/DirectML.dll" $destination -Force
Copy-Item "$CacheDirectory/ort-1.22.0/LICENSE" "$destination/ONNX-RUNTIME-LICENSE.txt" -Force
Copy-Item "$CacheDirectory/dml-1.15.4/LICENSE.txt" "$destination/DIRECTML-LICENSE.txt" -Force
Write-Output "GPU runtime ready: $destination"
