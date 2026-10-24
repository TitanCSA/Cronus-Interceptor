param([string]$DriverInstaller)
$ErrorActionPreference='Stop'
$expected='64b451334c3eca1b2a8c2941e72bf84983675098defd0b0269f4d945dbb2c75e'
if(!$DriverInstaller){
 $cache=Join-Path $PSScriptRoot '.build\Dependencies'
 New-Item -ItemType Directory -Force -Path $cache|Out-Null
 $DriverInstaller=Join-Path $cache 'InterceptorSetup-1.0.13.0.exe'
 if(!(Test-Path -LiteralPath $DriverInstaller)){
  Invoke-WebRequest -UseBasicParsing -Uri 'https://github.com/TitanCSA/Interceptor-Gamepad-Controller/releases/download/v1.0.13.0-demo/InterceptorSetup.exe' -OutFile $DriverInstaller -TimeoutSec 120
 }
}
$path=(Resolve-Path -LiteralPath $DriverInstaller).Path
if((Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -cne $expected){throw 'Supply the verified Interceptor Gamepad & Controller 1.0.13.0 UI installer.'}
$info=[Diagnostics.FileVersionInfo]::GetVersionInfo($path)
if($info.ProductName -cne 'InterceptorGamepadController' -or $info.FileVersion -cne '1.0.13.0'){throw 'Wrong installer edition or version.'}
return $path
