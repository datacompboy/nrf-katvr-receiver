#include <stdint.h>

#include <version.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>

#include "kat_usb.h"

// User-defined USB serial override.
char katUsbSerial[sizeof(CONFIG_USB_DEVICE_SN)];

//
// WARNING: Hack, relying on Zephyr's 3.4/3.6 internal implementation.
//
// In these versions USB descriptor is defined fixed and serial number is generated
// by calling usb_update_sn_string_descriptor() function that supposed to return
// runtime-defined serial number string.
//
// For our use case we need to be able to override the generated string is later,
// after USB device is already defined and announced, once SetSN command receiverd,
// or user-configured serial number loaded from the settings.
//
// Another required is to keep device-unique serial id unless it is overwritten...
// So the way to implement is:
//   - Default, device-unique, serial is generated during normal USB startup,
//   - Once we learned about user-supplied SN we should provide, find and
//     replace the contents of USB descriptor.
//     This is generally undefined behaviour of USB stack on both sides (PC and here),
//     but it proven to work with current KAT Gateway (2.5.2, which sends descriptor
//     request every time); and nRF52840 chipset.
//
// Tested on 3.4.99 only so far, but code is the same on 3.6.99.
//
#if (KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(3,4,0)) && (KERNEL_VERSION_NUMBER < ZEPHYR_VERSION(3,7,0))
struct usb_sn_descriptor {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bString[sizeof(CONFIG_USB_DEVICE_SN)-1];
} __packed;
#define USB_DESC_SERIAL_NUMBER_IDX			3
extern struct usb_desc_header __usb_descriptor_start[];
extern struct usb_desc_header __usb_descriptor_end[];
static struct usb_sn_descriptor * find_sn_descriptor()
{
    int descIdx = 0;
    struct usb_desc_header *head = __usb_descriptor_start;
    while (head->bLength != 0U) {
        if (head->bDescriptorType == USB_DESC_STRING) {
            if (descIdx == USB_DESC_SERIAL_NUMBER_IDX) {
                return (struct usb_sn_descriptor *)head;
            }
            descIdx++;
        }
        head = (struct usb_desc_header *)((uint8_t *)head + head->bLength);
    }
    return NULL;
}
void kat_usb_update_serial(void)
{
    struct usb_sn_descriptor * serial = find_sn_descriptor();
    if (!serial) {
        printk("Can't find USB Serial descriptor!\n");
        return;
    }
    if (serial->bLength != sizeof(CONFIG_USB_DEVICE_SN)  * 2) {
        printk("Unexpected USB Serial descriptor length!\n");
        return;
    }
    for (int i=0; i < sizeof(CONFIG_USB_DEVICE_SN)-1; ++i) {
        serial->bString[i] = katUsbSerial[i];
    }
    printk("Update of USB serial to %s\n", katUsbSerial);
}

#else  /* KERNEL_VERSION_NUMBER */
#error The USB Serial Update validated only for 3.4.99 .. 3.6.99. Fix needed.
#endif  /* KERNEL_VERSION_NUMBER */
