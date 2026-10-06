param(
    [ValidateSet('debug','release','relwithdebinfo','profile')][string]$Preset = 'debug',
    [string]$Target = '',
    [switch]$Test,
    [switch]$RenderDoc,
    [switch]$NoRenderDoc,
    [switch]$Tracy,
    [switch]$NoTracy,
    [string]$OutRoot = '',
    [string]$ToolCache = '',
    [string]$BuildDirectory = '',
    [string]$Dependencies = ''
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$PreviousOutRoot = $env:HYP_OUT_ROOT
$PreviousToolCache = $env:HYP_TOOL_CACHE
if ($OutRoot) { $env:HYP_OUT_ROOT = [System.IO.Path]::GetFullPath($OutRoot) }
if ($ToolCache) { $env:HYP_TOOL_CACHE = [System.IO.Path]::GetFullPath($ToolCache) }
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
    $PathsJson = & python tools/DevelopmentPaths.py
    if ($LASTEXITCODE) { throw 'Developer paths could not be resolved.' }
    $Paths = $PathsJson | ConvertFrom-Json
    if (-not $BuildDirectory) { $BuildDirectory = Join-Path $Paths.out "build/$Preset" }
    $BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
    if (-not $Dependencies) {
        $Dependencies = $Paths.deps
        $BootstrapArguments = @('tools/Bootstrap.py', '--legacy-root', (Join-Path $ProjectRoot 'out'))
        if ($RenderDoc) { $BootstrapArguments += '--renderdoc' }
        & python @BootstrapArguments
        if ($LASTEXITCODE) { throw 'Dependency bootstrap failed.' }
    }
    if ($RenderDoc -and $NoRenderDoc) { throw 'Use either -RenderDoc or -NoRenderDoc.' }
    $CaptureArguments = @()
    if ($RenderDoc) {
        $CaptureArguments += '-DHYP_ENABLE_RENDERDOC=ON'
    }
    if ($NoRenderDoc) { $CaptureArguments += '-DHYP_ENABLE_RENDERDOC=OFF' }
    if ($Tracy -and $NoTracy) { throw 'Use either -Tracy or -NoTracy.' }
    if ($Tracy) { $CaptureArguments += '-DHYP_ENABLE_TRACY=ON' }
    if ($NoTracy) { $CaptureArguments += '-DHYP_ENABLE_TRACY=OFF' }
    & python tools/TargetGraph.py --cmake $Cmake --build-dir $BuildDirectory -- --preset $Preset -B $BuildDirectory "-DCMAKE_MAKE_PROGRAM=$Ninja" "-DPython3_EXECUTABLE=$PythonExecutable" "-DHYP_DEPS=$Dependencies" "-DHYP_OUTPUT_ROOT=$($Paths.out)" "-DHYP_TOOL_CACHE=$($Paths.'tool-cache')" -Wno-dev @CaptureArguments
    if ($LASTEXITCODE) { throw 'CMake configure failed.' }
    $Arguments = @('--build', $BuildDirectory, '--parallel', '8')
    if ($Target) { $Arguments += @('--target', $Target) }
    & $Cmake @arguments
    if ($LASTEXITCODE) { throw 'Build failed.' }
    if ($Test) {
        & $Ctest --test-dir $BuildDirectory --output-on-failure
        if ($LASTEXITCODE) { throw 'CTest failed.' }
    }
} finally {
    Pop-Location
    $env:HYP_OUT_ROOT = $PreviousOutRoot
    $env:HYP_TOOL_CACHE = $PreviousToolCache
}
