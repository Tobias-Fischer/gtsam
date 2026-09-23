$ErrorActionPreference = "Continue"

$bin = "build/bin"
$crasher = "testUnit3.exe"
$control = "testRot3.exe"

function Show-Fault([string]$exe) {
    Write-Host "=============================================================="
    Write-Host "== $exe"
    Write-Host "=============================================================="
    if (-not (Test-Path "$bin/$exe")) { Write-Host "not built"; return }

    Push-Location $bin
    & ".\$exe" 2>&1 | Select-Object -First 20
    $code = $LASTEXITCODE
    Pop-Location

    Write-Host ("-- exit code: 0x{0:X8} ({0})" -f $code)
    switch ($code) {
        -1073741819 { Write-Host "-- STATUS_ACCESS_VIOLATION: memory fault, need a stack" }
        -1073741511 { Write-Host "-- STATUS_ENTRYPOINT_NOT_FOUND: an import is missing from a DLL" }
        -1073741515 { Write-Host "-- STATUS_DLL_NOT_FOUND: a dependent DLL is absent" }
        -1073740791 { Write-Host "-- STATUS_STACK_BUFFER_OVERRUN / __fastfail" }
        -1073741571 { Write-Host "-- STATUS_STACK_OVERFLOW" }
        0           { Write-Host "-- clean exit" }
    }
}

Show-Fault $crasher
Show-Fault $control

# If the loader is refusing an import, the missing symbol is findable without a
# debugger: compare what the exe imports from gtsam.dll against what it exports.
Write-Host "=============================================================="
Write-Host "== imports of $crasher from gtsam.dll vs exports of gtsam.dll"
Write-Host "=============================================================="
$dumpbin = Get-Command dumpbin -ErrorAction SilentlyContinue
if ($dumpbin) {
    Push-Location $bin
    $gtsamDll = Get-ChildItem -Filter "gtsam*.dll" | Select-Object -First 1
    if ($gtsamDll) {
        Write-Host "gtsam dll: $($gtsamDll.Name)"
        & dumpbin /IMPORTS:$($gtsamDll.Name) $crasher > ..\..\imports.txt 2>&1
        & dumpbin /EXPORTS $($gtsamDll.Name)                > ..\..\exports.txt 2>&1

        $imports = Select-String -Path ..\..\imports.txt -Pattern '^\s+[0-9A-F]+\s+\w+\s+(\?\S+)' `
                   | ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique
        $exports = Select-String -Path ..\..\exports.txt -Pattern '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\?\S+)' `
                   | ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique

        Write-Host "imported symbols: $($imports.Count), exported symbols: $($exports.Count)"
        $missing = $imports | Where-Object { $exports -notcontains $_ }
        if ($missing) {
            Write-Host "!! IMPORTED BUT NOT EXPORTED ($($missing.Count)):"
            $missing | Select-Object -First 40 | ForEach-Object { Write-Host "   $_" }
        } else {
            Write-Host "-- every imported symbol is exported; not a missing-export problem"
        }
        $unit3 = $exports | Where-Object { $_ -match "Unit3" }
        Write-Host "-- gtsam.dll exports $($unit3.Count) Unit3 symbols"
    } else {
        Write-Host "no gtsam dll found in $bin"
    }
    Pop-Location
} else {
    Write-Host "dumpbin not on PATH"
}

# A stack, with symbols, if the fault is a memory access violation.
Write-Host "=============================================================="
Write-Host "== cdb"
Write-Host "=============================================================="
$cdb = "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe"
if (Test-Path $cdb) {
    $sym = "$PWD\$bin;SRV*C:\Symbols*https://msdl.microsoft.com/download/symbols"
    Push-Location $bin
    & $cdb -g -G -y $sym -lines -c ".lastevent; .ecxr; kP 40; !analyze -v; q" ".\$crasher" 2>&1 `
        | Select-Object -Last 200
    Pop-Location
} else {
    Write-Host "cdb missing at $cdb"
}
