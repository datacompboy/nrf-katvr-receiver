param (
    [int16]$Frequency = 86
)

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

$FreqArr = [System.BitConverter]::GetBytes($Frequency)

# Request correction floats
[byte[]]$ans = New-Object byte[] 32
[byte[]]$command = 0x20,0x1f,0x55,0xAA,0x00,0x00,0xDD,0xF0 + $FreqArr + $ans
[IBizLibrary.KATSDKInterfaceHelper]::SendHIDCommand($dev.serialNumber, $command, 32, $ans, 32)

if ($ans[5] -ne 0xDD -and $ans[6] -ne 0xF0) {
    throw "No answer from device, please try again"
}

$Freq = [System.BitConverter]::ToInt16($ans, 7)

Write-Host "Receiver configuration updated to " $Freq " Hz"
