param([string]$DriverInstaller)
$ErrorActionPreference='Stop'
$expected='949ea2cd7d1b624ba6f9a830c1b95f77356d523cf9bf2fd86408a5888f8384c2'
if(!$DriverInstaller){
 $cache=Join-Path $PSScriptRoot '.build\Dependencies'
 New-Item -ItemType Directory -Force -Path $cache|Out-Null
 $DriverInstaller=Join-Path $cache 'InterceptorSetup-1.0.12.0.exe'
 if(!(Test-Path -LiteralPath $DriverInstaller)){
  Invoke-WebRequest -UseBasicParsing -Uri 'https://github.com/TitanCSA/Interceptor-Gamepad-Controller/releases/download/v1.0.12.0-demo/InterceptorSetup.exe' -OutFile $DriverInstaller -TimeoutSec 120
 }
}
$path=(Resolve-Path -LiteralPath $DriverInstaller).Path
if((Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -cne $expected){throw 'Supply the verified Interceptor Gamepad & Controller 1.0.12.0 UI installer.'}
$info=[Diagnostics.FileVersionInfo]::GetVersionInfo($path)
if($info.ProductName -cne 'InterceptorGamepadController' -or $info.FileVersion -cne '1.0.12.0'){throw 'Wrong installer edition or version.'}
return $path
