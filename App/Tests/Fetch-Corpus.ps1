$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$folder=Join-Path $root 'App\.build\Corpus'
New-Item -ItemType Directory -Force -Path $folder|Out-Null
$sources=@(
 @{Repo='cypher139/cronus';Commit='f364883e1df49451d5165c2ad1088244380f90f6'},
 @{Repo='Fadexz/COD-Pro';Commit='7fd78ebb3deedadcdbe2fbc6baca4105e3f9097e'}
)
$records=@()
foreach($source in $sources){
 $repoName=$source.Repo.Replace('/','-')
 $zipPath=Join-Path $folder ($repoName+'.zip');$destination=Join-Path $folder $repoName
 if(Test-Path -LiteralPath $destination){throw ('Corpus destination exists; choose another build checkout: '+$destination)}
 $url='https://codeload.github.com/'+$source.Repo+'/zip/'+$source.Commit
 Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $zipPath -TimeoutSec 60
 $archive=[IO.Compression.ZipFile]::OpenRead($zipPath)
 try{foreach($entry in $archive.Entries){if($entry.FullName -match '(^/|(^|/)\.\.(/|$)|:|\\)' -or $entry.Length -gt 16777216){throw 'Unexpected archive entry'}}}finally{$archive.Dispose()}
 Expand-Archive -LiteralPath $zipPath -DestinationPath $destination
 foreach($file in Get-ChildItem -LiteralPath $destination -Recurse -Filter '*.gpc'){$records+=@{repository=$source.Repo;commit=$source.Commit;file=$file.FullName.Substring($destination.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $file.FullName).Hash}}
}
$records|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $folder 'manifest.json') -Encoding UTF8
Write-Output ('Pinned GPL corpus downloaded to ignored build storage: '+$folder)
Write-Output 'Optional tests do not distribute these scripts as MIT examples or open a driver.'
