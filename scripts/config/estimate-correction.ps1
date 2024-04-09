# Script configuration
$katPath = "C:\Program Files (x86)\KAT Gateway\"

# Load Gateway's communication helper library
if (!(Test-Path $katPath -PathType Container)) {
    throw  "KAT Gateway expected to be installed in '${katPath}'"
}

if (Get-Process -name "KAT Gateway" -ErrorAction SilentlyContinue) {
    throw "Please stop KAT Gateway."
}

Add-Type -Path "C:\Program Files (x86)\KAT Gateway\IBizLibrary.dll"

#
# Detect known Receiver version
#
[IBizLibrary.ComUtility]::KatDevice='walk_c2'
if (Test-Path([IBizLibrary.ComUtility]::Device_Config_File_Walk_C2_Path)) {
    Write-Host "Gateway with configuration for Kat Walk C2/C2+ detected."
} else {
    [IBizLibrary.ComUtility]::KatDevice='walk_c2_core'
    if (Test-Path([IBizLibrary.ComUtility]::Device_Config_File_Walk_C2_Path)) {
        Write-Host "Gateway with configuration for Kat Walk C2 Core detected."
    } else {
        throw "Not found gateway configuration for neither Kat Walk C2/C2+ nor C2 Core."
    }
}
[IBizLibrary.Configs].GetMethod('C2ReceiverPairingInfoRead').invoke($null, $null)
if ($null -eq [IBizLibrary.KATSDKInterfaceHelper]::receiverPairingInfoSave.ReceiverSN) {
    throw "Gateway configuration for the treadmill is not found."
}

#
# Check the connected device.
#
[IBizLibrary.KATSDKInterfaceHelper].GetMethod('GetDeviceConnectionStatus').invoke($null, $null)

$platforms = [IBizLibrary.KATSDKInterfaceHelper]::walk_c2_Count + [IBizLibrary.KATSDKInterfaceHelper]::walk_c2_core_Count

if ($platforms -lt 1) {
    throw "Please connect the receiver USB dongle."
}

if ($platforms -gt 1) {
    throw "Please connect only the receiver USB dongle without the original treadmill cable."
}

if ([IBizLibrary.KATSDKInterfaceHelper]::walk_c2_Count -eq 1 -and [IBizLibrary.ComUtility]::KatDevice -ne 'walk_c2') {
    throw "The dongle flashed for Kat Walk C2/C2+, but gateway configuration is not."
}
if ([IBizLibrary.KATSDKInterfaceHelper]::walk_c2_core_Count -eq 1 -and [IBizLibrary.ComUtility]::KatDevice -ne 'walk_c2_core') {
    throw "The dongle flashed for Kat Walk C2 Core, but gateway configuration is not."
}

$dev = New-Object IBizLibrary.KATSDKInterfaceHelper+KATModels
for($devNo=0; $devNo -lt [IBizLibrary.KATSDKInterfaceHelper]::deviceCount; $devNo ++) {
    [IBizLibrary.KATSDKInterfaceHelper]::GetDevicesDesc([ref]$dev, $devNo)
    if ($dev.deviceType -eq 1) {
        break;
    }
}

if ($dev.device -ne "nRF KAT-VR Receiver") {
    throw "Please connect 'nRF KAT-VR Receiver' dongle, not the original treadmill ("+$dev.device+")"
}

#
#
#

function Start-Stream {
    [byte[]]$ans = New-Object byte[] 32
    [byte[]]$command = 0x20,0x1f,0x55,0xAA,0x00,0x00,0x30 + $ans
    [IBizLibrary.KATSDKInterfaceHelper]::SendHIDCommand($dev.serialNumber, $command, 32, $ans, 32)

    if ($ans[5] -eq 0) {
        [IBizLibrary.KATSDKInterfaceHelper]::SendHIDCommand($dev.serialNumber, $command, 32, $ans, 32)
    }
    if ($ans[5] -eq 0) {
        [IBizLibrary.KATSDKInterfaceHelper]::SendHIDCommand($dev.serialNumber, $command, 32, $ans, 32)
    }
    if ($ans[5] -eq 0) {
        Write-Host [System.BitConverter]::ToString($ans)
        throw "Can't start stream, sorry"
    }
}

function Stop-Stream {
    [byte[]]$ans = New-Object byte[] 32
    [byte[]]$command = 0x20,0x1f,0x55,0xAA,0x00,0x00,0x31 + $ans
    [IBizLibrary.KATSDKInterfaceHelper]::SendHIDCommand($dev.serialNumber, $command, 32, $ans, 32)
    [IBizLibrary.KATSDKInterfaceHelper]::SendHIDCommand($dev.serialNumber, $command, 32, $ans, 32)
}

function Run-Estimate([ref]$avgLeft, [ref]$avgRight) {
    $angleLeft = 0.0
    $angleLeftCnt = 0

    $angleRight = 0.0
    $angleRightCnt = 0

    [byte[]]$command = New-Object byte[] 64
    [byte[]]$ans = New-Object byte[] 32

    Write-Host "Please do a few steps..."
    while([IBizLibrary.KATSDKInterfaceHelper]::SendHIDCommand($dev.serialNumber, $command, 0, $ans, 32)) {
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
