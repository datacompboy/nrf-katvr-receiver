#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/l2cap.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/printk.h>

#include "kat_main.h"

#include "kat_ble.h"
#include "kat_usb.h"

/*
 * Bluetooth connections management.
 */
#ifdef CONFIG_APP_DEBUG
const char *const KatDeviceName[] = {"KAT_NONE", "KAT_DIR", "KAT_LEFT", "KAT_RIGHT", "KAT_BUG"};
BUILD_ASSERT(ARRAY_SIZE(KatDeviceName) == _KAT_MAX_DEVICE + 1, "Don't miss a thing!");
#endif

static eKatDevice KatBleConnectionDevice[CONFIG_BT_MAX_CONN] = {KAT_NONE};
static atomic_t NumKatBleConnections = ATOMIC_INIT(0);
static atomic_t KatBleBits = ATOMIC_INIT(0);
enum { bitBleActive, bitBleEnabled };

#ifdef CONFIG_SETTINGS
bt_addr_le_t KatBleDevices[_KAT_MAX_DEVICE-1] = {};
int NumKatBleDevices = 0;
#else
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
#endif

#ifdef CONFIG_APP_DEBUG
//
// Track and print latest statistics for the received packets
//
#define LINEUP "\r\33[2K"
static int __stat[6]={0};
static int __stat_sec[6][6]={{0}};
static int64_t __last_sec = 0;
static void __kat_ble_track_stat(int dev, int pack, struct bt_conn* conn)
{
    if (dev) {
        __stat[(dev-1)*2+pack]++;

        int64_t now = k_uptime_get();
        if (now - __last_sec >= 1000) {
            // Use simple moving averaging by keeping snapshots of counters for N+1 intervals.
            for (int i=0; i<ARRAY_SIZE(__stat_sec)-1; ++i) {
                memcpy(&__stat_sec[i], &__stat_sec[i+1], sizeof(__stat_sec[i]));
            }
            memcpy(&__stat_sec[ARRAY_SIZE(__stat_sec)-1], &__stat, sizeof(__stat));
            __last_sec += 1000;

            #define SEC1s(i) (__stat_sec[5][i]-__stat_sec[4][i])
            #define SEC5s(i) ((__stat_sec[5][i]-__stat_sec[0][i])*1000/5)
            printk(LINEUP 
                        "Dd:%d Ds:%d Ld:%d Ls:%d Rd:%d Rs:%d  "
                        "D1d:%d D1s:%d L1d:%d L1s:%d R1d:%d R1s:%d  "
                        "D5d:%d D5s:%d L5d:%d L5s:%d R5d:%d R5s:%d  "
                    , __stat[0], __stat[1], __stat[2], __stat[3], __stat[4], __stat[5]
                    ,SEC1s(0),SEC1s(1),SEC1s(2),SEC1s(3),SEC1s(4),SEC1s(5)
                    ,SEC5s(0),SEC5s(1),SEC5s(2),SEC5s(3),SEC5s(4),SEC5s(5)
                    );

            // haaack. note : we check only once second pass, it could be not every dev is checked.
            if (false && SEC1s((dev-1)*2) < 30 && (now - KatDeviceInfo[dev].deviceStatus.connect_ts) > 5000) {
                printk("Sensor too slow, reconnect...\n");
                bt_conn_disconnect(conn, BT_HCI_ERR_UNACCEPT_CONN_PARAM);
            }
            #undef SEC1s
            #undef SEC5s
        }
    }
}
#else  /* CONFIG_APP_DEBUG */
#define __kat_ble_track_stat(...) do{}while(0)
#endif  /* CONFIG_APP_DEBUG */


static void kat_ble_do_disconnect(struct bt_conn *conn, void *data)
{
    ARG_UNUSED(data);
    bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

static void kat_ble_disconnect(void)
{
    bt_conn_create_auto_stop();
    bt_conn_foreach(BT_CONN_TYPE_LE, kat_ble_do_disconnect, NULL);
}

#ifdef CONFIG_APP_KEEP_CONNECTIONS
#define _disconnect_delay K_SECONDS(30)
#else
#define _disconnect_delay K_MSEC(100)
#endif
ASYNC_FUNC(kat_ble_async_disconnect, _disconnect_delay)
{
    // If we weren't re-enabled in meanwhile -- trigger disconnect.
    if (! atomic_test_bit(&KatBleBits, bitBleEnabled)) {
        atomic_clear_bit(&KatBleBits, bitBleActive);
        kat_ble_disconnect();
    }
}

static int kat_ble_setup_filter(void)
{
    int err = bt_le_filter_accept_list_clear();
    if (err)
    {
        printk("bt_le_filter_accept_list_clear error (%d / 0x%02d)", err, err);
        return err;
    }

    for (int i = 0; i < NumKatBleDevices; ++i)
    {
        if (! bt_addr_le_eq(&bt_addr_le_any, &KatBleDevices[i])) {
            char addr_str[BT_ADDR_LE_STR_LEN];
            bt_addr_le_to_str(&KatBleDevices[i], addr_str, sizeof(addr_str));
            #ifdef CONFIG_APP_DEBUG
            printk("Adding to accept list device %s: %s\n", KatDeviceName[i + 1], addr_str);
            #endif
            err = bt_le_filter_accept_list_add(&KatBleDevices[i]);
            if (err)
            {
                printk("bt_le_filter_accept_list_add error (%d / 0x%02d)", err, err);
                return err;
            }
        }
    }

    return 0;
}

BUILD_ASSERT(CONFIG_BT_CTLR_SDC_MAX_CONN_EVENT_LEN_DEFAULT <= 2000, "We need shortest possible connection intervals");
static const struct bt_conn_le_create_param btConnCreateParam = BT_CONN_LE_CREATE_PARAM_INIT(BT_CONN_LE_OPT_NONE, BT_GAP_SCAN_FAST_INTERVAL, 0x10);

// Note: btConnParam and btConnParamUpdate should differ by only for timeout argument.
#define KAT_BT_CONN_PARAM(timeout) BT_LE_CONN_PARAM_INIT(CONFIG_APP_KAT_CONN_INTERVAL, CONFIG_APP_KAT_CONN_INTERVAL, 5, timeout)
static struct bt_le_conn_param btConnParam = KAT_BT_CONN_PARAM(500);
static struct bt_le_conn_param btConnParamUpdate = KAT_BT_CONN_PARAM(100);

#ifdef CONFIG_APP_KAT_FREQ_PARAM
short KatBleUpdateFrequency = 86;
void kat_ble_update_freq_param()
{
    short interval = 800 / KatBleUpdateFrequency;
    interval = CLAMP(interval, 6, 16);
    KatBleUpdateFrequency = 800 / interval;
    btConnParam.interval_max = interval;
    btConnParam.interval_min = interval;
    btConnParamUpdate.interval_max = interval;
    btConnParamUpdate.interval_min = interval;
    printk("Set connection interval to %d (%d Hz update rate)\n", interval, KatBleUpdateFrequency);
}
#endif

ASYNC_FUNC(kat_ble_connect_next, K_MSEC(100))
{
    if (atomic_test_bit(&KatBleBits, bitBleActive) && atomic_get(&NumKatBleConnections) < NumKatBleDevices) {
        int err = bt_conn_le_create_auto(&btConnCreateParam, &btConnParam);
        if (err == -EALREADY) {

        }
        else if (err && err != -EALREADY)
        {
            printk("bt_conn_le_create_auto error (%d)", err);
        }
        else
        {
            printk("Fishing for known device in advertising stream...\n");
        }
    }
}

ASYNC_FUNC(kat_ble_update_devices, K_MSEC(100))
{
    kat_ble_disconnect();
    kat_ble_setup_filter();
    if (IS_ENABLED(CONFIG_APP_DEBUG_BOOT_CONNECTIONS)) {
        kat_ble_connect_next();
    }
}

static void kat_ble_connected_cb(struct bt_conn *conn, uint8_t conn_err)
{
    if (!conn_err)
    {
        atomic_inc(&NumKatBleConnections);
        int conn_num = bt_conn_index(conn);
        printk("Device connected (%p/%d)\n", conn, conn_num);

        if (KatBleConnectionDevice[conn_num] == KAT_NONE)
        {
            int i;
            for (i = 0; i < NumKatBleDevices; ++i)
            {
                struct bt_conn_info info;
                bt_conn_get_info(conn, &info);
                if (bt_addr_le_eq(info.le.dst, &KatBleDevices[i]))
                {
                    eKatDevice dev = (eKatDevice)i + 1; // Shift by 1 from katDevices array
                    KatBleConnectionDevice[conn_num] = dev;
                    KatDeviceInfo[dev].deviceType = dev;
                    #ifdef CONFIG_APP_DEBUG
                    printk("Connected device of type %s\n", KatDeviceName[dev]);
                    #endif
                    // Not really helpful for KAT Gateway, which waits for several seconds anyway.
                    // But for native input reading sake, get fresh update ASAP is useful.
                    if (KatDeviceInfo[dev].deviceStatus.firmwareVersion > 0)
                        KatDeviceInfo[dev].deviceStatus.freshStatus = true;
                    KatDeviceInfo[dev].deviceStatus.connect_ts = k_uptime_get();
                    kat_usb_try_send_update();

                    break;
                }
            }
            if (i == NumKatBleDevices)
            {
                printk("Unexpected device connected, disconnect!");
                bt_conn_disconnect(conn, BT_HCI_ERR_UNACCEPT_CONN_PARAM);
            }
        }
    }
    else
    {
        printk("Connect error (%p/%x)\n", conn, conn_err);
        bt_conn_unref(conn);
    }
    kat_ble_connect_next();
}

static void kat_ble_auto_exchange_complete_cb(struct bt_conn *conn, struct bt_conn_remote_info *remote)
{
    // Delay send connection change till end of other communication,
    // as any other communication may break update stream.
    int err = bt_conn_le_param_update(conn, &btConnParamUpdate);
    if (!err)
    {
        printk("Sent param update.\n");
    }
    else
    {
        printk("Error on bt_conn_le_param_update (%d/0x%02x)\n", err, err);
    }
    kat_ble_connect_next();
}

static void kat_ble_disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
    atomic_dec(&NumKatBleConnections);
    printk("Device disconnected (%p/0x%02x)\n", conn, reason);
    int conn_num = bt_conn_index(conn);
    KatBleConnectionDevice[conn_num] = KAT_NONE;
    kat_ble_connect_next();
}

BT_CONN_CB_DEFINE(KatBleConnectionCallbacks) = {
    .connected = kat_ble_connected_cb,
    .disconnected = kat_ble_disconnected_cb,
    .remote_info_available = kat_ble_auto_exchange_complete_cb,
};

/*
  Hack to get ATT stream before ATT/GATT driver.
*/
static int kat_ble_att_recv_cb(struct bt_l2cap_chan *chan, struct net_buf *req_buf)
{
    int id = bt_conn_index(chan->conn);
    const eKatDevice dev = KatBleConnectionDevice[id];

#if CONFIG_APP_PACKET_LOG
    printk("Received packet for %p %8s [%d]", chan->conn, KatDeviceName[dev], req_buf->len);
    for (int i = 0; i < req_buf->len; ++i)
    {
        printk(" %02x", req_buf->data[i]);
    }
    printk("\n");
#endif

    kat_ble_handle_packet(req_buf, &KatDeviceInfo[dev]);
#if CONFIG_APP_PACKET_LOG
    if (req_buf->data[3] == 0 && req_buf->data[4] == 0)
    {
        printk("%s: charge_level=%d, charge_status=%d, firmware=%d\n",
               KatDeviceName[KatBleConnectionDevice[id]],
               KatDeviceInfo[dev].deviceStatus.chargeLevel,
               KatDeviceInfo[dev].deviceStatus.chargeStatus,
               KatDeviceInfo[dev].deviceStatus.firmwareVersion);
    }
#endif
    __kat_ble_track_stat(dev, (req_buf->data[3] == 0 && req_buf->data[4] == 0), chan->conn);

    kat_usb_try_send_update();
    return 0;
}

static int kat_ble_att_accept_cb(struct bt_conn *conn, struct bt_l2cap_chan **ch)
{
    static struct bt_l2cap_le_chan att_chan_pool[CONFIG_BT_MAX_CONN];
    static const struct bt_l2cap_chan_ops ops = {
        .recv = kat_ble_att_recv_cb,
    };
    int id = bt_conn_index(conn);
    struct bt_l2cap_le_chan *chan = &att_chan_pool[id];
    chan->chan.ops = &ops;
    *ch = &chan->chan;
    return 0;
}

//
// WARNING: Hack, relying on Zephyr's 3.4/3.6 internal implementation.
// 
// As sensors sends GATT update without GATT characteristics definitions announcements,
// we install our own fixed L2CAP channel 0x0004 handler, alphabetically named early so
// we are installed before zerphyr's own GATT handler if enabled.
//
// Tested on 3.4.99 only so far, but code is the same on 3.6.99.
//
#if (KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(3,4,0)) && (KERNEL_VERSION_NUMBER < ZEPHYR_VERSION(3,7,0))

#ifndef BT_L2CAP_CHANNEL_DEFINE
// Include l2cap_internal.h if you build in-tree; otherwise, let's just steal definitions from compatible (v2.5.2)
struct bt_l2cap_fixed_chan
{
    uint16_t cid;
    int (*accept)(struct bt_conn *conn, struct bt_l2cap_chan **chan);
    bt_l2cap_chan_destroy_t destroy;
};
#define BT_L2CAP_CHANNEL_DEFINE(_name, _cid, _accept, _destroy)   \
    const STRUCT_SECTION_ITERABLE(bt_l2cap_fixed_chan, _name) = { \
        .cid = _cid,                                              \
        .accept = _accept,                                        \
        .destroy = _destroy,                                      \
    }
#define BT_L2CAP_CID_ATT 0x0004
#endif

BT_L2CAP_CHANNEL_DEFINE(a_att_fixed_chan, BT_L2CAP_CID_ATT, kat_ble_att_accept_cb, NULL);
#else  /* KERNEL_VERSION_NUMBER */
#error The USB Serial Update validated only for 3.4.99 .. 3.6.99. Fix needed.
#endif  /* KERNEL_VERSION_NUMBER */


/*
 * Public KAT BLE subsystem oeprations
*/

void kat_ble_enable_connections(void)
{
    atomic_set_bit(&KatBleBits, bitBleEnabled);
    atomic_set_bit(&KatBleBits, bitBleActive);
    kat_ble_connect_next();
    // Stop disconnection timer if it was active
    // (note: that will run it's function, but due to set bitBleEnabled it'll be noop)
    k_timer_stop(&_timer_kat_ble_async_disconnect);
}

void kat_ble_disable_connections(void)
{
    atomic_clear_bit(&KatBleBits, bitBleEnabled);
    kat_ble_async_disconnect();
}

void kat_ble_get_localaddr(uint8_t* output, size_t bufsize)
{
    bt_addr_le_t addr;
    size_t count = 1;
    bt_id_get(&addr, &count); // get only the 1st
    memccpy(output, addr.a.val, sizeof(addr.a.val), bufsize);
}

void kat_ble_init(void)
{
    int err = bt_enable(NULL);
    if (err)
    {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }
    printk("Bluetooth initialized\n");
    kat_ble_setup_filter();
    if (IS_ENABLED(CONFIG_APP_DEBUG_BOOT_CONNECTIONS)) {
        kat_ble_enable_connections();
    }
}
