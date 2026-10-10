# Ripes sweep of every distance-11 state listed by verify_gates --list11.
# Before running, rebuild in the BusyBox shell:
#   build/rubik_rv32i_cli.s  (the three-line command in tools/strip_render.awk)
#   build/dist11.csv         (./verify_gates.exe --list11 > build/dist11.csv)
# Usage (Windows PowerShell 5.1, from the repo root):
#   powershell -ExecutionPolicy Bypass -File tools\sweep.ps1 [-Limit N] [-SummaryOnly]
# Each finished state is appended to build/sweep.csv at once, so running the
# script again continues with the states that have no row yet. The five states
# measured earlier run first, so a short -Limit trial checks this harness.
param(
    [int]$Limit = 0,
    [switch]$SummaryOnly,
    [string]$Ripes = 'C:\Users\ASUS\Desktop\Ripes-continuous\Ripes-v2.2.6-106-g5b8a616-win-x86_64\Ripes.exe'
)
$ErrorActionPreference = 'Stop'
$repo   = Split-Path -Parent $PSScriptRoot
$build  = Join-Path $repo 'build'
$cli    = Join-Path $build 'rubik_rv32i_cli.s'
$list   = Join-Path $build 'dist11.csv'
$csv    = Join-Path $build 'sweep.csv'
$outDir = Join-Path $build 'sweep'
$work   = Join-Path $build 'sweep_cur.s'
$budget = 50000000
# a0 at exit is the last string printed: msg_pass sits at .data 0x10000000
# + test_count (4) + three 16-byte test entries (48) + msg_colon (3).
$passA0 = 0x10000037
$knownOrder = '21345671111111', '54721631111111', '14325671111111', '41752632313211', '21354672313211'
$known = @{ '21345671111111' = 6917670; '54721631111111' = 18904945; '14325671111111' = 18584269;
            '41752632313211' = 18132625; '21354672313211' = 17943030 }

function Read-Results {
    $rows = @{}
    if (Test-Path $csv) {
        foreach ($r in Import-Csv $csv) {
            if ($r.seconds) { $rows[$r.state] = $r }
        }
    }
    return $rows
}

if (-not (Test-Path $list)) { throw "missing $list; run verify_gates --list11 first" }
$states = @(Import-Csv $list)
if ($states.Count -ne 2644) { throw "$list has $($states.Count) states, expected 2644" }
$byState = @{}
foreach ($s in $states) {
    if ($s.state -notmatch '^[1-7]{7}[1-3]{7}$') { throw "bad state '$($s.state)' in $list" }
    $byState[$s.state] = $s
}
foreach ($k in $knownOrder) { if (-not $byState.ContainsKey($k)) { throw "$k is not in $list" } }

if (-not $SummaryOnly) {
    if (-not (Test-Path $Ripes)) { throw "Ripes not found: $Ripes" }
    if (-not (Test-Path $cli)) { throw "missing $cli; rebuild it in BusyBox first" }
    if ((Get-Item $cli).LastWriteTime -lt (Get-Item (Join-Path $repo 'rubik_rv32i.s')).LastWriteTime) {
        throw "$cli is older than rubik_rv32i.s; rebuild it in BusyBox first"
    }
    if (Get-Process -Name Ripes -ErrorAction SilentlyContinue) {
        throw 'a Ripes process is running (GUI or a run left by an interrupted sweep); close it first'
    }
    $src = [IO.File]::ReadAllText($cli)
    $inputLine = 'input_str: .asciz "21345671111111"'
    $countLine = 'test_count: .word 3'
    foreach ($l in $inputLine, $countLine) {
        $n = [regex]::Matches($src, [regex]::Escape($l)).Count
        if ($n -ne 1) { throw "expected one '$l' in $cli, found $n" }
    }
    $base = $src.Replace($countLine, 'test_count: .word 1')
    $noBom = New-Object System.Text.UTF8Encoding $false
    New-Item -ItemType Directory -Force $outDir | Out-Null
    if ((Test-Path $csv) -and (Get-Item $csv).Length -gt 0) {
        # An interrupted append may leave the last row without its line end.
        if (-not [IO.File]::ReadAllText($csv).EndsWith("`n")) { [IO.File]::AppendAllText($csv, "`r`n") }
    } else {
        [IO.File]::WriteAllText($csv, "state,nodes,iret,x10,exit,seconds`r`n")
    }

    $done = Read-Results
    $queue = @($knownOrder | ForEach-Object { $byState[$_] }) + @($states | Where-Object { -not $known.ContainsKey($_.state) })
    $todo = @($queue | Where-Object { -not $done.ContainsKey($_.state) })
    if ($Limit -gt 0 -and $todo.Count -gt $Limit) { $todo = @($todo[0..($Limit - 1)]) }
    $left = @($states | Where-Object { -not $done.ContainsKey($_.state) }).Count
    $spent = 0.0
    for ($i = 0; $i -lt $todo.Count; $i++) {
        $s = $todo[$i]
        [IO.File]::WriteAllText($work, $base.Replace($inputLine, "input_str: .asciz `"$($s.state)`""), $noBom)
        $out = Join-Path $outDir "$($s.state).txt"
        if (Test-Path $out) { Remove-Item $out }
        $clock = [Diagnostics.Stopwatch]::StartNew()
        $p = Start-Process -FilePath $Ripes -Wait -PassThru -ArgumentList "--mode cli --src `"$work`" -t asm --proc RV32_ISS --iret --regs --timeout 600000 --output `"$out`""
        $clock.Stop()
        $iret = ''; $x10 = ''
        if (Test-Path $out) {
            $report = @(Get-Content $out)
            $k = [array]::IndexOf($report, '===== instructions retired')
            if ($k -ge 0 -and $k + 1 -lt $report.Count) { $iret = $report[$k + 1].Trim() }
            $line = $report | Where-Object { $_ -match '^x10:' } | Select-Object -First 1
            if ($line) { $x10 = ($line -split '\s+')[1] }
        }
        $sec = [math]::Round($clock.Elapsed.TotalSeconds, 2)
        [IO.File]::AppendAllText($csv, ('{0},{1},{2},{3},{4},{5}' -f $s.state, $s.nodes, $iret, $x10, $p.ExitCode, $sec) + "`r`n")
        $spent += $sec
        '[{0}/{1}] {2} iret={3} x10={4} exit={5} {6:N2}s  sweep left ~{7:N1} h' -f ($i + 1), $todo.Count, $s.state,
            $iret, $x10, $p.ExitCode, $sec, ($spent / ($i + 1) * ($left - $i - 1) / 3600)
    }
}

$rows = Read-Results
$missing = @($states | Where-Object { -not $rows.ContainsKey($_.state) }).Count
$fail = @($rows.Values | Where-Object { -not ($_.exit -eq '0' -and $_.iret -match '^\d+$' -and $_.x10 -eq "$passA0") })
''
'=== sweep: {0} of {1} states done, {2} missing; PASS {3}, FAIL {4} ===' -f $rows.Count, $states.Count, $missing,
    ($rows.Count - $fail.Count), $fail.Count
foreach ($r in @($fail | Select-Object -First 10)) {
    '  FAIL {0} iret={1} x10={2} exit={3}' -f $r.state, $r.iret, $r.x10, $r.exit
}
$measured = @($rows.Values | Where-Object { $_.iret -match '^\d+$' })
if ($measured.Count -gt 0) {
    $sorted = @($measured | Sort-Object { [long]$_.iret })
    $max = $sorted[-1]; $min = $sorted[0]
    $sum = 0L; foreach ($r in $measured) { $sum += [long]$r.iret }
    $over = @($measured | Where-Object { [long]$_.iret -gt $budget }).Count
    $perNode = @($measured | ForEach-Object { [double]$_.iret / [double]$_.nodes } | Sort-Object)
    $secs = 0.0; foreach ($r in $rows.Values) { $secs += [double]$r.seconds }
    'max   {0,12:N0}  {1}  ({2:N1}% below 5e7)' -f [long]$max.iret, $max.state, ((1 - [long]$max.iret / $budget) * 100)
    'min   {0,12:N0}  {1}' -f [long]$min.iret, $min.state
    'mean  {0,14:N1}' -f ($sum / $measured.Count)
    'over 5e7: {0}' -f $over
    'iret per node: {0:N2} .. {1:N2}' -f $perNode[0], $perNode[-1]
    'Ripes time: {0:N2} h, {1:N2} s per state, full sweep ~{2:N1} h' -f ($secs / 3600), ($secs / $rows.Count),
        ($secs / $rows.Count * $states.Count / 3600)
}
foreach ($k in $knownOrder) {
    if ($rows.ContainsKey($k)) {
        $tag = if ($rows[$k].iret -eq "$($known[$k])") { 'OK' } else { 'MISMATCH' }
        '  check {0}: {1} vs {2} measured before  {3}' -f $k, $rows[$k].iret, $known[$k], $tag
    } else {
        '  check {0}: not run yet' -f $k
    }
}
