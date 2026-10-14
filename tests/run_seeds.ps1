# Run the simulation test with many seeds and report only failing ones.
# Usage: .\tests\run_seeds.ps1 [-BuildDir build] [-Count 100]
#   e.g. .\tests\run_seeds.ps1 -BuildDir "build/INTERCOM[SimTests]" -Count 100
un_seeds.ps1 -BuildDir "build/INTERCOM[SimTests]" -Count 100
param(
    [string]$BuildDir = "build",
    [int]$Count = 100
)

$candidates = @(
    (Join-Path $BuildDir "intercom_sim_test.exe"),
    (Join-Path $BuildDir "Debug/intercom_sim_test.exe"),
    (Join-Path $BuildDir "Release/intercom_sim_test.exe"),
    (Join-Path $BuildDir "tests/intercom_sim_test.exe"),
    (Join-Path $BuildDir "tests/Debug/intercom_sim_test.exe"),
    (Join-Path $BuildDir "tests/Release/intercom_sim_test.exe"),
    (Join-Path $BuildDir "tests/intercom_sim_test")
)
$exe = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $exe) {
    Write-Host "test executable not found in $BuildDir (build with -DINTERCOM_BUILD_TESTS=ON)"
    exit 2
}

$fails = 0
for ($i = 1; $i -le $Count; $i++) {
    $seed = $i * 7919
    $out = & $exe $seed 2>&1 | Out-String
    if ($out -notmatch "ALL TESTS PASSED") {
        $fails++
        Write-Host "=== seed $seed ==="
        ($out -split "`n") | Select-String -Pattern "FAIL|ERROR|scenario" | Select-Object -First 40 | ForEach-Object { Write-Host $_ }
    }
}
Write-Host "failed seeds: $fails/$Count"
if ($fails -ne 0) { exit 1 }
