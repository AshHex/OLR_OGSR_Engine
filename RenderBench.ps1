# Unattended render benchmark on several renderers (see gamedata\config\misc\render_bench.ltx).
#   powershell -File RenderBench.ps1                                   - all scenarios from [general], r1..r4
#   powershell -File RenderBench.ps1 -Bench "npc+water" -Renders r1,r2 - selected scenarios / renderers
#   powershell -File RenderBench.ps1 -Save mysave                      - another save game (default: all)
# Every run loads the save, waits for the start-up cutscene and post-process effects, runs the benchmark and quits.
# Results: appdata\logs\render_bench_<render>_<level>.txt, appdata\logs\shader_audit_<render>.txt,
#          appdata\screenshots\bench\<render>\<level>\ (same file names on every renderer - compare them side by side).
# Renderer settings are taken from appdata\test_<render>.ltx.
param(
    [string]$Bench = "cfg",
    [string[]]$Renders = @("r1", "r2", "r3", "r4"),
    [string]$Save = "all",
    [int]$TimeoutMinutes = 30
)
$Renders = @($Renders | ForEach-Object { $_ -split "," } | Where-Object { $_ })
$root = $PSScriptRoot
foreach ($r in $Renders) {
    $ltx = "test_$r.ltx"
    if (-not (Test-Path (Join-Path $root "appdata\$ltx"))) { Write-Warning "appdata\$ltx not found - skipped"; continue }
    Write-Host "[$r] running: $Bench"
    $p = Start-Process -FilePath "$root\bin_x64\xrEngine.exe" -WorkingDirectory $root -PassThru `
        -ArgumentList "-ltx $ltx -nointro -rbench $Bench -start server($Save/single/alife/load) client(localhost)"
    if (-not $p.WaitForExit($TimeoutMinutes * 60000)) {
        Stop-Process -Id $p.Id -Force
        Write-Warning "[$r] timeout after $TimeoutMinutes min - killed"
    } else {
        Write-Host "[$r] exit code $($p.ExitCode)"
    }
}
