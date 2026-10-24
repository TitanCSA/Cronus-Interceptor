param([string]$DriverInstaller)
$ErrorActionPreference='Stop'
$DriverInstaller=& (Join-Path $PSScriptRoot 'PackageDriver.ps1') -DriverInstaller $DriverInstaller
$root=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $root 'App\.build\Release\bin\CronusInterceptor.exe'
if((Get-Item -LiteralPath $exe).VersionInfo.FileVersion -cne '0.1.3'){throw 'Unexpected executable version'}
$bytes=[IO.File]::ReadAllBytes($exe);$pe=[BitConverter]::ToInt32($bytes,0x3c)
if([BitConverter]::ToUInt16($bytes,$pe+24)-ne 0x20b -or [BitConverter]::ToUInt32($bytes,$pe+24+112+14*8)-ne 0){throw 'Expected native Windows x64 app'}
if([Text.Encoding]::ASCII.GetString($bytes).IndexOf('clang_rt.asan',[StringComparison]::OrdinalIgnoreCase)-ge 0){throw 'Do not package an AddressSanitizer build'}
$stage=Join-Path $root ('App\.build\Package-'+[guid]::NewGuid().ToString('N'));$dist=Join-Path $root 'App\.build\dist'
New-Item -ItemType Directory -Force -Path $stage,$dist|Out-Null
Copy-Item -LiteralPath $exe -Destination $stage
Copy-Item -LiteralPath $DriverInstaller -Destination (Join-Path $stage 'InterceptorSetup.exe')
Copy-Item -LiteralPath $DriverInstaller -Destination (Join-Path $dist 'InterceptorSetup.exe') -Force
Copy-Item -LiteralPath $DriverInstaller -Destination (Join-Path (Split-Path $exe -Parent) 'InterceptorSetup.exe') -Force
foreach($name in 'README.md','LICENSE','DRIVER-BINARY-LICENSE.txt','THIRD-PARTY-NOTICES.md'){Copy-Item -LiteralPath (Join-Path $root $name) -Destination $stage}
foreach($name in 'Scripts','docs'){Copy-Item -LiteralPath (Join-Path $root $name) -Destination $stage -Recurse}
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$path=Join-Path $dist 'Cronus-Interceptor-0.1.3-Windows-x64.zip';$stream=[IO.File]::Open($path,[IO.FileMode]::Create)
try{$zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create,$true);try{foreach($file in Get-ChildItem -LiteralPath $stage -Recurse -File){[void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$file.FullName,$file.FullName.Substring($stage.Length+1).Replace('\','/'),[IO.Compression.CompressionLevel]::Optimal)}}finally{$zip.Dispose()}}finally{$stream.Dispose()}
Copy-Item -LiteralPath $exe -Destination $dist -Force
$lines=foreach($f in @($path,(Join-Path $dist 'CronusInterceptor.exe'),(Join-Path $dist 'InterceptorSetup.exe'))){(Get-FileHash -LiteralPath $f).Hash.ToLowerInvariant()+'  '+[IO.Path]::GetFileName($f)}
[IO.File]::WriteAllText((Join-Path $dist 'SHA256SUMS.txt'),($lines -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
Write-Output ('Portable app, original scripts, documentation and checksums: '+$dist)
Write-Output 'Verified Gamepad & Controller setup included inside the ZIP and as a separate download. No kernel source or downloaded third-party scripts are packaged.'
