#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/sys/printk.h>

// Ensure the console is USB-UART
BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
             "Console device is not ACM CDC UART device");

int main(void)
{
        int err;

        if (IS_ENABLED(CONFIG_USB_DEVICE_STACK))
        {
                const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

                err = usb_enable(NULL);
                if (err && (err != -EALREADY))
                {
                        printk("Failed to enable USB");
                        return err;
                }

                if (IS_ENABLED(CONFIG_APP_WAIT_FOR_OBSERVER))
                {
                        /* Poll if the DTR flag was set */
                        uint32_t dtr = 0;
                        while (!dtr)
                        {
                                uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
                                /* Give CPU resources to low priority threads. */
                                k_sleep(K_MSEC(100));
                        }
                }
        }

        printk("*** nRF KAT Receiver ***\n");
        err = bt_enable(NULL);
        if (err)
        {
                printk("Bluetooth init failed (err %d)\n", err);
                return 0;
        }

        printk("Bluetooth initialized\n");

        return 0;
}
