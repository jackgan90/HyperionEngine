param(
    [ValidateSet('debug','release')][string]$Preset = 'debug',
    [string]$Target = '',
    [switch]$Test,
    [switch]$RenderDoc,
    [switch]$NoRenderDoc
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$Vs = & $Vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $Vs) { throw 'MSVC x64 build tools were not found.' }
Import-Module (Join-Path $Vs 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $Vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$CmakeRoot = Join-Path $Vs 'Common7\IDE\CommonExtensions\Microsoft\CMake'
$Cmake = Join-Path $CmakeRoot 'CMake\bin\cmake.exe'
$Ctest = Join-Path $CmakeRoot 'CMake\bin\ctest.exe'
$Ninja = Join-Path $CmakeRoot 'Ninja\ninja.exe'
$PythonExecutable = & python -c 'import sys; print(sys.executable)'
if ($LASTEXITCODE -or -not $PythonExecutable) { throw 'A working Python interpreter is required.' }
Push-Location $ProjectRoot
try {
    if ($RenderDoc -and $NoRenderDoc) { throw 'Use either -RenderDoc or -NoRenderDoc.' }
    $CaptureArguments = @()
    if ($RenderDoc) {
        & python tools/Bootstrap.py --only renderdoc
        if ($LASTEXITCODE) { throw 'RenderDoc header bootstrap failed.' }
        $CaptureArguments += '-DHYP_ENABLE_RENDERDOC=ON'
    }
    if ($NoRenderDoc) { $CaptureArguments += '-DHYP_ENABLE_RENDERDOC=OFF' }
    & $Cmake --preset $Preset "-DCMAKE_MAKE_PROGRAM=$Ninja" "-DPython3_EXECUTABLE=$PythonExecutable" @CaptureArguments
    if ($LASTEXITCODE) { throw 'CMake configure failed.' }
    $Arguments = @('--build', '--preset', $Preset, '--parallel', '8')
    if ($Target) { $Arguments += @('--target', $Target) }
    & $Cmake @arguments
    if ($LASTEXITCODE) { throw 'Build failed.' }
    if ($Test) {
        & $Ctest --preset $Preset --output-on-failure
        if ($LASTEXITCODE) { throw 'CTest failed.' }
    }
} finally { Pop-Location }
