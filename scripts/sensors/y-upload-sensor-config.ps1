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

[IBizLibrary.KATSDKInterfaceHelper].GetMethod('GetDeviceConnectionStatus').invoke($null, $null)
$dev = New-Object IBizLibrary.KATSDKInterfaceHelper+KATModels
for($devNo=0; $devNo -lt [IBizLibrary.KATSDKInterfaceHelper]::deviceCount; $devNo ++) {
    [IBizLibrary.KATSDKInterfaceHelper]::GetDevicesDesc([ref]$dev, $devNo)
    if ($dev.deviceType -eq 0 -and $dev.device -match "walk c2 foot") {
        break;
    }
    if ($dev.deviceType -eq 0 -and $dev.device -match "walk c2 direction") {
        break;
    }
}

if (-not($dev.device -match "walk c2 foot" -or $dev.device -match "walk c2 direction")) {
    throw "Please connect foot or direction sensor."
}

$id = -1
[IBizLibrary.KATSDKInterfaceHelper]::ReadDeviceId($dev.serialNumber, [ref]$id)
if ($id -eq 1) {
    Write-Warning "Sensor is already configured to be direction sensor"
}
elseif ($id -eq 2) {
    Write-Warning "Sensor is already configured to be left sensor"
}
elseif ($id -eq 3) {
    Write-Warning "Sensor is already configured to be right sensor"
}
elseif ($id -ne 0) {
    throw "Sensor configuration error!"
}

$sensor = New-Object IBizLibrary.KATSDKInterfaceHelper+sensorInformation
[IBizLibrary.KATSDKInterfaceHelper]::GetSensorInformation([ref]$sensor, $dev.serialNumber)
$dirmac = [IBizLibrary.KATSDKInterfaceHelper]::receiverPairingInfoSave.ReceiverPairingByte[1..6]
$leftmac = [IBizLibrary.KATSDKInterfaceHelper]::receiverPairingInfoSave.ReceiverPairingByte[7..12]
$rightmac = [IBizLibrary.KATSDKInterfaceHelper]::receiverPairingInfoSave.ReceiverPairingByte[13..19]

if(-not(Compare-Object $dirmac $sensor.mac)) {
    [IBizLibrary.KATSDKInterfaceHelper]::WriteDeviceId($dev.serialNumber, 1)
    Write-Host "Made the sensor to be Direction"
}
elseif(-not(Compare-Object $leftmac $sensor.mac)) {
    [IBizLibrary.KATSDKInterfaceHelper]::WriteDeviceId($dev.serialNumber, 2)
    Write-Host "Made the sensor to be Left Foot"
}
elseif(-not(Compare-Object $rightmac $sensor.mac)) {
    [IBizLibrary.KATSDKInterfaceHelper]::WriteDeviceId($dev.serialNumber, 3)
    Write-Host "Made the sensor to be Right Foot"
}
else {
    throw "The sensor's mac is not paired to the treadmill"
}
