# Script configuration
. .\z_kat-lib.ps1
LoadKatFunctions
#
$devNo = -1
$dev = FindKatTreadmill([ref]$devNo)
#
$ans = KatSendHIDCommand $dev.serialNumber @(0xDC,0xFA)
if ($ans[5] -ne 0xDC -and $ans[6] -ne 0xFA) {
    throw "No answer from device, please try again"
}

$LeftCorrection = [System.BitConverter]::ToSingle($ans, 7)
$RightCorrection = [System.BitConverter]::ToSingle($ans, 11)

Write-Host("Left foot correction: " + ($LeftCorrection*180.0/3.1415926535))
Write-Host("Right foot correction: " + ($RightCorrection*180.0/3.1415926535))
