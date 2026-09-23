$ErrorActionPreference = "Continue"
$pdb = "build/bin/gtsam.pdb"
if (-not (Test-Path $pdb)) { Write-Host "no pdb at $pdb"; exit 0 }
Write-Host "pdb size: $((Get-Item $pdb).Length) bytes"

# Find the module index for Unit3.cpp, then dump only that module.
$modules = & llvm-pdbutil dump --modules $pdb 2>$null
$idx = $null
foreach ($l in $modules) {
    if ($l -match 'Mod (\d+).*Unit3\.cpp\.obj') { $idx = [int]$Matches[1]; break }
}
if ($null -eq $idx) { Write-Host "Unit3.cpp module not found"; exit 0 }
Write-Host "Unit3.cpp is module $idx"

$syms = & llvm-pdbutil dump --modi=$idx --symbols $pdb 2>$null
$name = ""
$results = @()
foreach ($l in $syms) {
    if ($l -match 'S_GPROC32 \[size = \d+\] `([^`]+)`') { $name = $Matches[1]; $code = $null; continue }
    if ($l -match 'code size = (\d+)') { $code = [int]$Matches[1]; continue }
    if ($l -match '^\s+size = (\d+), padding size') {
        if ($name -ne "") { $results += [pscustomobject]@{ Frame = [int]$Matches[1]; Code = $code; Name = $name } }
        $name = ""
    }
}
Write-Host ""
Write-Host "=== largest stack frames in Unit3.cpp ==="
$results | Sort-Object Frame -Descending | Select-Object -First 8 |
    ForEach-Object { "{0,12:N0} bytes frame {1,12:N0} bytes code  {2}" -f $_.Frame, $_.Code, $_.Name.Substring(0, [Math]::Min(80, $_.Name.Length)) }
Write-Host ""
$r = $results | Where-Object { $_.Name -eq "gtsam::Unit3::retract" }
if ($r) {
    Write-Host ("Unit3::retract frame = {0:N0} bytes (was 3,162,008 before the fix; 1 MB stack limit = 1,048,576)" -f $r.Frame)
} else {
    Write-Host "Unit3::retract not found in this module"
}
