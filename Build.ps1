param([ValidateSet('All','Build','Test','Package')][string]$Target='All',[switch]$Render,[switch]$AddressSanitizer)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'Install Visual Studio 2022 C++ x64 tools and a Windows SDK.'}
$cmake=Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$build=Join-Path $root $(if($AddressSanitizer){'App\.build\Asan'}else{'App\.build\Release'})
$savedPath=$env:PATH
try{
 if($AddressSanitizer){$toolset=Get-ChildItem -LiteralPath (Join-Path $vs 'VC\Tools\MSVC') -Directory|Where-Object {Test-Path -LiteralPath (Join-Path $_.FullName 'bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll')}|Sort-Object {[version]$_.Name} -Descending|Select-Object -First 1;if(!$toolset){throw 'Install the C++ AddressSanitizer component.'};$env:PATH=(Join-Path $toolset.FullName 'bin\Hostx64\x64')+';'+$env:PATH}
 if($Target -ne 'Test'){
  & $cmake -S $root -B $build -G 'Visual Studio 17 2022' -A x64 ('-DCRONUS_ASAN='+$(if($AddressSanitizer){'ON'}else{'OFF'}))
  if($LASTEXITCODE){throw 'CMake configuration failed.'}
  $extra=if($AddressSanitizer){@('--',('/p:VCToolsVersion='+$toolset.Name))}else{@()}
  & $cmake --build $build --config Release --parallel @extra
  if($LASTEXITCODE){throw 'C++ build failed.'}
 }
 if($Target -ne 'Build'){
  & (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir $build -C Release --output-on-failure
  if($LASTEXITCODE){throw 'Offline tests failed.'}
  $process=Start-Process -FilePath (Join-Path $build 'bin\CronusInterceptor.exe') -ArgumentList '--self-test',('"'+(Join-Path $build 'ui-test.json')+'"') -PassThru -WindowStyle Hidden
  $held=$process.Handle;if(!$process.WaitForExit(30000)){throw 'Offline UI test timed out.'};if($process.ExitCode){throw 'Offline UI test failed.'}
 }
 if($Render){$process=Start-Process -FilePath (Join-Path $build 'bin\CronusInterceptor.exe') -ArgumentList '--render',('"'+(Join-Path $root 'docs\images\studio.png')+'"') -PassThru -WindowStyle Hidden;$held=$process.Handle;if(!$process.WaitForExit(30000) -or $process.ExitCode){throw 'Offline screenshot failed.'}}
 if($Target -eq 'Package'){if($AddressSanitizer){throw 'Do not package an AddressSanitizer build.'};& (Join-Path $root 'App\Package.ps1')}
}finally{$env:PATH=$savedPath}
Write-Host 'Native build complete. No driver installed or opened by offline tests.'
