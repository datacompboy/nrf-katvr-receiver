param (
    [string]$patch = "",
    [string]$orig = "zzz",
    [int]$dvid = 0xC4F4,
    [int]$dpid = -1,
    [int]$type = 3,
    [int]$index = 0
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

Add-Type -Path "C:\Program Files (x86)\KAT Gateway\KAT_WalkC2_Dx.dll"

$firmware = $katPath + "katvr_" + $orig + ".hex"

if (!(Test-Path $katPath)) {
    throw "Original sensor firmware is not found at '$firmware'"
}

if ($patch -ne "") {
    $newfw = $ENV:TEMP + "\katvr_" + $orig + "_patch.hex"
    $patchscript = ".\z_patch_" + $patch + ".ps1"
    if (Get-Content $firmware | & $patchscript | Out-File -FilePath $newfw -Encoding ascii) {
        throw "Failed to patch the firmware file."
    }
    $firmware = $newfw
    Write-Host "Flashing the PATCH for " + $orig
} else {
    Write-Host "Restoring the ORIGINAL firmware for " + $orig
}

Write-Host "Using file $firmware"

if ([KAT_WalkC2_Dx.KatvrFirmwareHelper]::ch9326_find() -eq 0) {
    throw "ch9326_find failed"
}
if ([KAT_WalkC2_Dx.KatvrFirmwareHelper]::ch9326_open($dvid, $dpid) -eq 0) {
    throw "ch9326_open failed"
}
if ([KAT_WalkC2_Dx.KatvrFirmwareHelper]::ch9326_set_gpio($index, 15, 15) -eq 0) {
    throw "ch9326_set_gpio failed"
}
if ([KAT_WalkC2_Dx.KatvrFirmwareHelper]::ch9326_connected($index) -eq 0) {
    throw "ch9326_connected failed"
}
if ([KAT_WalkC2_Dx.KatvrFirmwareHelper]::flash($firmware, $type, 0) -ne 1) {
    throw "KatvrFirmwareHelper.flash failed"
}
[KAT_WalkC2_Dx.KatvrFirmwareHelper]::ch9326_ClearThreadData()
[KAT_WalkC2_Dx.KatvrFirmwareHelper]::close_ch9326()
Start-Sleep 1
.\y-upload-sensor-config.ps1
