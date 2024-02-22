#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/l2cap.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/sys/printk.h>

#include "kat.h"

// Ensure the console is USB-UART
BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
             "Console device is not ACM CDC UART device");

const char *const stKatDevice[] = {"KAT_NONE", "KAT_DIR", "KAT_LEFT", "KAT_RIGHT", "KAT_BUG"};
BUILD_ASSERT(ARRAY_SIZE(stKatDevice) == _KAT_MAX_DEVICE + 1, "Don't miss a thing!");

static katConnectionInfo connections[CONFIG_BT_MAX_CONN] = {KAT_NONE};
static tKatDeviceInfo devices[_KAT_MAX_DEVICE] = {{KAT_NONE}};

const bt_addr_le_t katDevices[] = {
    // KAT_DIR
    {.type = BT_ADDR_LE_PUBLIC, .a = {.val = {0x01, 0x74, 0xEB, 0x16, 0x4D, 0xAC}}}, // AC:4D:16:EB:74:01 - direction
    // KAT_LEFT
    {.type = BT_ADDR_LE_PUBLIC, .a = {.val = {0x54, 0x46, 0xF2, 0x66, 0x98, 0x60}}}, // 60:98:66:F2:46:54 - unpaired
    // {.type=BT_ADDR_LE_PUBLIC, .a={.val={0x0B, 0x9D, 0xF3, 0x66, 0x98, 0x60}}}, // 60:98:66:F3:9D:0B - paired left
    // KAT_RIGHT
    {.type = BT_ADDR_LE_PUBLIC, .a = {.val = {0x6A, 0x1C, 0xF2, 0x66, 0x98, 0x60}}}, // 60:98:66:F2:1C:6A - paired right
};
const int numKatDevices = ARRAY_SIZE(katDevices);
BUILD_ASSERT(ARRAY_SIZE(katDevices) < _KAT_MAX_DEVICE, "We can keep only that much different devices");

void do_resume_connections(struct k_work *work)
{
    ARG_UNUSED(work);
    int err = bt_conn_le_create_auto(BT_CONN_LE_CREATE_CONN_AUTO, BT_LE_CONN_PARAM_DEFAULT);
    if (err && err != -EALREADY)
    {
        printk("bt_conn_le_create_auto error (%d)", err);
    }
    else
    {
        printk("Fishing for known device in advertising stream...\n");
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

static void device_connected(struct bt_conn *conn, uint8_t conn_err)
{
    if (!conn_err)
    {
        int conn_num = bt_conn_index(conn);
        printk("Device connected (%p/%d)\n", conn, conn_num);
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
                    // Don't wipe last known settings to speedup reconnection
                    //   memset(&devices[dev], 0, sizeof(devices[dev]));
                    devices[dev].deviceType = dev;
                    printk("Connected device of type %s\n", stKatDevice[dev]);
                    break;
                }
            }
            if (i == numKatDevices)
            {
                printk("Unexpected device connected, disconnect!");
                bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
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

static void auto_exchange_complete(struct bt_conn *conn, struct bt_conn_remote_info *remote)
{
    // Delay send connection change till end of other communication,
    // as any other communication may break update stream.
    struct bt_le_conn_param param = {
        .interval_min = 8,
        .interval_max = 9,
        .latency = 10,  // 100,
        .timeout = 500, // 600,
    };
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
    if (devices[dev].deviceStatus.freshStatus)
    {
        devices[dev].deviceStatus.freshStatus = false;
        printk("%s: charge_level=%d, charge_status=%d, firmware=%d\n",
               stKatDevice[connections[id].deviceType],
               devices[dev].deviceStatus.chargeLevel,
               devices[dev].deviceStatus.chargeStatus,
               devices[dev].deviceStatus.firmwareVersion);
    }
    return 0;
}

static struct bt_l2cap_le_chan my_att_chan_pool[CONFIG_BT_MAX_CONN];
static int my_att_accept(struct bt_conn *conn, struct bt_l2cap_chan **ch)
{
    static const struct bt_l2cap_chan_ops ops = {
        .recv = my_att_recv,
    };

    int id = bt_conn_index(conn);
    printk("Capturing L2CAP ATT channel on connection %p/%d\n", conn, id);

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
