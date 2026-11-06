# hook-gate.ps1 - GUI hook-scenario gate: launch the player with the
# in-process event hook (BILILOCAL_EMBED_TEST=1) against the generated
# local media fixture, then assert three log markers:
#   hover-test: seeked to 10%   playback started and duration is ready
#   hover: freeze "..."         a danmaku froze under the held cursor
#   hover: release (left)       the frozen danmaku released on sweep
# Exit 0 only when all three appear within the timeout.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\hook-gate.ps1
#        [-Exe path] [-Dist path] [-Media path] [-Python path] [-Root path]
#        [-TimeoutSec 120]

param(
    [string]$Exe = '',
    [string]$Dist = '',
    [string]$Media = '',
    [string]$Python = '',
    [string]$Root = '',
    [int]$TimeoutSec = 120
)

$ErrorActionPreference = 'Continue'

# Resolve the project root by walking up until dist exists, so the same file
# runs from <root>\tools\ and <root>\src\tools\. Never hard-code a machine
# path; every runtime location can be overridden on the command line.
function Find-ProjectRoot([string]$start) {
    $p = $start
    for ($i = 0; $i -lt 5; $i++) {
        if (Test-Path (Join-Path $p 'dist')) { return $p }
        $parent = Split-Path -Parent $p
        if (-not $parent -or $parent -eq $p) { break }
        $p = $parent
    }
    return $null
}
$root = $Root
if (-not $root) { $root = Find-ProjectRoot $PSScriptRoot }
if (-not $root) { Write-Output 'project root not found - pass -Root <path>'; exit 1 }
# Runtime root: Qt/VLC DLLs + plugins beside the exe. Defaults to dist;
# a parallel tree (e.g. dist-msvc) passes -Dist so staging swaps the right ABI.
if (-not $Exe)  { $Exe  = Join-Path $root 'dist\BiliLocal.exe' }
if (-not $Dist) { $Dist = Join-Path $root 'dist' }
$py = Join-Path $env:USERPROFILE '.dsh\dsh-runtimes\dsh-next-primary-runtime\dependencies\python\python.exe'
if ($Python) { $py = $Python }
if (-not (Test-Path $py)) {
    $candidate = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($candidate) { $py = $candidate.Source }
}
$mediaDir = Join-Path $PSScriptRoot '.media'
$y4m      = Join-Path $mediaDir 'sample-60s.y4m'
$xml      = Join-Path $mediaDir 'sample-60s.xml'

if ($Media) {
    # external media (e.g. a real Z: recording): danmaku comes from its
    # sibling .xml via the %{File}.xml template, same as production
    $y4m = (Get-Item $Media).FullName
    $xml = [System.IO.Path]::ChangeExtension($y4m, '.xml')
    if (-not (Test-Path $xml)) { Write-Output ("SIBLING XML MISSING: " + $xml); exit 1 }
}
else {
    if (-not (Test-Path $Exe)) { Write-Output ("EXE MISSING: " + $Exe); exit 1 }
    if (-not (Test-Path $y4m) -or -not (Test-Path $xml)) {
        Write-Output 'media fixture missing, generating...'
        & $py (Join-Path $PSScriptRoot 'gen-media.py') $mediaDir
        if ($LASTEXITCODE -ne 0) { Write-Output 'gen-media FAILED'; exit 1 }
    }
}

Get-Process BiliLocal -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800
$log = "$env:LOCALAPPDATA\BiliLocal\cache\msg.log"
Remove-Item $log -ErrorAction SilentlyContinue

# Pitfall 46: env vars alone cannot start an exe outside dist - the loader
# needs the runtime DLLs and the plugins dir beside it. Stage them once for
# any non-dist exe (build trees are disposable; the junction is a link only).
$exeDir = ((Split-Path -Parent $Exe) -replace '/', '\').TrimEnd('\')
$distN  = ($Dist -replace '/', '\').TrimEnd('\')
if ($exeDir -ne $distN) {
    if (-not (Test-Path (Join-Path $exeDir 'plugins'))) {
        cmd /c mklink /J (Join-Path $exeDir 'plugins') (Join-Path $Dist 'plugins') | Out-Null
    }
    Copy-Item (Join-Path $Dist '*.dll') $exeDir -Force
}

# The hook reads this variable at EWindow construction; child inherits env.
$env:BILILOCAL_EMBED_TEST = '1'
# Non-dist exes (e.g. build\release) have no Qt/VLC runtime beside them;
# point the child at the runtime root explicitly, or it dies on the platform plugin.
$env:PATH = $Dist + ';' + $env:PATH
$env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $Dist 'platforms'
$env:VLC_PLUGIN_PATH = Join-Path $Dist 'plugins'

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $Exe
$psi.WorkingDirectory = $Dist
$psi.Arguments = '"' + $y4m + '"'
$psi.UseShellExecute = $false
# Iron rule: redirect child stdio, or the caller's pipeline hangs on it.
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$p = [System.Diagnostics.Process]::Start($psi)

$want = @('hover-test: seeked to 10%', 'hover: freeze ', 'hover: release (left)')
$found = @{}
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 1
    if ($p.HasExited) {
        Write-Output ('PROCESS EXITED EARLY, code=' + $p.ExitCode)
        break
    }
    if (Test-Path $log) {
        $tail = Get-Content $log -Tail 400 -ErrorAction SilentlyContinue
        foreach ($m in $want) {
            if (-not $found.ContainsKey($m)) {
                if ($tail | Select-String -SimpleMatch $m -Quiet) { $found[$m] = $true }
            }
        }
    }
    if ($found.Count -eq $want.Count) { break }
}

$alive = -not $p.HasExited
Get-Process BiliLocal -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item Env:BILILOCAL_EMBED_TEST -ErrorAction SilentlyContinue

foreach ($m in $want) {
    $state = 'MISS'
    if ($found.ContainsKey($m)) { $state = 'OK' }
    Write-Output ('[' + $state + '] ' + $m)
}
Write-Output ('alive-when-checked=' + $alive)

if ($found.Count -eq $want.Count) {
    Write-Output 'HOOK GATE: PASS'
    exit 0
}
Write-Output 'HOOK GATE: FAIL'
exit 1
