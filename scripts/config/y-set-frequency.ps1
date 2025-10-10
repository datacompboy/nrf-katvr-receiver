param (
    [int16]$Frequency = 86
)
#
. .\z_kat-lib.ps1
$null = LoadKatFunctions
#
$devNo = -1
$dev = FindKatTreadmill([ref]$devNo)
#
$ans = KatSendHIDCommand $dev.serialNumber @(0xDD,0xF0 + [System.BitConverter]::GetBytes($Frequency))
if ($ans[5] -ne 0xDD -and $ans[6] -ne 0xF0) {
    throw "No answer from device, please try again"
}
$Freq = [System.BitConverter]::ToInt16($ans, 7)

Write-Host "Receiver configuration updated to " $Freq " Hz"
