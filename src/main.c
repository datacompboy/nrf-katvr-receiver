#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/l2cap.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/sys/printk.h>

#include "kat.h"

// Ensure the console is USB-UART
BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
             "Console device is not ACM CDC UART device");

static bool try_send_update_packet(void);

/*
 * Bluetooth management.
 */

const char *const stKatDevice[] = {"KAT_NONE", "KAT_DIR", "KAT_LEFT", "KAT_RIGHT", "KAT_BUG"};
BUILD_ASSERT(ARRAY_SIZE(stKatDevice) == _KAT_MAX_DEVICE + 1, "Don't miss a thing!");

static katConnectionInfo connections[CONFIG_BT_MAX_CONN] = {KAT_NONE};
// Note: devices[0] is unused, array idexed by tKatDevice
tKatDeviceInfo devices[_KAT_MAX_DEVICE] = {{KAT_NONE}};
atomic_t numConnections = 0;

const bt_addr_le_t katDevices[] = {
    // KAT_DIR
    // {.type = BT_ADDR_LE_PUBLIC, .a = {.val = {0x01, 0x74, 0xEB, 0x16, 0x4D, 0xAC}}}, // AC:4D:16:EB:74:01 - direction - unpaired
    {.type = BT_ADDR_LE_PUBLIC, .a = {.val = {0xCA, 0xF8, 0xF8, 0x66, 0x98, 0x60}}}, // 60:98:66:F8:F8:CA - direction - paired

    // KAT_LEFT
    // {.type = BT_ADDR_LE_PUBLIC, .a = {.val = {0x54, 0x46, 0xF2, 0x66, 0x98, 0x60}}}, // 60:98:66:F2:46:54 - unpaired
    {.type = BT_ADDR_LE_PUBLIC, .a={.val={0x0B, 0x9D, 0xF3, 0x66, 0x98, 0x60}}}, // 60:98:66:F3:9D:0B - paired left

    // KAT_RIGHT
    {.type = BT_ADDR_LE_PUBLIC, .a = {.val = {0x6A, 0x1C, 0xF2, 0x66, 0x98, 0x60}}}, // 60:98:66:F2:1C:6A - paired right
};
const int numKatDevices = ARRAY_SIZE(katDevices);
BUILD_ASSERT(ARRAY_SIZE(katDevices) < _KAT_MAX_DEVICE, "We can keep only that much different devices");

static const struct bt_conn_le_create_param btConnCreateParam = {
    .options = BT_CONN_LE_OPT_NONE,
    .interval = BT_GAP_SCAN_FAST_INTERVAL,
    .timeout = 0,
    .window = 5,
};
static const struct bt_le_conn_param btConnParam = BT_LE_CONN_PARAM_INIT(15, 15, 0, 2000); // BT_LE_CONN_PARAM_INIT(7, 8, 0, 2000);
void do_resume_connections(struct k_work *work)
{
    ARG_UNUSED(work);
    if (atomic_get(&numConnections) < numKatDevices) {
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
K_WORK_DEFINE(resume_connections_work, do_resume_connections);
void exp_resume_connections(struct k_timer *timer)
{
    k_work_submit(&resume_connections_work);
}
K_TIMER_DEFINE(resume_connection_timer, exp_resume_connections, NULL);
void resume_connections()
{
    /* trigger fishing start with a delay, to let connection packets fly */
    k_timer_start(&resume_connection_timer, K_MSEC(100), K_NO_WAIT);
}

/* internal... */
int bt_conn_le_conn_update(struct bt_conn *conn, const struct bt_le_conn_param *param);
int bt_l2cap_update_conn_param(struct bt_conn *conn, const struct bt_le_conn_param *param);
/* *** */

static void device_connected(struct bt_conn *conn, uint8_t conn_err)
{
    if (!conn_err)
    {
        atomic_inc(&numConnections);
        int conn_num = bt_conn_index(conn);
        printk("Device connected (%p/%d)\n", conn, conn_num);

        // bt_l2cap_update_conn_param(conn, &btConnParam);

        if (connections[conn_num].deviceType == KAT_NONE)
        {
            int i;
            for (i = 0; i < numKatDevices; ++i)
            {
                struct bt_conn_info info;
                bt_conn_get_info(conn, &info);
                if (bt_addr_le_eq(info.le.dst, &katDevices[i]))
                {
                    tKatDevice dev = (tKatDevice)i + 1; // Shift by 1 from katDevices array
                    connections[conn_num].deviceType = dev;
                    devices[dev].deviceType = dev;
                    // To speedup gateway reconnect, report last known state if it is non-null
                    if (devices[dev].deviceStatus.firmwareVersion > 0)
                        devices[dev].deviceStatus.freshStatus = true;
                    devices[dev].deviceStatus.connect_ts = k_uptime_get();
                    printk("Connected device of type %s\n", stKatDevice[dev]);
                    try_send_update_packet();
                    break;
                }
            }
            if (i == numKatDevices)
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
    resume_connections();
}

static const struct bt_le_conn_param param = {
    // .interval_min = 7,
    // .interval_max = 8,
    .interval_min = btConnParam.interval_min,
    .interval_max = btConnParam.interval_max,
    .latency = 0,
    .timeout = 500,
};
static void auto_exchange_complete(struct bt_conn *conn, struct bt_conn_remote_info *remote)
{
    // Delay send connection change till end of other communication,
    // as any other communication may break update stream.
    int err = bt_conn_le_param_update(conn, &param);
    if (!err)
    {
        printk("Sent param update.\n");
    }
    else
    {
        printk("Error on bt_conn_le_param_update (%d/0x%02x)\n", err, err);
    }
    resume_connections();
}

static void device_disconnected(struct bt_conn *conn, uint8_t reason)
{
    atomic_dec(&numConnections);
    printk("Device disconnected (%p/0x%02x)\n", conn, reason);
    int conn_num = bt_conn_index(conn);
    connections[conn_num].deviceType = KAT_NONE;
    resume_connections();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = device_connected,
    .disconnected = device_disconnected,
    .remote_info_available = auto_exchange_complete,
};

//
// Track and print latest statistics for the received packets
//
#define LINEUP "\r\33[2K"
static int stat[6]={0};
static int stat_sec[6][6]={{0}};
static int64_t last_sec = -1;
void track_stat(int dev, int pack, struct bt_conn* conn)
{
    if (dev) {
        stat[(dev-1)*2+pack]++;

        int64_t now = k_uptime_get();
        if (now - last_sec >= 1000) {
            // Yeah yeah, i know, it's imprecise
            for (int i=0; i<ARRAY_SIZE(stat_sec)-1; ++i) {
                memcpy(&stat_sec[i], &stat_sec[i+1], sizeof(stat_sec[i]));
            }
            memcpy(&stat_sec[ARRAY_SIZE(stat_sec)-1], &stat, sizeof(stat));
            if (last_sec < 0)
                last_sec = now;
            else
                last_sec += 1000;

            #define SEC1s(i) (stat_sec[5][i]-stat_sec[4][i])
            #define SEC5s(i) ((stat_sec[5][i]-stat_sec[0][i])*1000/5)

            printk(LINEUP 
                        "Dd:%d Ds:%d Ld:%d Ls:%d Rd:%d Rs:%d  "
                        "D1d:%d D1s:%d L1d:%d L1s:%d R1d:%d R1s:%d  "
                        "D5d:%d D5s:%d L5d:%d L5s:%d R5d:%d R5s:%d  "
                    , stat[0], stat[1], stat[2], stat[3], stat[4], stat[5]
                    ,SEC1s(0),SEC1s(1),SEC1s(2),SEC1s(3),SEC1s(4),SEC1s(5)
                    ,SEC5s(0),SEC5s(1),SEC5s(2),SEC5s(3),SEC5s(4),SEC5s(5)
                    );
            // haaack. note : we check only once second pass, it could be not every dev is checked.
            if (false && SEC1s((dev-1)*2) < 30 && (now - devices[dev].deviceStatus.connect_ts) > 5000) {
                printk("Sensor too slow, reconnect...\n");
                bt_conn_disconnect(conn, BT_HCI_ERR_UNACCEPT_CONN_PARAM);
            }
        }
    }
}

/*
  Hack to get ATT stream before ATT/GATT driver.
*/
static int my_att_recv(struct bt_l2cap_chan *chan, struct net_buf *req_buf)
{
    int id = bt_conn_index(chan->conn);
    const tKatDevice dev = connections[id].deviceType;

#if CONFIG_APP_PACKET_LOG
    printk("Received packet for %p %8s [%d]", chan->conn, stKatDevice[dev], req_buf->len);
    for (int i = 0; i < req_buf->len; ++i)
    {
        printk(" %02x", req_buf->data[i]);
    }
    printk("\n");
#endif

    parseKatBtPacket(req_buf, &devices[dev]);
    /*
    if (req_buf->data[3] == 0 && req_buf->data[4] == 0)
    {
        printk("%s: charge_level=%d, charge_status=%d, firmware=%d\n",
               stKatDevice[connections[id].deviceType],
               devices[dev].deviceStatus.chargeLevel,
               devices[dev].deviceStatus.chargeStatus,
               devices[dev].deviceStatus.firmwareVersion);
    }
    */
    track_stat(dev, (req_buf->data[3] == 0 && req_buf->data[4] == 0), chan->conn);

    try_send_update_packet();
    return 0;
}

static struct bt_l2cap_le_chan my_att_chan_pool[CONFIG_BT_MAX_CONN];
static int my_att_accept(struct bt_conn *conn, struct bt_l2cap_chan **ch)
{
    static const struct bt_l2cap_chan_ops ops = {
        .recv = my_att_recv,
    };

    int id = bt_conn_index(conn);
    // printk("Capturing L2CAP ATT channel on connection %p/%d\n", conn, id);

    struct bt_l2cap_le_chan *chan = &my_att_chan_pool[id];
    chan->chan.ops = &ops;
    *ch = &chan->chan;
    return 0;
}

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

BT_L2CAP_CHANNEL_DEFINE(a_att_fixed_chan, BT_L2CAP_CID_ATT, my_att_accept, NULL);

/*
 * USB management.
 */

static const struct device *hiddev;
bool usb_stream_enabled = false;
atomic_t usbbusy = 0;
enum tUsbBusiness
{
    cUsbOutBusy
};
atomic_ptr_t usb_queue = NULL;

static void usb_write_and_forget(const struct device *dev, void *buf)
{
    int wrote = -1;
    hid_int_ep_write(dev, buf, KAT_USB_PACKET_LEN, &wrote); // feeling lucky
    if (wrote != KAT_USB_PACKET_LEN)
    {
        // This shouldn't happen. To avoid hanging USB, release it and keep trying other time.
        atomic_clear_bit(&usbbusy, cUsbOutBusy);
        printk("Send output bug: wrote only %d bytes instead of %d.\n", wrote, KAT_USB_PACKET_LEN);
    }
}

static void usb_send_or_queue(const struct device *dev, void *buf)
{
    // Check old business status, send if were free.
    if (!atomic_test_and_set_bit(&usbbusy, cUsbOutBusy))
    {
        usb_write_and_forget(dev, buf);
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
static bool send_update_packet(const struct device *dev)
{
    if (!usb_stream_enabled)
        return false;
    // Try send status first
    for (int i = 0; i < numKatDevices; ++i)
    {
        tKatDevice devId = (tKatDevice)i + 1;
        if (devices[devId].deviceStatus.freshStatus)
        {
            uint8_t *buf = encodeKatUsbStatus(devId, &devices[devId]);
            if (buf)
            {
                devices[devId].deviceStatus.freshStatus = false;
                usb_write_and_forget(dev, buf);
                return true;
            }
        }
    }
    // If there is no fresh status updates -- send data update
    for (int i = 0; i < numKatDevices; ++i)
    {
        tKatDevice devId = (tKatDevice)i + 1;
        if (devices[devId].deviceStatus.freshData)
        {
            uint8_t *buf = encodeKatUsbData(devId, &devices[devId]);
            if (buf)
            {
                devices[devId].deviceStatus.freshData = false;
                usb_write_and_forget(dev, buf);
                return true;
            }
        }
    }
    // No fresh data -- nothing to send.
    return false;
}

static bool try_send_update_packet(void)
{
    if (!usb_stream_enabled || !hiddev)
        return false;
    if (!atomic_test_and_set_bit(&usbbusy, cUsbOutBusy))
    {
        // if usb channel was free, we can try send packet.
        if (send_update_packet(hiddev))
        {
            return true;
        }
        // If nothing to send -- clear the lock.
        atomic_clear_bit(&usbbusy, cUsbOutBusy);
    }
    return false;
}

// Outgoing packet complete
static void int_in_ready_cb(const struct device *dev)
{
    // We enter assuming we own the business lock.
    // printk("USB int_in_ready_cb\n");
    // If there queued packed to send -- send it now.
    atomic_ptr_val_t queue = atomic_ptr_clear(&usb_queue);
    if (queue)
    {
        usb_write_and_forget(dev, queue);
    }
    else
    {
        // if there is no queued packets - try to send fresh update.
        if (!send_update_packet(dev))
        {
            // If there was nothing to send -- we clear out busy signal.
            atomic_clear_bit(&usbbusy, cUsbOutBusy);
        }
    }
}

// Handle incoming packet from PC
tKatUsbBuf usb_command_buf;
static void int_out_ready_cb(const struct device *dev)
{
    int read = -1;
    int err = hid_int_ep_read(dev, usb_command_buf, sizeof(usb_command_buf), &read);
    // printk("USB int_out_ready_cb (%d): read %d bytes\n", err, read);
    if (!err)
    {
        if (handle_kat_usb(usb_command_buf, read))
        {
            usb_send_or_queue(dev, usb_command_buf);
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

static const struct hid_ops usb_ops = {
    .int_in_ready = int_in_ready_cb,
    .int_out_ready = int_out_ready_cb,
};

int start_usb(void)
{
    int err;

    initKatUsb();

    hiddev = device_get_binding("HID_0");
    if (hiddev == NULL)
    {
        return -ENODEV;
    }

    usb_hid_register_device(hiddev, hid_report_desc, sizeof(hid_report_desc), &usb_ops);

    err = usb_hid_init(hiddev);
    if (err)
    {
        printk("usb_hid_init failed: %d\n", err);
        return err;
    }

    return usb_enable(NULL /*status_cb*/);
}

/*
 * Startup code.
 */

int main(void)
{
    int err;

    if (IS_ENABLED(CONFIG_USB_DEVICE_STACK))
    {
        const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

        err = start_usb();
        if (err && (err != -EALREADY))
        {
            printk("Failed to enable USB: %d\n", err);
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
    for (int i = 0; i < numKatDevices; ++i)
    {
        printk("Adding to accept list device %s...\n", stKatDevice[i + 1]);
        err = bt_le_filter_accept_list_add(&katDevices[i]);
        if (err)
        {
            printk("bt_le_filter_accept_list_add error (%d / 0x%02d)", err, err);
        }
    }

    resume_connections();

    return 0;
}
