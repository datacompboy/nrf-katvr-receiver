# Script configuration
$katPath = "C:\Program Files (x86)\KAT Gateway\"

# Load Gateway's communication helper library

if (!(Test-Path $katPath -PathType Container)) {
    throw  "KAT Gateway expected to be installed in '${katPath}'"
}

Add-Type -Path "C:\Program Files (x86)\KAT Gateway\IBizLibrary.dll"

#
# Check the connected device.
#
[IBizLibrary.KATSDKInterfaceHelper].GetMethod('GetDeviceConnectionStatus').invoke($null, $null)

if ([IBizLibrary.KATSDKInterfaceHelper]::deviceCount -lt 1) {
    throw "Please connect the receiver USB dongle."
}

if ([IBizLibrary.KATSDKInterfaceHelper]::deviceCount -gt 1) {
    throw "Please connect only the receiver USB dongle without the original treadmill cable."
}

if ([IBizLibrary.KATSDKInterfaceHelper]::walk_c2_Count -ne 1) {
    throw "Only KatWalkC2/C2+ is supported, please flash correct firmware."
}

$dev = New-Object IBizLibrary.KATSDKInterfaceHelper+KATModels
[IBizLibrary.KATSDKInterfaceHelper]::GetDevicesDesc([ref]$dev, 0)

if ($dev.device -ne "nRF KAT-VR Receiver") {
    throw "Please connect 'nRF KAT-VR Receiver' dongle, not the original treadmill."
}

#
# Now we know that the right dongle is connected, let's duplicate orignial treadmill settings.
#

[IBizLibrary.ComUtility]::KatDevice='walk_c2'
[IBizLibrary.Configs].GetMethod('C2ReceiverPairingInfoRead').invoke($null, $null)

# Write the new SN
$newSn = [IBizLibrary.KATSDKInterfaceHelper]::receiverPairingInfoSave.ReceiverSN
[byte[]]$newSnArr = $newSn.ToCharArray()
[byte[]]$ans = New-Object byte[] 32
[byte[]]$command = 0x20,0x1f,0x55,0xAA,0x00,0x00,0x07,0x00 + $newSnArr + $ans
[IBizLibrary.KATSDKInterfaceHelper]::SendHIDCommand($dev.serialNumber, $command, 32, $ans, 32)

# Refresh device settings
[IBizLibrary.KATSDKInterfaceHelper]::GetDevicesDesc([ref]$dev, 0)
if ($dev.serialNumber -ne $newSn) {
    throw "Something went off. Please try again."
}

# Write the pairing info
$pairing=[IBizLibrary.KATSDKInterfaceHelper]::receiverPairingInfoSave.ReceiverPairingByte
[byte[]]$ans = New-Object byte[] 32
[byte[]]$command = 0x20,0x1f,0x55,0xAA,0x00,0x00,0x20,0x00 + $pairing + $ans
[IBizLibrary.KATSDKInterfaceHelper]::SendHIDCommand($dev.serialNumber, $command, 32, $ans, 32)

Write-Host "Congratulations! The device is ready to use."
