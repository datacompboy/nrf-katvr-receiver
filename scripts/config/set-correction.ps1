param (
    [single]$LeftCorrectionDeg = -20.0,
    [single]$RightCorrectionDeg = 20.0
)
. .\z_kat-lib.ps1
LoadKatFunctions
#
$devNo = -1
$dev = FindKatTreadmill([ref]$devNo)
#
$LeftCorrection = $LeftCorrectionDeg * [System.Math]::PI / 180.0
$RightCorrection = $RightCorrectionDeg * [System.Math]::PI / 180.0
$LeftCorrectionArr = [System.BitConverter]::GetBytes([Single]$LeftCorrection)
$RightCorrectionArr = [System.BitConverter]::GetBytes([Single]$RightCorrection)
#
$ans = KatSendHIDCommand $dev.serialNumber @(0xDD,0xFA + $LeftCorrectionArr + $RightCorrectionArr)
if ($ans[5] -ne 0xDD -and $ans[6] -ne 0xFA) {
    throw "No answer from device, please try again"
}

$LeftCorrection = [System.BitConverter]::ToSingle($ans, 7)
$RightCorrection = [System.BitConverter]::ToSingle($ans, 11)

Write-Host("Left foot correction: " + ($LeftCorrection*180.0/[System.Math]::PI))
Write-Host("Right foot correction: " + ($RightCorrection*180.0/[System.Math]::PI))
