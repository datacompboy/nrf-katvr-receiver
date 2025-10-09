# Common KAT functions helpers library

# Detect the Gateway location and load IBizLibrary
function LoadKatFunctions {
    if (-not('IBizLibrary.KATSDKInterfaceHelper' -as [type])) {
        $gateway = (Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\KAT Gateway.exe").'(default)'
        if ($gateway -ne '') {
            $katPath = Split-Path $gateway
        } else {
            $katPath = "C:\Program Files (x86)\KAT Gateway"
        }

        # Load Gateway's communication helper library
        if (!(Test-Path $katPath -PathType Container)) {
            throw  "KAT Gateway expected to be installed in '${katPath}'"
        }

        $kernel32 = Add-Type -Name 'Kernel32' -Namespace 'Win32' -PassThru -MemberDefinition '
            [DllImport("kernel32.dll", SetLastError = true, CharSet=CharSet.Auto)]
            public static extern bool SetDllDirectory(string lpPathName);'

        $kernel32::SetDllDirectory($katPath)
        Add-Type -Path "${katPath}\IBizLibrary.dll"
    }
}

#
# Detect known Receiver version
#
function LoadKatGatewayConfig {
    LoadKatFunctions
    [IBizLibrary.ComUtility]::KatDevice='walk_c2'
    if (Test-Path([IBizLibrary.ComUtility]::Device_Config_File_Walk_C2_Path)) {
        Write-Host "Gateway with configuration for Kat Walk C2/C2+ detected."
    } else {
        [IBizLibrary.ComUtility]::KatDevice='walk_c2_core'
        if (Test-Path([IBizLibrary.ComUtility]::Device_Config_File_Walk_C2_Path)) {
            Write-Host "Gateway with configuration for Kat Walk C2 Core detected."
        } else {
            [IBizLibrary.ComUtility]::KatDevice='walk_c2_plus_enhanced'
            if (Test-Path([IBizLibrary.ComUtility]::Device_Config_File_Walk_C2_Path)) {
                Write-Host "Gateway with configuration for Kat Walk C2+Enhanced detected."
            } else {
                throw "Not found gateway configuration for neither Kat Walk C2/C2+ nor C2 Core nor C2+ Enhanced."
            }
        }
    }
    [IBizLibrary.Configs].GetMethod('C2ReceiverPairingInfoRead').invoke($null, $null)
    if ($null -eq [IBizLibrary.KATSDKInterfaceHelper]::receiverPairingInfoSave.ReceiverSN) {
        throw "Gateway configuration for the treadmill is not found."
    }
}

#
# Check the connected device.
#
function FindKatTreadmill ([ref]$refDevNo=[ref]0) {
    $null = @(
        LoadKatFunctions
        [IBizLibrary.KATSDKInterfaceHelper].GetMethod('GetDeviceConnectionStatus').invoke($null, $null)
        Start-Sleep -Seconds 3
        [IBizLibrary.KATSDKInterfaceHelper].GetMethod('GetDeviceConnectionStatus').invoke($null, $null)

        $platforms = [IBizLibrary.KATSDKInterfaceHelper]::walk_c2_Count + [IBizLibrary.KATSDKInterfaceHelper]::walk_c2_core_Count + [IBizLibrary.KATSDKInterfaceHelper]::walk_c2_plus_enhanced_Count; 

        if ($platforms -lt 1) {
            throw "Please connect the receiver USB dongle."
        }

        if ($platforms -gt 1) {
            throw "Please connect only the receiver USB dongle without the original treadmill cable."
        }

        $dev = New-Object IBizLibrary.KATSDKInterfaceHelper+KATDeviceDesc
        for($devNo=0; $devNo -lt [IBizLibrary.KATSDKInterfaceHelper]::deviceCount; $devNo ++) {
            [IBizLibrary.KATSDKInterfaceHelper]::GetDevicesDesc([ref]$dev, $devNo)
            if ($dev.deviceType -eq 1) {
                break;
            }
        }

        if ($dev.device -ne "nRF KAT-VR Receiver") {
            throw "Please connect 'nRF KAT-VR Receiver' dongle, not the original treadmill."
        }

        $refDevNo.Value = $devNo
    )
    return $dev
}

function KatSendHIDCommand([string]$serial, [byte[]]$cmd_and_args) {
    $null = @(
        [byte[]]$ans = New-Object byte[] 32
        [byte[]]$command = 0x20,0x1f,0x55,0xAA,0x00,0x00 + $cmd_and_args + $ans
        [IBizLibrary.KATSDKInterfaceHelper]::SendHIDCommand($serial, $command, 32, $ans, 32)
    )
    return $ans
}
