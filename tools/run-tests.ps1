# run-tests.ps1 - build and run every unit test target in one shot (CMake + Qt 6).
# Replaced the qmake/MinGW version in Phase 2 (qmake trees deleted, see HANDOVER 10).
# Usage (PS 5.1): powershell -NoProfile -ExecutionPolicy Bypass -File tools\run-tests.ps1
#        [-Root <project root>]
# Exit code 0 = all targets green; nonzero = configure/build/test failure.

param(
    [string]$Root = ''
)

$ErrorActionPreference = 'Stop'

# Resolve the project root by walking up until build-cmake6 exists, so the same
# file runs from <root>\tools\ (local deployment) and <root>\src\tools\ (git).
# Never hard-code a machine path; pass -Root to override.
function Find-ProjectRoot([string]$start) {
    $p = $start
    for ($i = 0; $i -lt 5; $i++) {
        if (Test-Path (Join-Path $p 'build-cmake6')) { return $p }
        $parent = Split-Path -Parent $p
        if (-not $parent -or $parent -eq $p) { break }
        $p = $parent
    }
    return $null
}
$root = $Root
if (-not $root) { $root = Find-ProjectRoot $PSScriptRoot }
if (-not $root) { Write-Error 'project root not found - pass -Root <path>' }
$build = Join-Path $root 'build-cmake6'
$qt    = Join-Path $root 'deps\6.8.3\msvc2022_64\bin'
$scripts = Join-Path $env:USERPROFILE '.dsh\dsh-runtimes\dsh-next-primary-runtime\dependencies\python\Scripts'
$cmake  = Join-Path $scripts 'cmake.exe'

# Prefer the native VS CMake ctest: the pip cmake shim under dsh-runtimes was
# observed to leave tst_*.exe unable to resolve Qt6Core.dll (0xc0000135) in
# some shells, while the native ctest is 6/6 green from the Git shell with the
# same PATH. Locate it via vswhere - never pin a VS version (pitfall 51).
$ctest = $null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path $vswhere) {
    $found = & $vswhere -latest -products * -find '**\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' 2>$null
    if ($found) { $ctest = @($found)[0] }
}
if (-not $ctest) {
    $onPath = Get-Command ctest.exe -ErrorAction SilentlyContinue
    if ($onPath -and $onPath.Source -notlike '*dsh-runtimes*') { $ctest = $onPath.Source }
}
if (-not $ctest) { $ctest = Join-Path $scripts 'ctest.exe' }

foreach ($tool in @($cmake, $ctest)) {
    if (-not (Test-Path $tool)) {
        Write-Error "tool not found: $tool - check the dsh runtime / VS CMake location"
    }
}
if (-not (Test-Path (Join-Path $build 'CMakeCache.txt'))) {
    Write-Error ("build tree missing: " + $build + " - configure first (HANDOVER section 4)")
}

& $cmake --build $build --config Release -j8
if ($LASTEXITCODE -ne 0) { Write-Output 'BUILD FAILED'; exit 1 }

# Qt6Test/Qt6Core DLLs must be on the loader's search path for the test exes
$env:PATH = "$qt;" + $env:PATH
& $ctest --test-dir $build --output-on-failure -C Release
if ($LASTEXITCODE -ne 0) { Write-Output 'TESTS FAILED'; exit 1 }
Write-Output 'RESULT: all test targets passed'
exit 0
