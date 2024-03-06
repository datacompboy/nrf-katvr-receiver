powershell -Command "$COM=[System.IO.Ports.SerialPort]::getportnames()[0]; $port=new-Object System.IO.Ports.SerialPort $COM,256000,None,8,one; $port.Open(); $port.DtrEnable=1; do { Write-Host -NoNewline $port.ReadExisting(); } while ($port.IsOpen -and !([console]::KeyAvailable))"
pause
