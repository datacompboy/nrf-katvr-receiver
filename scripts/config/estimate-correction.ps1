## VVVV555

. .\z_kat-lib.ps1
LoadKatFunctions
#
$devNo = -1
$dev = FindKatTreadmill([ref]$devNo) $false
#

function Start-Stream {
    $ans = KatSendHIDCommand $dev.serialNumber @(,0x30)
    if ($ans[5] -ne 0) {
        return
    }
    $ans = KatSendHIDCommand $dev.serialNumber @(,0x30)
    if ($ans[5] -ne 0) {
        return
    }
    $ans = KatSendHIDCommand $dev.serialNumber @(,0x30)
    if ($ans[5] -ne 0) {
        return
    }
    Write-Host ([System.BitConverter]::ToString($ans))
    throw "Can't start stream, sorry"
}

function Stop-Stream {
    $ans = KatSendHIDCommand $dev.serialNumber @(,0x31)
    Start-Sleep Seconds 1
    $ans = KatSendHIDCommand $dev.serialNumber @(,0x31)
}

function Run-Estimate([ref]$avgLeft, [ref]$avgRight) {
    $angleLeft = 0.0
    $angleLeftCnt = 0

    $angleRight = 0.0
    $angleRightCnt = 0

    [byte[]]$command = New-Object byte[] 64
    [byte[]]$ans = New-Object byte[] 32

    Write-Host "Please do a few steps..."
    while($null = Invoke-NulRedirected { [IBizLibrary.KATSDKInterfaceHelper]::SendHIDCommand($dev.serialNumber, $command, 0, $ans, 32) }) {
        if ($ans[5] -eq 48 -and $ans[6] -gt 0) { # Leg data packet
            $X = [System.BitConverter]::ToInt16($ans, 21)
            $Y = [System.BitConverter]::ToInt16($ans, 23)
            $RightLeg = $ans[6] -eq 2
            
            if ($Y -gt 50) {
                $angle = [System.Math]::Atan2($Y, $X) - [System.Math]::PI/2.0

                if ($RightLeg) {
                    $angleRight += $angle
                    $angleRightCnt += 1
                    Write-Host -NoNewline "R"
                    #Write-Host "R " $Y $X  $angle
                } else {
                    $angleLeft += $angle
                    $angleLeftCnt += 1
                    Write-Host -NoNewline "L"
                }
            }
        }
        if ($angleLeftCnt -gt 50 -and $angleRightCnt -gt 50) {
            break;
        }
    }

    $avgLeft.value = ($angleLeft / $angleLeftCnt) * 180.0 / [System.Math]::PI
    $avgRight.value = ($angleRight / $angleRightCnt) * 180.0 / [System.Math]::PI
    Write-Host ""

    return 
}

if ($false) {
    Stop-Stream

    Write-Host "==== Original estimations:"
    .\get-correction.ps1

    Write-Host "==== Reset to zero"
    .\set-correction.ps1 -LeftCorrectionDeg 0 -RightCorrectionDeg 0
}
Start-Stream
Start-Sleep 5

$avgLeft = 0.0
$avgRight = 0.0    
Run-Estimate ([ref]$avgLeft) ([ref]$avgRight)
Write-Host "Average left angle: " $avgLeft
Write-Host "Average right angle: " $avgRight

if ($false) {
    .\set-correction.ps1 -LeftCorrectionDeg (-$avgLeft) -RightCorrectionDeg (-$avgRight)

    Write-Host "==== Test the correction:"
    Run-Estimate ([ref]$avgLeft) ([ref]$avgRight)
    Write-Host "Corrected left angle: " $avgLeft
    Write-Host "Corrected right angle: " $avgRight

    Stop-Stream
}
