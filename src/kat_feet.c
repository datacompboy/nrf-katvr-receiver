/*
 * KAT Feet sensors handler.
 *
 * Sensor sends updates as GATT NOTIFICATION (0x1B) for handle 0x002E of 20 bytes length.
 *
 * Sensor sends following packets:
 *      Byte 0: 00 => status (sent every 500 packets)
 *                  Status data (6 bytes of data, rest is garbage):
 *                      Byte 1 => 00 always
 *                      Byte 2 => sensor type (aka 2==left leg, 3==right leg, 1==direction, 0==unpaired)
 *                      Byte 3 => some status, join of two flags:
 *                 updateBuf[3] = (DAT_400e00f6 == 0) | (DAT_400e00fa == 0) << 1;
 *                      Byte 4+5 => (Hi)(Lo), 2-byte integer DAT_20001fec or DAT_20001148
 *                              Charge level? scaled to 20..100
 *                      Byte 6 => 5 (Current foot sensor firmware version)
 *              01 => data   (499 other packets)
 *                  Packet data:
 *                      Byte 2 & 3: two statuses (height from the ground & something else)
 *                        Byte (1-5) may be (1 0 1 0 1) [when something happens]
 *                      Byte 14-15: Speed X
 *                      Byte 16-17: Speed Y
 *                      Byte 18: always 0
 *                      Byte 19: color under the sensor (?)
 * 
 * Example packets, status:
 *  KAT_LEFT  [23] 1b 2e 00 00 00 00 02 00 5f 05 00 29 00 00 00 10 00 00 00 00 00 00 1c -- unpaired
 *  KAT_LEFT  [23] 1b 2e 00 00 00 02 01 00 64 05 00 29 00 00 00 b0 3d 00 00 00 00 00 77 -- paired
 *  KAT_RIGHT [23] 1b 2e 00 00 00 03 02 00 64 05 00 29 00 00 00 10 00 00 00 00 00 00 82 -- paired
 *                 ^^^^^^^^ GATT prefix, notification for 0x002E
 *                          ^^ Type = status
 *                             ^^ always zero
 *                                ^^ Sensor type (2 left, 3 right)
 *                                   ^^ two status bits
 *                                      ^^^^^ charge level
 *                                            ^^ firmware version
 * 
 *  KAT_LEFT  [23] 1b 2e 00 01 00 00 8d 18 00 00 00 29 00 00 00 b0 3d 00 00 00 00 00 78 -- ground
 *  KAT_LEFT  [23] 1b 2e 00 01 08 4e 20 38 00 00 00 00 00 00 00 d0 00 00 00 00 00 00 1a -- air
 *  KAT_RIGHT [23] 1b 2e 00 01 00 00 92 18 00 00 00 29 00 00 00 10 00 00 00 00 00 00 83 -- ground
 *                 ^^^^^^^^ GATT prefix, notification for 0x002E
 *                          ^^ Packet type (1 == data update)
 *                                ^^^^^ some status bytes
 *                                                                    ^^^^^ speed x
 *                                                                          ^^^^^ speed y
 *                                                                                   ^^ color under
*/

#include <zephyr/bluetooth/buf.h>

#include "kat.h"

typedef enum { KAT_BT_STATUS=0, KAT_BT_DATA=1 } tKatFootStatus;
BUILD_ASSERT(sizeof(tKatFootStatus)==1, "tKatFootStatus should be 1 byte");

typedef __PACKED_STRUCT {
    tKatFootStatus packetType;
    __PACKED_UNION {
        __PACKED_STRUCT {
            char __zero;
            tKatDevice deviceType;
            char status;
            short chargeLevel; // network order (high, low)
            char firmwareVersion;
        } status;
        __PACKED_STRUCT {
            char __unk;
            char status1;
            char status2;
            char __unk2[10];
            short speed_x; // network order (high, low)
            short speed_y; // network order (high, low)
            char __zero;
            char color;
        } data;
    };
} tKatFootBtData;

BUILD_ASSERT(sizeof(tKatFootBtData) == 20, "tKatFootBtData definition doesn't match expectation");

typedef __PACKED_STRUCT {
    char gatt_comand;
    char char_id;
    char __zero;
    tKatFootBtData foot;
} tGattKatPacket;

BUILD_ASSERT(sizeof(tGattKatPacket) == 23, "tGattKatPacket definition doesn't match expectation");


#define GATT_NOTIFICATION 0x1B
#define KAT_CHAR_HANDLE 0x2E
void parseFeet(struct net_buf *req_buf, tKatFootDevice* footDevice)
{
    // Ignore not initialized sensors
    if (footDevice->deviceType == KAT_NONE)
        return;
    // Process only valid packets
    if (req_buf->len != sizeof(tGattKatPacket))
        return;
    tGattKatPacket * pack = (tGattKatPacket *) req_buf->data;
    if (pack->gatt_comand != GATT_NOTIFICATION)
        return;
    if (pack->char_id != KAT_CHAR_HANDLE || pack->__zero != 0)
        return;
    const tKatFootBtData * foot = &pack->foot;

    if (foot->packetType == KAT_BT_STATUS)
    {
        /// copy
        if (footDevice->deviceType != foot->status.deviceType) {
            /// Sensor not paired, mark it broken
            printk("Sensor is not paired! Ignoring it");
            footDevice->deviceType = KAT_NONE;
            return;
        }
        footDevice->chargeLevel = foot->status.chargeLevel;
        footDevice->chargeStatus = foot->status.status;
        footDevice->firmwareVersion = foot->status.firmwareVersion;
        footDevice->freshStatus = true;
    }
    else if (foot->packetType == KAT_BT_DATA)
    {
        /// refresh data
        footDevice->color = foot->data.color;
        footDevice->speed_x = foot->data.speed_x;
        footDevice->speed_y = foot->data.speed_y;
        footDevice->status1 = foot->data.status1;
        footDevice->status2 = foot->data.status2;
        // TODO: add timestamp
        footDevice->fresh = true;
    }    
}
