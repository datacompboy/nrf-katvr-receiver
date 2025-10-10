# Script configuration
param (
	[Parameter(Mandatory)]
    [string]$Serial = ""
)

. .\z_kat-lib.ps1
$null = LoadKatFunctions

$devNo = -1
$dev = FindKatTreadmill([ref]$devNo)

#
# Now we know that the right dongle is connected, let's duplicate orignial treadmill settings.
#

# Write the new SN
$newSn = $Serial
$null = KatSendHIDCommand $dev.serialNumber @(0x07,0x00 + $newSn.ToCharArray()) 

# Refresh device settings
Start-Sleep -Seconds 1
[IBizLibrary.KATSDKInterfaceHelper]::GetDevicesDesc([ref]$dev, $devNo)
if ($dev.serialNumber -ne $newSn) {
    throw "Something went off. Please try again."
}

Write-Host "Serial number now is '$Serial'"
