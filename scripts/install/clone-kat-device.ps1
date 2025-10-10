# Script configuration
. .\z_kat-lib.ps1
$null = LoadKatFunctions

if (Get-Process -name "KAT Gateway" -ErrorAction SilentlyContinue) {
    throw "Please stop KAT Gateway."
}

LoadKatGatewayConfig

#
# Check is the ready to flash dongle connected
#
if (Get-Volume -FileSystemLabel "XIAO-SENSE" -ErrorAction SilentlyContinue)
{
    $dest = (Get-Volume -FileSystemLabel "XIAO-SENSE").DriveLetter + ":\"
    $firmware = 'nrf_receiver_'+([IBizLibrary.ComUtility]::KatDevice)+'.uf2'
    if (Test-Path($firmware)) {
        Write-Host "Uploading firmware $firmware..."
        Copy-Item $firmware $dest
        Start-Sleep -Seconds 5
    } else {
        throw "Please flash dongle with receiver firmware."
    }
}

$devNo = -1
$dev = FindKatTreadmill([ref]$devNo)

if ([IBizLibrary.KATSDKInterfaceHelper]::walk_c2_Count -eq 1 -and [IBizLibrary.ComUtility]::KatDevice -ne 'walk_c2') {
    throw "The dongle flashed for Kat Walk C2/C2+, but gateway configuration is not."
}
if ([IBizLibrary.KATSDKInterfaceHelper]::walk_c2_core_Count -eq 1 -and [IBizLibrary.ComUtility]::KatDevice -ne 'walk_c2_core') {
    throw "The dongle flashed for Kat Walk C2 Core, but gateway configuration is not."
}
if ([IBizLibrary.KATSDKInterfaceHelper]::walk_c2_plus_enhanced_Count -eq 1 -and [IBizLibrary.ComUtility]::KatDevice -ne 'walk_c2_plus_enhanced') {
    throw "The dongle flashed for Kat Walk C2+ Enhanced, but gateway configuration is not."
}

if ($dev.device -ne "nRF KAT-VR Receiver") {
    throw "Please connect 'nRF KAT-VR Receiver' dongle, not the original treadmill."
}

#
# Now we know that the right dongle is connected, let's duplicate orignial treadmill settings.
#

# Write the new SN
$newSn = [IBizLibrary.KATSDKInterfaceHelper]::receiverPairingInfoSave.ReceiverSN
$null = KatSendHIDCommand $dev.serialNumber @(0x07,0x00 + $newSn.ToCharArray()) 

# Refresh device settings
[IBizLibrary.KATSDKInterfaceHelper]::GetDevicesDesc([ref]$dev, $devNo)
if ($dev.serialNumber -ne $newSn) {
    throw "Something went off. Please try again."
}

# Write the pairing info
$null = KatSendHIDCommand $dev.serialNumber @(,$20 + [IBizLibrary.KATSDKInterfaceHelper]::receiverPairingInfoSave.ReceiverPairingByte)

Write-Host "Congratulations! The device is ready to use."
