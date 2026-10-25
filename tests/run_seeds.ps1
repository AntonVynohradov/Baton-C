# Run the bus simulation (baton_sim) with many seeds and report only failing ones.
# Usage: .\tests\run_seeds.ps1 [-BuildDir build] [-Count 100]
#   e.g. .\tests\run_seeds.ps1 -BuildDir "build/BATON[UnitTests]" -Count 100
param(
    [string]$BuildDir = "build",
    [int]$Count = 100
)

$candidates = @(
    (Join-Path $BuildDir "baton_sim.exe"),
    (Join-Path $BuildDir "Debug/baton_sim.exe"),
    (Join-Path $BuildDir "Release/baton_sim.exe"),
    (Join-Path $BuildDir "tests/baton_sim.exe"),
    (Join-Path $BuildDir "tests/Debug/baton_sim.exe"),
    (Join-Path $BuildDir "tests/Release/baton_sim.exe"),
    (Join-Path $BuildDir "tests/baton_sim")
)
$exe = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $exe) {
    Write-Host "test executable not found in $BuildDir (build with -DBATON_UNIT_TESTS=ON, target baton_sim)"
    exit 2
}

# Start the runner through .NET: '& $exe' treats the brackets of preset folders such as
# build/BATON[UnitTests] as a wildcard pattern and does not find the program.
$exePath = (Get-Item -LiteralPath $exe).FullName

function Invoke-Simulation([int]$Seed) {
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $exePath
    $info.Arguments = "$Seed"
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::Start($info)
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    return $stdout.Result + $stderr.Result
}

$fails = 0
for ($i = 1; $i -le $Count; $i++) {
    $seed = $i * 7919
    $out = Invoke-Simulation $seed
    if ($out -notmatch "ALL TESTS PASSED") {
        $fails++
        Write-Host "=== seed $seed ==="
        ($out -split "`n") | Select-String -Pattern "FAIL|ERROR|scenario" | Select-Object -First 40 | ForEach-Object { Write-Host $_ }
    }
}
Write-Host "failed seeds: $fails/$Count"
if ($fails -ne 0) { exit 1 }
