#include <stdlib.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/sys/printk.h>

#include "kat_main.h"
#include "kat_ble.h"
#include "kat_usb.h"

#ifdef CONFIG_APP_DEBUG
// Ensure the console is USB-UART
BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
             "Console device is not ACM CDC UART device");
#endif

/*
 * USB management.
 */

static const struct device *usb_hid_dev;
static atomic_t usbFlags = ATOMIC_INIT(0);
enum tUsbBusiness
{
    bitUsbOutBusy,
    bitUsbStreamEnabled,
};
static atomic_ptr_t usb_queue = ATOMIC_PTR_INIT(NULL);


void kat_usb_enable_stream(void)
{
    atomic_set_bit(&usbFlags, bitUsbStreamEnabled);
}

void kat_usb_disable_stream(void)
{
    atomic_clear_bit(&usbFlags, bitUsbStreamEnabled);
}


static void kat_usb_write_and_forget(const struct device *dev, void *buf)
{
    int wrote = -1;
    hid_int_ep_write(dev, buf, KAT_USB_PACKET_LEN, &wrote); // feeling lucky
    if (wrote != KAT_USB_PACKET_LEN)
    {
        // This shouldn't happen. To avoid hanging USB, release it and keep trying other time.
        atomic_clear_bit(&usbFlags, bitUsbOutBusy);
        printk("Send output bug: wrote only %d bytes instead of %d.\n", wrote, KAT_USB_PACKET_LEN);
    }
}

static void kat_usb_send_or_queue(const struct device *dev, void *buf)
{
    // Check old business status, send if were free.
    if (!atomic_test_and_set_bit(&usbFlags, bitUsbOutBusy))
    {
        kat_usb_write_and_forget(dev, buf);
    }
    else
    {
        // Queue the buffer till the next time.
        if (!atomic_ptr_cas(&usb_queue, NULL, buf))
        {
            printk("Output queue is busy. Packet is lost. [should not happen (tm)]");
        }
    }
}

// Try to send packet if there something to send
static bool kat_send_update_packet(const struct device *dev)
{
    if (!atomic_test_bit(&usbFlags, bitUsbStreamEnabled))
        return false;
    uint8_t* buf = kat_usb_get_update_packet();
    if (buf) {
        kat_usb_write_and_forget(dev, buf);
        return true;
    }
    // No fresh data -- nothing to send.
    return false;
}

bool kat_usb_try_send_update(void)
{
    if (!usb_hid_dev || !atomic_test_bit(&usbFlags, bitUsbStreamEnabled))
        return false;
        
    if (!atomic_test_and_set_bit(&usbFlags, bitUsbOutBusy))
    {
        // if usb channel was free, we can try send packet.
        if (kat_send_update_packet(usb_hid_dev))
        {
            return true;
        }
        // If nothing to send -- clear the lock.
        atomic_clear_bit(&usbFlags, bitUsbOutBusy);
    }
    return false;
}

// Outgoing packet complete
static void kat_usb_int_in_ready_cb(const struct device *dev)
{
    // We enter assuming we own the business lock.
    // If there queued packed to send -- send it now.
    atomic_ptr_val_t queue = atomic_ptr_clear(&usb_queue);
    if (queue)
    {
        kat_usb_write_and_forget(dev, queue);
    }
    else
    {
        // if there is no queued packets - try to send fresh update.
        if (!kat_send_update_packet(dev))
        {
            // If there was nothing to send -- we clear out busy signal.
            atomic_clear_bit(&usbFlags, bitUsbOutBusy);
        }
    }
}

// Handle incoming packet from PC
tKatUsbBuf usb_command_buf;
static void kat_usb_int_out_ready_cb(const struct device *dev)
{
    int read = -1;
    int err = hid_int_ep_read(dev, usb_command_buf, sizeof(usb_command_buf), &read);
    if (!err)
    {
        if (kat_usb_handle_request(usb_command_buf, read))
        {
            kat_usb_send_or_queue(dev, usb_command_buf);
        }
    }
}

static const uint8_t hid_report_desc[] = {
    HID_ITEM(HID_ITEM_TAG_USAGE_PAGE, HID_ITEM_TYPE_GLOBAL, 2),
    0xA0,
    0xFF,                                       // Usage Page (Vendor Defined 0xFFA0)
    HID_USAGE(0x01),                            // Usage (0x01)
    HID_COLLECTION(HID_COLLECTION_APPLICATION), // Collection (Application)
    HID_USAGE(0x01),                            //   Usage (0x01)
    HID_LOGICAL_MIN8(0x00),                     //   Logical Minimum (0)
    HID_LOGICAL_MAX16(0xFF, 0x00),              //   Logical Maximum (0xFF)
    HID_REPORT_SIZE(8),                         //   Report Size (8)
    HID_REPORT_COUNT(32),                       //   Report Count (32)
    HID_INPUT(0x02),                            //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    HID_USAGE(0x02),                            //   Usage (0x02)
    HID_REPORT_SIZE(8),                         //   Report Size (8)
    HID_REPORT_COUNT(32),                       //   Report Count (32)
    HID_OUTPUT(0x02),                           //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    HID_USAGE(0x03),                            //   Usage (0x03)
    HID_REPORT_SIZE(8),                         //   Report Size (8)
    HID_REPORT_COUNT(5),                        //   Report Count (5) -- why?!
    HID_FEATURE(0x02),                          //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    HID_END_COLLECTION,
};

static const struct hid_ops kat_usb_hid_ops = {
    .int_in_ready = kat_usb_int_in_ready_cb,
    .int_out_ready = kat_usb_int_out_ready_cb,
};

int kat_usb_init(void)
{
    int err;

    kat_usb_init_buffers();

    usb_hid_dev = device_get_binding("HID_0");
    if (usb_hid_dev == NULL)
    {
        return -ENODEV;
    }

    usb_hid_register_device(usb_hid_dev, hid_report_desc, sizeof(hid_report_desc), &kat_usb_hid_ops);

    err = usb_hid_init(usb_hid_dev);
    if (err)
    {
        printk("usb_hid_init failed: %d\n", err);
        return err;
    }

    err = usb_enable(NULL);
    if (err)
    {
        printk("usb_enable failed: %d\n", err);
    }

    if (IS_ENABLED(CONFIG_APP_WAIT_FOR_OBSERVER))
    {
        const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

        /* Poll if the DTR flag was set */
        uint32_t dtr = 0;
        while (!dtr)
        {
            uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
            /* Give CPU resources to low priority threads. */
            k_sleep(K_MSEC(100));
        }
    }

    return err;
}
