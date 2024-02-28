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
extern char katUsbSerial[sizeof(CONFIG_USB_DEVICE_SN)];
void update_usb_serial(void);

extern bool usb_stream_enabled;
extern bt_addr_le_t katDevices[_KAT_MAX_DEVICE-1];
extern int numKatDevices;
extern tKatDeviceInfo devices[];
void reset_bt_filter(void);
void resume_connections(void);
void async_save_settings(void);
// TEMPORARY


#define ASYNC_FUNC(name, delay)                                 \
    void _do_async_##name(struct k_work *);                     \
    K_WORK_DEFINE(_worker_##name, _do_async_##name);            \
    void _timer_exp_##name(struct k_timer *timer)               \
    {                                                           \
        k_work_submit(&_worker_##name);                         \
    }                                                           \
    K_TIMER_DEFINE(_timer_##name, _timer_exp_##name, NULL);     \
    void name(void)                                             \
    {                                                           \
        k_timer_start(&_timer_##name, delay, K_NO_WAIT);        \
    }                                                           \
    void _do_async_##name(struct k_work *)


#endif // __KAT_H__
