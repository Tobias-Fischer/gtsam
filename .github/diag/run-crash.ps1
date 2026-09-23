$ErrorActionPreference = "Continue"
Set-Location build/bin

foreach ($exe in @("testUnit3.exe", "testRot3.exe")) {
    Write-Host "=============================================================="
    Write-Host "== $exe"
    Write-Host "=============================================================="
    if (-not (Test-Path $exe)) { Write-Host "not built"; continue }

    & ".\$exe" 2>&1 | Select-Object -First 20
    $code = $LASTEXITCODE
    Write-Host ("-- exit code: 0x{0:X8} ({0})" -f $code)

    # 0xC0000005 access violation, 0xC0000139 entry point not found,
    # 0xC0000135 DLL not found -- these mean very different things.
    switch ($code) {
        -1073741819 { Write-Host "-- STATUS_ACCESS_VIOLATION" }
        -1073741511 { Write-Host "-- STATUS_ENTRYPOINT_NOT_FOUND" }
        -1073741515 { Write-Host "-- STATUS_DLL_NOT_FOUND" }
        -1073740791 { Write-Host "-- STATUS_STACK_BUFFER_OVERRUN" }
        0           { Write-Host "-- clean exit" }
    }
}

$cdb = "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe"
if (Test-Path $cdb) {
    Write-Host "=============================================================="
    Write-Host "== stack for testUnit3.exe under cdb"
    Write-Host "=============================================================="
    & $cdb -g -G -c ".lastevent; k 40; lm; q" .\testUnit3.exe 2>&1 | Select-Object -Last 120
} else {
    Write-Host "cdb not present at $cdb"
}
