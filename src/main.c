#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/printk.h>

#include "kat_main.h"
#include "kat_ble.h"
#include "kat_usb.h"

/*
 * The global state.
 */

// Note: KatDeviceInfo[0] is unused, array idexed by eKatDevice
sKatDeviceInfo KatDeviceInfo[_KAT_MAX_DEVICE] = {{KAT_NONE}};

#ifdef CONFIG_APP_FEET_ROTATION
// Angle to turn data from left and right sensors before sending to PC.
float KatCorrectLeft = 0.0f;
float KatCorrectRight = 0.0f;
#endif

/*
 * Settings management.
 */

#ifdef CONFIG_SETTINGS

#define READ_SIMPLE_SETTING(NAME, VAR, DEFAULT) \
    if (settings_name_steq(key, NAME, &next) && !next) { \
        if (len != sizeof(VAR)) {                        \
            return -EINVAL;                              \
        }                                                \
        ret = read_cb(cb_arg, &VAR, sizeof(VAR));        \
        if (ret < 0) {                                   \
            VAR = DEFAULT;                               \
            return ret;                                  \
        }                                                \
        return 0;                                        \
    }

int kat_settings_set_cb(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
    const char *next;
    int ret;

#ifdef CONFIG_APP_FEET_ROTATION
    READ_SIMPLE_SETTING("correctLeft", KatCorrectLeft, 0.0f);
    READ_SIMPLE_SETTING("correctRight", KatCorrectRight, 0.0f);
#endif
#ifdef CONFIG_APP_KAT_FREQ_PARAM
    READ_SIMPLE_SETTING("updateFreq", KatBleUpdateFrequency, (kat_ble_update_freq_param(), 86));
#endif
    READ_SIMPLE_SETTING("devCnt", NumKatBleDevices, 0);

    if (settings_name_steq(key, "serial", &next) && !next) {
        if (len != sizeof(katUsbSerial)) {
            return -EINVAL;
        }

        ret = read_cb(cb_arg, &katUsbSerial, sizeof(katUsbSerial));
        if (ret < 0) {
            strncpy(katUsbSerial, CONFIG_USB_DEVICE_SN, sizeof(katUsbSerial));
            katUsbSerial[sizeof(katUsbSerial)-1] = 0;
            return ret;
        }
        kat_usb_update_serial();
        return 0;
    }

    if (settings_name_steq(key, "dev", &next) && next && *next) {
        int dev = atoi(next);

        if (dev < 0 || dev > ARRAY_SIZE(KatBleDevices)) {
            return -ENOENT;
        }

        if (len != sizeof(KatBleDevices[dev].a)) {
            return -EINVAL;
        }

        KatBleDevices[dev].type = BT_ADDR_LE_PUBLIC; // Well, it's just zero, so noop.
        ret = read_cb(cb_arg, &KatBleDevices[dev].a, sizeof(KatBleDevices[dev].a));
        if (ret < 0) {
            memset(&KatBleDevices[dev].a, 0, sizeof(KatBleDevices[dev].a));
            return ret;
        }
        return 0;
    }

    return -ENOENT;
}

int kat_settings_export_cb(int(*export_func)(const char *name, const void *val, size_t val_len))
{
    int ret;

#ifdef CONFIG_APP_FEET_ROTATION
    ret = export_func("katrc/correctLeft", &KatCorrectLeft, sizeof(KatCorrectLeft));
    if (ret < 0) return ret;

    ret = export_func("katrc/correctRight", &KatCorrectRight, sizeof(KatCorrectRight));
    if (ret < 0) return ret;
#endif

#ifdef CONFIG_APP_KAT_FREQ_PARAM
    ret = export_func("katrc/updateFreq", &KatBleUpdateFrequency, sizeof(KatBleUpdateFrequency));
    if (ret < 0) return ret;
#endif

    ret = export_func("katrc/serial", &katUsbSerial, sizeof(katUsbSerial));
    if (ret < 0) return ret;

    ret = export_func("katrc/devCnt", &NumKatBleDevices, sizeof(NumKatBleDevices));
    if (ret < 0) return ret;

    char argstr[100] = "katrc/dev/";
    char * argsuffix = &argstr[strlen(argstr)]; // argsuffix now is the pointer beyond "/"
    for (int dev = 0; dev < NumKatBleDevices; ++dev) {
        sprintf(argsuffix, "%d", dev); // argstr now katrc/dev/N
        ret = export_func(argstr, &KatBleDevices[dev].a, sizeof(KatBleDevices[dev].a));
        if (ret < 0) return ret;
    }
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(
    katrc, "katrc", /*get=*/NULL, /*set=*/kat_settings_set_cb, /*commit=*/NULL, /*export=*/kat_settings_export_cb);

ASYNC_FUNC(kat_settings_async_save, K_MSEC(1000))
{
    int err = settings_save();
    if (err) {
        printk("Error saving settings: %d\n", err);
    }
}

void kat_settings_init()
{
    int err = settings_subsys_init();
    if (err)
    {
        printk("Error starting settings subsystem (err %d)\n", err);
    }
    err = settings_load();
    if (err)
    {
        printk("Error loading settings (err %d)\n", err);
    }
}

#else  // CONFIG_SETTINGS

bt_addr_le_t KatBleDevices[] = {
    // KAT_DIR
    // {.type = BT_ADDR_LE_PUBLIC, .a = {.val = {0x01, 0x74, 0xEB, 0x16, 0x4D, 0xAC}}}, // AC:4D:16:EB:74:01 - direction - unpaired
    {.type = BT_ADDR_LE_PUBLIC, .a = {.val = {0xCA, 0xF8, 0xF8, 0x66, 0x98, 0x60}}}, // 60:98:66:F8:F8:CA - direction - paired

    // KAT_LEFT
    // {.type = BT_ADDR_LE_PUBLIC, .a = {.val = {0x54, 0x46, 0xF2, 0x66, 0x98, 0x60}}}, // 60:98:66:F2:46:54 - unpaired
    {.type = BT_ADDR_LE_PUBLIC, .a={.val={0x0B, 0x9D, 0xF3, 0x66, 0x98, 0x60}}}, // 60:98:66:F3:9D:0B - paired left

    // KAT_RIGHT
    {.type = BT_ADDR_LE_PUBLIC, .a = {.val = {0x6A, 0x1C, 0xF2, 0x66, 0x98, 0x60}}}, // 60:98:66:F2:1C:6A - paired right
};
int NumKatBleDevices = ARRAY_SIZE(KatBleDevices);
BUILD_ASSERT(ARRAY_SIZE(KatBleDevices) < _KAT_MAX_DEVICE, "We can keep only that much different devices");

#define kat_settings_async_save do{}while(0)
#define kat_settings_init do{}while(0)

#endif  // CONFIG_SETTINGS

/*
 * Startup code.
 */

int main(void)
{
    if (IS_ENABLED(CONFIG_APP_DEBUG))
    {
        // Start USB subsystem first, as it also the one that provides console (in debug build).
        kat_usb_init();
        kat_settings_init();
    } else {
        // In release mode, enable USB after settings load to ensure device serial announced from the beginning
        kat_settings_init();
        kat_usb_init();
    }
    printk("*** nRF KAT Receiver ***\n");
    kat_ble_init();
    return 0;
}
