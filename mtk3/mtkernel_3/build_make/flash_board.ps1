Write-Host "=== Cleaning previous build ===" -ForegroundColor Cyan
make clean

Write-Host ""
Write-Host "=== Compiling firmware ===" -ForegroundColor Cyan
make all

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "[!] Build failed! Fix your C code errors and try again." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "=== Erasing the micro:bit ===" -ForegroundColor Cyan
pyocd erase --mass

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "[!] Failed to erase the board. Is it plugged in?" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "=== Flashing new firmware ===" -ForegroundColor Cyan
pyocd load mtkernel_3.elf

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "[OK] Success! Firmware loaded and running." -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "[!] Flashing failed." -ForegroundColor Red
}