#ifndef __KAT_USB_H__
#define __KAT_USB_H__
#include <stdbool.h>
#include <stdint.h>

#include "kat_main.h"

/*
 * USB subsystem management API and settings.
 */

int kat_usb_init(void);
bool kat_usb_try_send_update(void);

extern char katUsbSerial[sizeof(CONFIG_USB_DEVICE_SN)];
void kat_usb_update_serial(void);

/*
 * USB subsystem internal part.
 */

#define KAT_USB_PACKET_LEN 32
typedef uint8_t tKatUsbBuf[KAT_USB_PACKET_LEN];

void kat_usb_init_buffers(void);
bool kat_usb_handle_request(uint8_t *buf, int size);
uint8_t * kat_usb_get_update_packet(void);
void kat_usb_enable_stream(void);
void kat_usb_disable_stream(void);

#endif // __KAT_USB_H__
