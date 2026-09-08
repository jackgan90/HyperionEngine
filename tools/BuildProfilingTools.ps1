param()
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
& python "$ProjectRoot/tools/Bootstrap.py" --only tracy
if ($LASTEXITCODE) { throw 'Locked Tracy bootstrap failed.' }
$Vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$Vs = & $Vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $Vs) { throw 'MSVC x64 build tools were not found.' }
Import-Module (Join-Path $Vs 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $Vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$CmakeRoot = Join-Path $Vs 'Common7\IDE\CommonExtensions\Microsoft\CMake'
$Cmake = Join-Path $CmakeRoot 'CMake\bin\cmake.exe'
$Ninja = Join-Path $CmakeRoot 'Ninja\ninja.exe'
& $Cmake -S "$ProjectRoot/cmake/ProfilingTools" -B "$ProjectRoot/out/build/profiling-tools" -G Ninja "-DCMAKE_MAKE_PROGRAM=$Ninja" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE) { throw 'Profiling tools configure failed.' }
& $Cmake --build "$ProjectRoot/out/build/profiling-tools" --parallel 8
if ($LASTEXITCODE) { throw 'Profiling tools build failed.' }
