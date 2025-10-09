set /p serial=Enter desired serial number: 
powershell.exe -ExecutionPolicy Bypass -File write-serial.ps1 --Serial %serial%
pause
