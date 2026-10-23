param([string]$Output = (Join-Path $PSScriptRoot 'level6_opt.exe'))
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Nie znaleziono Visual Studio Installer / vswhere.exe.' }
$installation = & $vswhere -latest -prerelease -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Nie znaleziono narzedzi Visual C++ x64.' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
$buildDir = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
$response = Join-Path $buildDir 'compile.rsp'
$source = Join-Path $PSScriptRoot 'level6_opt.cpp'
$object = Join-Path $buildDir 'level6_opt.obj'
@"
/nologo /O2 /std:c++20 /EHsc /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /W3
/arch:AVX2 /Fe:"$Output" /Fo:"$object" "$source"
"@ | Set-Content -LiteralPath $response -Encoding ascii
$command = 'call "' + $vcvars + '" >nul && cl @"' + $response + '"'
& $env:ComSpec /d /s /c $command
if ($LASTEXITCODE -ne 0) { throw "Kompilacja zakonczona kodem $LASTEXITCODE" }
