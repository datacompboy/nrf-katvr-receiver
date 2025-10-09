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
function FindKatTreadmill ([ref]$refDevNo=[ref]0, $forceNew=$true) {
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

        if ($forceNew -and $dev.device -ne "nRF KAT-VR Receiver") {
            throw "Please connect 'nRF KAT-VR Receiver' dongle, not the original treadmill."
        }

        $refDevNo.Value = $devNo
    )
    return $dev
}

function KatSendHIDCommand([string]$serial, [byte[]]$cmd_and_args) {
    $VerbosePreference = "Continue"

    [byte[]]$ans = New-Object byte[] 32
    [byte[]]$command = 0x20,0x1f,0x55,0xAA,0x00,0x00 + $cmd_and_args + $ans
    $null = Invoke-NulRedirected { [IBizLibrary.KATSDKInterfaceHelper]::SendHIDCommand($serial, $command, 32, $ans, 32) }
    return $ans
}

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.IO;

public static class StdOutRedirector
{
    private static int _originalStdOutFd = -1;
    private const int STDOUT_FILENO = 1;
    [DllImport("msvcrt.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int _dup(int filedes);
    [DllImport("msvcrt.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int _dup2(int filedes1, int filedes2);
    [DllImport("msvcrt.dll", CallingConvention = CallingConvention.Cdecl)]
    private static extern int _close(int filedes);
    [DllImport("msvcrt.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
    private static extern int _open(string filename, int oflag, int pmode);
    // C-runtime open flags (simplified for 'write/create')
    private const int _O_WRONLY = 0x0002;
    private const int _O_CREAT = 0x0100;
    private const int _O_TRUNC = 0x0200;
    private const int _S_IREAD = 0x0100; // Read permission for file owner
    private const int _S_IWRITE = 0x0080; // Write permission for file owner
    private const int OPEN_FLAGS = _O_WRONLY | _O_CREAT | _O_TRUNC;
    private const int OPEN_PERMS = _S_IREAD | _S_IWRITE;
    // --- Redirection Methods ---
    public static bool StartStdOutRedirection(string lpFile)
    {
        if (_originalStdOutFd != -1)
        {
            return false;
        }
        _originalStdOutFd = _dup(STDOUT_FILENO);
        if (_originalStdOutFd == -1)
        {
            return false;
        }
        int newFileFd = _open(lpFile, OPEN_FLAGS, OPEN_PERMS);
        if (newFileFd == -1)
        {
            _close(_originalStdOutFd);
            _originalStdOutFd = -1;
            return false;
        }
        int result = _dup2(newFileFd, STDOUT_FILENO);
        _close(newFileFd);
        if (result == -1)
        {
             EndStdOutRedirection();
             return false;
        }
        return true;
    }

    public static bool EndStdOutRedirection()
    {
        if (_originalStdOutFd == -1)
        {
            return false;
        }
        int result = _dup2(_originalStdOutFd, STDOUT_FILENO);
        _close(_originalStdOutFd);
        _originalStdOutFd = -1;
        return result == 0; // _dup2 returns 0 on success
    }
}
'@

function Invoke-NulRedirected {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [scriptblock]$ScriptBlock
    )
    if (-not ('StdOutRedirector' -as [type])) {
        Write-Error "The StdOutRedirector type is not loaded. Run the Add-Type block first."
        return
    }
    if (-not [StdOutRedirector]::StartStdOutRedirection("NUL")) {
        Write-Error "Failed to start STDOUT redirection. Is another redirection active?"
        return
    }
    try {
        & $ScriptBlock
    }
    finally {
        if (-not [StdOutRedirector]::EndStdOutRedirection()) {
            Write-Warning "Failed to end STDOUT redirection. The console state may be corrupted."
        }
    }
}
