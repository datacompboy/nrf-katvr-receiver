# Script configuration
. .\z_kat-lib.ps1
$null = LoadKatFunctions "KAT_WalkC2_Dx.dll"
$null = LoadKatGatewayConfig

$dev = FindKatSensor
if (-not($dev.device -match "walk c2 foot" -or $dev.device -match "walk c2 direction")) {
    throw "Please connect foot or direction sensor."
}

$id = -1
$null = Invoke-NulRedirected { [IBizLibrary.KATSDKInterfaceHelper]::ReadDeviceId($dev.serialNumber, [ref]$id) }
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
Invoke-NulRedirected { [IBizLibrary.KATSDKInterfaceHelper]::GetSensorInformation([ref]$sensor, $dev.serialNumber) }
$dirmac = [IBizLibrary.KATSDKInterfaceHelper]::receiverPairingInfoSave.ReceiverPairingByte[1..6]
$leftmac = [IBizLibrary.KATSDKInterfaceHelper]::receiverPairingInfoSave.ReceiverPairingByte[7..12]
$rightmac = [IBizLibrary.KATSDKInterfaceHelper]::receiverPairingInfoSave.ReceiverPairingByte[13..19]

if(-not(Compare-Object $dirmac $sensor.mac)) {
    Invoke-NulRedirected { [IBizLibrary.KATSDKInterfaceHelper]::WriteDeviceId($dev.serialNumber, 1) }
    Write-Host "Made the sensor to be Direction"
}
elseif(-not(Compare-Object $leftmac $sensor.mac)) {
    Invoke-NulRedirected { [IBizLibrary.KATSDKInterfaceHelper]::WriteDeviceId($dev.serialNumber, 2) }
    Write-Host "Made the sensor to be Left Foot"
}
elseif(-not(Compare-Object $rightmac $sensor.mac)) {
    Invoke-NulRedirected { [IBizLibrary.KATSDKInterfaceHelper]::WriteDeviceId($dev.serialNumber, 3) }
    Write-Host "Made the sensor to be Right Foot"
}
else {
    throw "The sensor's mac is not paired to the treadmill"
}
