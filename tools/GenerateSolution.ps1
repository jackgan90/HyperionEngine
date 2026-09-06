param(
    [ValidateSet('auto', '2022', '2026')][string]$VisualStudioVersion = 'auto',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [switch]$Build,
    [switch]$Test,
    [switch]$Open,
    [switch]$Fresh,
    [switch]$RenderDoc,
    [switch]$NoRenderDoc,
    [switch]$Tracy,
    [switch]$NoTracy
)
$ErrorActionPreference = 'Stop'
if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    throw 'Python 3.10+ is required. Install Python and ensure the python command works.'
}
$Helper = Join-Path $PSScriptRoot 'VisualStudio.py'
$Arguments = @($Helper, '--vs-version', $VisualStudioVersion, '--configuration', $Configuration)
if ($Build) { $Arguments += '--build' }
if ($Test) { $Arguments += '--test' }
if ($Open) { $Arguments += '--open' }
if ($Fresh) { $Arguments += '--fresh' }
if ($RenderDoc -and $NoRenderDoc) { throw 'Use either -RenderDoc or -NoRenderDoc.' }
if ($RenderDoc) { $Arguments += '--renderdoc' }
if ($NoRenderDoc) { $Arguments += '--no-renderdoc' }
if ($Tracy -and $NoTracy) { throw 'Use either -Tracy or -NoTracy.' }
if ($Tracy) { $Arguments += '--tracy' }
if ($NoTracy) { $Arguments += '--no-tracy' }
& python @arguments
if ($LASTEXITCODE) { throw "Visual Studio workflow failed (exit code $LASTEXITCODE). See the error above." }
