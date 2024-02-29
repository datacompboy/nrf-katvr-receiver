#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#include "kat_main.h"
#include "kat_ble.h"
#include "kat_usb.h"

/*
 * USB protocol
 */

typedef __PACKED_UNION
{
    tKatUsbBuf raw;
    __PACKED_STRUCT
    {
        uint8_t len;   // should be 31 / 0x1F
        uint8_t c55;   // should be 0x55
        uint8_t cAA;   // should be 0xAA
        uint16_t zero; // should be zero
        uint8_t cmd;
        __PACKED_UNION
        {
            uint8_t args[26];
            __PACKED_STRUCT
            {
                uint8_t zero;
                uint8_t ans;
            }
            ansbyte;
            __PACKED_STRUCT
            {
                uint8_t zero;
                char ans[25];
            }
            ansstring;
            __PACKED_STRUCT
            {
                uint8_t zero;
                uint8_t count;
                uint8_t addrs[4][6]; // maximum 4 child MACs fits
            }
            ansaddrs;
            __PACKED_STRUCT
            {
                uint8_t sensor_id;
                uint8_t sensor_type;
                uint8_t chargeStatus;
                uint16_t charge;
                uint8_t firmware;
                // 20 unused bytes
            }
            anscontrol;
            __PACKED_STRUCT
            {
                uint8_t sensor_id;
                uint16_t axis[7];
                uint8_t __undef0[4];
                uint8_t button;
                // 6 unused bytes
            }
            ansdirection;
            __PACKED_STRUCT
            {
                uint8_t sensor_id;
                uint16_t __undef0;
                uint8_t status1;
                uint8_t status2;
                uint8_t __undef1[10];
                uint16_t speed_x;
                uint16_t speed_y;
                uint8_t __zero;
                uint8_t color;
                // 5 unused bytes
            }
            ansfoot;
        };
    }
    data;
}
tKatUsbPacket;

BUILD_ASSERT(sizeof(tKatUsbPacket) == sizeof(tKatUsbBuf), "Packet definitions doesn't match");
BUILD_ASSERT(offsetof(tKatUsbPacket, data.anscontrol.sensor_id) == offsetof(tKatUsbPacket, data.ansdirection.sensor_id), "Alternate packets fields should match");
BUILD_ASSERT(offsetof(tKatUsbPacket, data.anscontrol.sensor_id) == offsetof(tKatUsbPacket, data.ansfoot.sensor_id), "Alternate packets fields should match");
BUILD_ASSERT(offsetof(tKatUsbPacket, data.anscontrol.sensor_id) == 6, "Struct packing seems broken");

enum tKatUsbCommand
{
    cReadDeviceId = 0x03,
    cWriteDeviceId = 0x04,
    cGetFirmwareVersion = 0x05,
    cGetSN = 0x06,
    cSetSN = 0x07,
    cGetSensorInformation = 0x08,

    cWritePairing = 0x20,
    cReadPairing = 0x21,
    cDeepSleep = 0x23,

    cStartRead = 0x30,
    cStopRead = 0x31,

    cUpdateData = cStartRead, // Data matches Start Read command
    cUpdateConfig = 0x32,
    // 0x33 is unknown

    cCloseVibration = 0xA0,
    cSetOutput = 0xA1, // LED & Vibration
};

// We use single buffer for commands & answers and separate buffer for notifications
#define KAT_USB_BUF_CONF 0
#define KAT_USB_BUF_DATA 1
#define KAT_USB_BUFS 2
static tKatUsbBuf usb_update_buf[_KAT_MAX_DEVICE - 1][KAT_USB_BUFS];

//
// Returns true if buf now contains the answer to send.
//
bool kat_usb_handle_request(uint8_t *buf, int size)
{
#if CONFIG_APP_PACKET_LOG
    printk("USB packet [%2d]:", size);
    for (int i = 0; i < size; ++i)
    {
        printk(" %02x", buf[i]);
    }
    printk("\n");
#endif

    // KAT uses USBHID<=>UART chip, all packages set to be aligned 32 bytes packets.
    // Byte 0 is length of the rest (31)
    // Bytes 1-4 are 0x55 0xAA 0x00 0x00
    if (size != KAT_USB_PACKET_LEN)
        return false;
    tKatUsbPacket *pack = (tKatUsbPacket *)buf;

    if (pack->data.len != KAT_USB_PACKET_LEN - 1 || pack->data.c55 != 0x55 || pack->data.cAA != 0xAA || pack->data.zero != 0)
        return false;

    switch (pack->data.cmd)
    {
    case cReadDeviceId:
        pack->data.ansbyte.zero = 0;
        pack->data.ansbyte.ans = 0;
        return true;

    case cWriteDeviceId: // Do nothing, receiver is not configurable
        return false;

    case cGetFirmwareVersion:
        pack->data.ansbyte.zero = 0;
        pack->data.ansbyte.ans = 3; // current kat fw version
        return true;

    case cGetSN:
        pack->data.ansstring.zero = 0;
        strncpy(pack->data.ansstring.ans, CONFIG_USB_DEVICE_SN, KAT_USB_PACKET_LEN - 8);
        pack->data.ansstring.ans[sizeof(pack->data.ansstring.ans) - 1] = 0; // ensure null termination
        return true;

    case cSetSN:
        strncpy(katUsbSerial, pack->data.ansstring.ans, sizeof(CONFIG_USB_DEVICE_SN));
        katUsbSerial[sizeof(CONFIG_USB_DEVICE_SN)-1] = 0;
        kat_usb_update_serial();
        kat_settings_async_save();
        return false;

    case cGetSensorInformation:
        // Return local MAC address
        pack->data.ansstring.zero = 0;
        kat_ble_get_localaddr(pack->data.ansstring.ans, sizeof(pack->data.ansstring.ans));
        return true;

    case cReadPairing:
        pack->data.ansaddrs.zero = 0;
        const int rcnt = MIN(NumKatBleDevices, ARRAY_SIZE(pack->data.ansaddrs.addrs));
        pack->data.ansaddrs.count = rcnt;
        for (int i = 0; i < rcnt; ++i)
        {
            BUILD_ASSERT(sizeof(pack->data.ansaddrs.addrs[i]) == sizeof(KatBleDevices[i].a), "Size of BT address should match");
            memcpy(&pack->data.ansaddrs.addrs[i], &KatBleDevices[i].a, sizeof(KatBleDevices[i].a));
        }
        return true;

    case cWritePairing:
        const int wcnt = MIN(ARRAY_SIZE(KatBleDevices), pack->data.ansaddrs.count);
        NumKatBleDevices = wcnt;
        for (int i = 0; i < wcnt; ++i)
        {
            BUILD_ASSERT(sizeof(pack->data.ansaddrs.addrs[i]) == sizeof(KatBleDevices[i].a), "Size of BT address should match");
            memcpy(&KatBleDevices[i].a, &pack->data.ansaddrs.addrs[i], sizeof(KatBleDevices[i].a));
        }
        kat_ble_update_devices();
        kat_settings_async_save();
        return false;

    case cDeepSleep:
        return false;

    case cStartRead:
        // Speed-up connection, by forcing last known status packets first
        for (int i = 1; i < _KAT_MAX_DEVICE; ++i)
        {
            if (KatDeviceInfo[i].deviceStatus.firmwareVersion > 0)
                KatDeviceInfo[i].deviceStatus.freshStatus = true;
        }
        kat_ble_enable_connections();
        kat_usb_enable_stream();
        return false; // Don't force answer here, we'll send packets from different buffers.

    case cStopRead:
        kat_usb_disable_stream();
        kat_ble_disable_connections();
        return false;

    case cCloseVibration:
        [[fallthrough]];
    case cSetOutput:
        // Noop, we don't have control for LED of vibration here.
        return false;

    default:
        printk("Unknown USB packet [%2d]:", size);
        for (int i = 0; i < size; ++i)
        {
            printk(" %02x", buf[i]);
        }
        printk("\n");
        return false;
    }
}

void kat_usb_init_buffers(void)
{
    for (int i = 0; i < ARRAY_SIZE(usb_update_buf); ++i)
    {
        for (int j = 0; j < KAT_USB_BUFS; ++j)
        {
            tKatUsbPacket *b = (tKatUsbPacket *)usb_update_buf[i][j];
            b->data.len = 0x1F;
            b->data.c55 = 0x55;
            b->data.cAA = 0xAA;
            b->data.zero = 0;
            b->data.anscontrol.sensor_id = i;
            if (j == KAT_USB_BUF_CONF)
            {
                b->data.cmd = cUpdateConfig;
                b->data.anscontrol.sensor_type = i + 1; // we rely on id-to-type mapping for communication
            }
            else
            {
                b->data.cmd = cUpdateData;
            }
        }
    }
}

static uint8_t *kat_usb_encode_status(eKatDevice devId, sKatDeviceInfo *katDevice)
{
    int dev = (int)devId - 1;
    tKatUsbPacket *b = (tKatUsbPacket *)usb_update_buf[dev][KAT_USB_BUF_CONF];
    b->data.anscontrol.charge = katDevice->deviceStatus.chargeLevel;
    b->data.anscontrol.chargeStatus = katDevice->deviceStatus.chargeStatus;
    b->data.anscontrol.firmware = katDevice->deviceStatus.firmwareVersion;
    return b->raw;
}

static uint8_t *kat_usb_encode_data(eKatDevice devId, sKatDeviceInfo *katDevice)
{
    int dev = (int)devId - 1;
    tKatUsbPacket *b = (tKatUsbPacket *)usb_update_buf[dev][KAT_USB_BUF_DATA];
    switch (katDevice->deviceType)
    {
    case KAT_DIR:
        BUILD_ASSERT(sizeof(katDevice->dirData.axis) == sizeof(b->data.ansdirection.axis), "USB and BT structs should match");
        memcpy(b->data.ansdirection.axis, katDevice->dirData.axis, sizeof(katDevice->dirData.axis));
        b->data.ansdirection.button = katDevice->dirData.button;
        return b->raw;
    case KAT_LEFT:
        [[fallthrough]];
    case KAT_RIGHT:
        b->data.ansfoot.status1 = katDevice->footData.status1;
        b->data.ansfoot.status2 = katDevice->footData.status2;
        b->data.ansfoot.speed_x = katDevice->footData.speed_x;
        b->data.ansfoot.speed_y = katDevice->footData.speed_y;
        b->data.ansfoot.color = katDevice->footData.color;
        return b->raw;
    case _KAT_MAX_DEVICE:
        [[fallthrough]];
    case KAT_NONE:
        return NULL;
    }
    return NULL;
}

uint8_t* kat_usb_get_update_packet(void)
{
    // Try send status first
    for (int i = 0; i < NumKatBleDevices; ++i)
    {
        eKatDevice devId = (eKatDevice)i + 1;
        if (KatDeviceInfo[devId].deviceStatus.freshStatus)
        {
            uint8_t *buf = kat_usb_encode_status(devId, &KatDeviceInfo[devId]);
            if (buf)
            {
                KatDeviceInfo[devId].deviceStatus.freshStatus = false;
                return buf;
            }
        }
    }
    // If there is no fresh status updates -- send data update
    for (int i = 0; i < NumKatBleDevices; ++i)
    {
        eKatDevice devId = (eKatDevice)i + 1;
        if (KatDeviceInfo[devId].deviceStatus.freshData)
        {
            uint8_t *buf = kat_usb_encode_data(devId, &KatDeviceInfo[devId]);
            if (buf)
            {
                KatDeviceInfo[devId].deviceStatus.freshData = false;
                return buf;
            }
        }
    }
    return NULL;
}
