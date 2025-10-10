# Script configuration
. .\z_kat-lib.ps1
$null = LoadKatFunctions
#
$devNo = -1
$dev = FindKatTreadmill([ref]$devNo)
#
$ans = KatSendHIDCommand $dev.serialNumber @(0xDC,0xF0)
if ($ans[5] -ne 0xDC -and $ans[6] -ne 0xF0) {
    throw "No answer from device, please try again"
}
$Freq  = [System.BitConverter]::ToInt16($ans, 7)

Write-Host "The receiver is set up to " $Freq " Hz"
