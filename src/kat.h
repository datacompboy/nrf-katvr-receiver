#ifndef __KAT_H__
#define __KAT_H__

typedef enum { KAT_NONE=0, KAT_DIR, KAT_LEFT, KAT_RIGHT, _KAT_MAX_DEVICE } tKatDevice;

typedef struct {
    tKatDevice deviceType;
} katConnectionInfo;

typedef struct {
    // Status
    tKatDevice deviceType;
    char firmwareVersion;
    char chargeStatus;
    short chargeLevel;
    bool freshStatus;
    // Latest data
    char status1;
    char status2;
    short speed_x;
    short speed_y;
    char color;
    // TODO: add timestamp
    bool fresh;
} tKatFootDevice;

typedef struct {
    union {
        tKatDevice deviceType;
        tKatFootDevice foot;
        //tKatDirDevice direction;
    };
} tKatDeviceInfo;

BUILD_ASSERT(offsetof(tKatDeviceInfo, foot.deviceType) == offsetof(tKatDeviceInfo, deviceType), "All tKatDeviceInfo memvers should have the same deviceType");


void parseFeet(struct net_buf *req_buf, tKatFootDevice* footDevice);


#endif // __KAT_H__