#ifndef __KAT_BLE_H__
#define __KAT_BLE_H__
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/bluetooth/bluetooth.h>

#include "kat_main.h"

/*
 * BLE subsystem management API and settings.
 */

// Number of paired bluetooth sensors
extern int NumKatBleDevices;

// Addresses list of paired bluetooth sensors
extern bt_addr_le_t KatBleDevices[_KAT_MAX_DEVICE-1];

void kat_ble_init(void);
void kat_ble_update_devices(void);
void kat_ble_enable_connections(void);
void kat_ble_disable_connections(void);
void kat_ble_get_localaddr(uint8_t* output, size_t bufsize);

/*
 * BLE subsystem internal part.
 */

// Bluetooth KAT sensors packet parser.
void kat_ble_handle_packet(struct net_buf *req_buf, sKatDeviceInfo *katDevice);



#endif // __KAT_BLE_H__
