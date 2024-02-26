#ifndef __KAT_H__
#define __KAT_H__
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/bluetooth/bluetooth.h>

typedef enum __packed
{
    KAT_NONE = 0,
    KAT_DIR,
    KAT_LEFT,
    KAT_RIGHT,
    _KAT_MAX_DEVICE
} tKatDevice;

// Internal representation

typedef struct
{
    tKatDevice deviceType;
} katConnectionInfo;

typedef struct
{
    // Status
    char firmwareVersion;
    char chargeStatus;
    short chargeLevel;
    bool freshStatus;
    // TODO: add timestamp
    bool freshData;
    int64_t connect_ts;
} tKatDeviceStatus;

typedef struct
{
    // Latest data
    char status1;
    char status2;
    short speed_x;
    short speed_y;
    char color;
} tKatFootDevice;

typedef struct
{
    // Latest data
    short axis[7];
    char button;
} tKatDirDevice;

typedef struct
{
    tKatDevice deviceType;
    tKatDeviceStatus deviceStatus;
    union
    {
        tKatFootDevice footData;
        tKatDirDevice dirData;
    };
} tKatDeviceInfo;

// Decoder from BT
void parseKatBtPacket(struct net_buf *req_buf, tKatDeviceInfo *katDevice);

// Encoder into USB
#define KAT_USB_PACKET_LEN 32
typedef uint8_t tKatUsbBuf[KAT_USB_PACKET_LEN];

void initKatUsb(void);
bool handle_kat_usb(uint8_t *buf, int size);
uint8_t *encodeKatUsbStatus(tKatDevice devId, tKatDeviceInfo *katDevice);
uint8_t *encodeKatUsbData(tKatDevice devId, tKatDeviceInfo *katDevice);

// TEMPORARY
extern bool usb_stream_enabled;
extern const bt_addr_le_t katDevices[];
extern const int numKatDevices;
extern tKatDeviceInfo devices[];
// TEMPORARY

#endif // __KAT_H__
