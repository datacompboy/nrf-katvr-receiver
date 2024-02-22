#ifndef __KAT_H__
#define __KAT_H__

typedef enum __packed { KAT_NONE=0, KAT_DIR, KAT_LEFT, KAT_RIGHT, _KAT_MAX_DEVICE } tKatDevice;

typedef struct {
    tKatDevice deviceType;
} katConnectionInfo;

typedef struct {
    // Status
    char firmwareVersion;
    char chargeStatus;
    short chargeLevel;
    bool freshStatus;
    // TODO: add timestamp
    bool freshData;
} tKatDeviceStatus;

typedef struct {
    // Latest data
    char status1;
    char status2;
    short speed_x;
    short speed_y;
    char color;
} tKatFootDevice;

typedef struct {
    // Latest data
    short axis[7];
    char button;
} tKatDirDevice;

typedef struct {
    tKatDevice deviceType;
    tKatDeviceStatus deviceStatus;
    union {
        tKatFootDevice footData;
        tKatDirDevice dirData;
    };
} tKatDeviceInfo;

void parseKatBtPacket(struct net_buf *req_buf, tKatDeviceInfo* katDevice);

#endif // __KAT_H__