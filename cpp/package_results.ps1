param([string]$Node = 'node')
$ErrorActionPreference = 'Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    & $Node python/verify_level6.cjs cpp/outputs
    if ($LASTEXITCODE -ne 0) { throw 'Original simulator rejected the outputs.' }
    $report = Get-Content cpp/results.json -Raw | ConvertFrom-Json
    $improvedTotal = 0
    $readme = Get-Content cpp/README.md -Raw
    foreach ($entry in $report.datasets) {
        $before = (Get-Content (Join-Path cpp/baseline_e6d1c9c $entry.file) -Raw).Trim() -split '\r?\n\s*\r?\n'
        $after = (Get-Content (Join-Path cpp/outputs $entry.file) -Raw).Trim() -split '\r?\n\s*\r?\n'
        if ($before.Count -ne $entry.cases -or $after.Count -ne $entry.cases) { throw 'Wrong case count.' }
        $sumBefore = 0; $sumAfter = 0; $improved = 0
        for ($i = 0; $i -lt $before.Count; $i++) {
            $b = ($before[$i] -split '\r?\n').Count
            $a = ($after[$i] -split '\r?\n').Count
            if ($a -gt $b) { throw "Regression: $($entry.file), case $i" }
            $sumBefore += $b; $sumAfter += $a
            if ($a -lt $b) { $improved++ }
        }
        $entry.before = $sumBefore; $entry.after = $sumAfter
        $entry.saved = $sumBefore - $sumAfter; $entry.improved_cases = $improved
        $improvedTotal += $improved
        $name = ($entry.file -replace '^out_level-6_\d-', '') -replace '\.txt$', ''
        $readme = $readme -replace "(?m)^\| $name \|.*$", "| $name | $sumBefore | $sumAfter |"
        Copy-Item -LiteralPath (Join-Path cpp/outputs $entry.file) -Destination (Join-Path python/outputs $entry.file) -Force
    }
    $report.scored_after = [long]($report.datasets | Select-Object -Skip 2 | Measure-Object after -Sum).Sum
    $report.all_after = [long]($report.datasets | Measure-Object after -Sum).Sum
    $report.recorded_utc = [DateTime]::UtcNow.ToString('o')
    $readme = $readme -replace '(?m)^\| medium \+ large \+ enormous \|.*$', "| medium + large + enormous | $($report.scored_before) | **$($report.scored_after)** |"
    $readme = $readme -replace '(?m)^\| Wszystkie pliki \|.*$', "| Wszystkie pliki | $($report.all_before) | **$($report.all_after)** |"
    $readme = $readme -replace 'Poprawiono \d+ przypadków;', "Poprawiono $improvedTotal przypadków;"
    [IO.File]::WriteAllText((Join-Path $PWD cpp/README.md), $readme, [Text.UTF8Encoding]::new($false))
    $report | ConvertTo-Json -Depth 8 | Set-Content cpp/results.json -Encoding utf8
    Compress-Archive -Path cpp/outputs/out_level-6_*.txt -DestinationPath cpp/level6_submission.zip -Force
    $zip = [IO.Compression.ZipFile]::OpenRead((Join-Path $PWD cpp/level6_submission.zip))
    try {
        if ($zip.Entries.Count -ne 5) { throw 'Wrong ZIP entry count.' }
        foreach ($entry in $zip.Entries) {
            $stream = $entry.Open(); $sha = [Security.Cryptography.SHA256]::Create()
            try { $hash = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
            finally { $stream.Dispose(); $sha.Dispose() }
            if ($hash -ne (Get-FileHash (Join-Path cpp/outputs $entry.Name)).Hash) { throw "ZIP mismatch: $($entry.Name)" }
        }
    } finally { $zip.Dispose() }
    Write-Output "Scored: $($report.scored_after); all files: $($report.all_after); improved cases: $improvedTotal"
} finally { Pop-Location }
