/*
 * KAT sensors BT packet handler.
 *
 * Sensors send updates as GATT NOTIFICATION (0x1B) for handle 0x002E of 20 bytes length.
 *
 * Sensors sends following packets:
 *   Bytes 0-1: 0000 => status (sent every 500 packets)
 *                  Status data (6 bytes of data, rest is garbage):
 *                      Byte 2 => sensor type (aka 3==right leg, 2==left leg, 1==direction, 0==unpaired)
 *                      Byte 3 => charge status, two bits (bit 0 == charging, bit 1 == fully charged)
 *                      Byte 4+5 => Short, charge level (in 1/256th, so 25600 == 100%)
 *                      Byte 6 => (Current foot sensor firmware version, 5 for feet, 4 for direction)
 *              non-zero => data   (499 other packets).
 *                  Foot sensor data (byte 0 equals 1, byte 1 equals 0):
 *                      Byte 2 & 3: two statuses (height from the ground & something else)
 *                        Byte (1-5) may be (1 0 1 0 1) [when something happens]
 *                      Byte 14-15: Speed X
 *                      Byte 16-17: Speed Y
 *                      Byte 18: always 0
 *                      Byte 19: brightness under the sensor (?)
 *                  Direction sensor data:
 *                      Bytes 0..13: 7 shorts, accelerometer data
 *                              First 4: direction, last 3 -- rotational angles, currently always zero
 *                      Byte 18: 0x80 if button press, 0 otherwise
 *
 * Example packets, status:
 *  KAT_LEFT  [23] 1b 2e 00 00 00 00 02 00 5f 05 00 29 00 00 00 10 00 00 00 00 00 00 1c -- unpaired
 *  KAT_LEFT  [23] 1b 2e 00 00 00 02 01 00 64 05 00 29 00 00 00 b0 3d 00 00 00 00 00 77 -- paired
 *  KAT_RIGHT [23] 1b 2e 00 00 00 03 02 00 64 05 00 29 00 00 00 10 00 00 00 00 00 00 82 -- paired
 *  KAT_DIR   [23] 1b 2e 00 00 00 01 00 56 5e 04 3e 00 00 00 00 00 00 00 00 00 00 00 00
 *                 ^^^^^^^^ GATT prefix, notification for 0x002E
 *                          ^^ Type = status
 *                             ^^ always zero
 *                                ^^ Sensor type (2 left, 3 right, 1 dir, 0 unpaired)
 *                                   ^^ charge status bits
 *                                      ^^^^^ charge level
 *                                            ^^ firmware version
 *
 *  KAT_LEFT  [23] 1b 2e 00 01 00 00 8d 18 00 00 00 29 00 00 00 b0 3d 00 00 00 00 00 78 -- ground
 *  KAT_LEFT  [23] 1b 2e 00 01 08 4e 20 38 00 00 00 00 00 00 00 d0 00 00 00 00 00 00 1a -- air
 *  KAT_RIGHT [23] 1b 2e 00 01 00 00 92 18 00 00 00 29 00 00 00 10 00 00 00 00 00 00 83 -- ground
 *                 ^^^^^^^^ GATT prefix, notification for 0x002E
 *                          ^^ Packet type (1 == data update)
 *                                ^^^^^ some status bytes
 *  KAT_LEFT  [23] 1b 2e 00 01 00 00 73 9b 4f 05 00 84 01 00 00 00 00 51 fe 1f ff 00 7b
 *                                                                    ^^^^^ speed x
 *                                                                          ^^^^^ speed y
 *                                                                                   ^^ color under
 *
 *  KAT_DIR   [23] 1b 2e 00 7d f3 9f 00 8f 05 84 3e 00 00 00 00 00 00 00 00 00 00 00 00
 *  KAT_DIR   [23] 1b 2e 00 1f e4 59 00 e9 0a 91 38 00 00 00 00 00 00 00 00 00 00 80 00
 *                 ^^^^^^^^ GATT prefix, notification for 0x002E                  ^^ is button pressed
 *                          ^^^^^ Short 1     ^^^^^ short 4     ^^^^^ short 7
 *                                ^^^^^ Short 2     ^^^^^ short 5
 *                                      ^^^^^ Short 3     ^^^^^ short 6
 */

#include <zephyr/bluetooth/buf.h>

#include "kat_main.h"
#include "kat_ble.h"

#define GATT_NOTIFICATION 0x1B
#define KAT_CHAR_HANDLE 0x2E
#define KAT_BT_STATUS 0

typedef __PACKED_STRUCT
{
    __PACKED_UNION
    {
        short packetType; // 0 == status, non-zero == data
        __PACKED_STRUCT
        {
            short __zero;
            eKatDevice deviceType;
            char status;
            short chargeLevel; // network order (high, low)
            char firmwareVersion;
        }
        status;
        __PACKED_STRUCT
        {
            short __undef0;
            char status1;
            char status2;
            char __undef1[10];
            short speed_x; // network order (high, low)
            short speed_y; // network order (high, low)
            char __zero;
            char color;
        }
        footData;
        __PACKED_STRUCT
        {
            short axis[7];
            char __undef0[4];
            char button;
            char __undef1;
        }
        dirData;
    };
}
tKatBtData;

BUILD_ASSERT(sizeof(tKatBtData) == 20, "tKatBtData definition doesn't match expectation");
BUILD_ASSERT(offsetof(tKatBtData, footData.color) == 19, "footData structure doesn't match expectation");
BUILD_ASSERT(offsetof(tKatBtData, dirData.button) == 18, "dirData structure doesn't match expectation");

typedef __PACKED_STRUCT
{
    char gatt_comand;
    char char_id;
    char __zero;
    tKatBtData data;
}
tGattKatPacket;

BUILD_ASSERT(sizeof(tGattKatPacket) == 23, "tGattKatPacket definition doesn't match expectation");

void kat_ble_handle_packet(struct net_buf *req_buf, sKatDeviceInfo *katDevice)
{
    // Ignore not initialized sensors
    if (katDevice->deviceType == KAT_NONE)
        return;
    // Process only valid packets
    if (req_buf->len != sizeof(tGattKatPacket))
        return;
    tGattKatPacket *pack = (tGattKatPacket *)req_buf->data;
    if (pack->gatt_comand != GATT_NOTIFICATION)
        return;
    if (pack->char_id != KAT_CHAR_HANDLE || pack->__zero != 0)
        return;

    const tKatBtData *data = &pack->data;
    if (data->packetType == KAT_BT_STATUS)
    {
        if (!IS_ENABLED(CONFIG_APP_ACCEPT_UNPAIRED_SENSORS) && katDevice->deviceType != data->status.deviceType)
        {
            /// Sensor not paired, mark it broken
            printk("Sensor is not paired! Ignoring it");
            katDevice->deviceType = KAT_NONE;
            return;
        }
        katDevice->deviceStatus.chargeLevel = data->status.chargeLevel;
        katDevice->deviceStatus.chargeStatus = data->status.status;
        katDevice->deviceStatus.firmwareVersion = data->status.firmwareVersion;
        katDevice->deviceStatus.freshStatus = true;
    }
    else
    {
        switch (katDevice->deviceType)
        {
        case KAT_LEFT:
            [[fallthrough]];
        case KAT_RIGHT:
            /// refresh data
            katDevice->footData.color = data->footData.color;
            katDevice->footData.speed_x = data->footData.speed_x;
            katDevice->footData.speed_y = data->footData.speed_y;
            katDevice->footData.status1 = data->footData.status1;
            katDevice->footData.status2 = data->footData.status2;
            break;
        case KAT_DIR:
            BUILD_ASSERT(sizeof(katDevice->dirData.axis) == 14, "We copy whole sensor data batch (7 shorts)");
            BUILD_ASSERT(sizeof(katDevice->dirData.axis) == sizeof(data->dirData.axis), "We should copy matching arrays");
            memcpy(&katDevice->dirData.axis, &data->dirData.axis, sizeof(data->dirData.axis));
            katDevice->dirData.button = data->dirData.button;
            break;
        case _KAT_MAX_DEVICE:
            [[fallthrough]]; // Should not happen
        case KAT_NONE:       // Should not happen
            return;
        }
        // TODO: add timestamp
        katDevice->deviceStatus.freshData = true;
    }
}
