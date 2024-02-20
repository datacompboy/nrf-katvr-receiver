#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/l2cap.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/sys/printk.h>

// Ensure the console is USB-UART
BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
             "Console device is not ACM CDC UART device");

typedef enum { KAT_NONE=0, KAT_DIR, KAT_LEFT, KAT_RIGHT } tKatDevice;
const char * const stKatDevice[] = { "KAT_NONE", "KAT_DIR", "KAT_LEFT", "KAT_RIGHT", "KAT_BUG" };

typedef struct {
        tKatDevice deviceType;
} katConnectionInfo;

static katConnectionInfo connections[CONFIG_BT_MAX_CONN] = {KAT_NONE};

const bt_addr_le_t katDevices[] = {
        // KAT_DIR
        {.type=BT_ADDR_LE_PUBLIC, .a={.val={0x01, 0x74, 0xEB, 0x16, 0x4D, 0xAC}}}, // AC:4D:16:EB:74:01
        // KAT_LEFT
        {.type=BT_ADDR_LE_PUBLIC, .a={.val={0x54, 0x46, 0xF2, 0x66, 0x98, 0x60}}}, // 60:98:66:F2:46:54
        // KAT_RIGHT
        {.type=BT_ADDR_LE_PUBLIC, .a={.val={0x6A, 0x1C, 0xF2, 0x66, 0x98, 0x60}}}, // 60:98:66:F2:1C:6A
};
const int numKatDevices = ARRAY_SIZE(katDevices);


void resume_connections() {
        int err = bt_conn_le_create_auto(BT_CONN_LE_CREATE_CONN_AUTO, BT_LE_CONN_PARAM_DEFAULT);
        if (err) {
                printk("bt_conn_le_create_auto error (%d / 0x%02d)", err, err);
        } else {
                printk("Fishing for known device in advertising stream...\n");
        }

}

static void device_connected(struct bt_conn *conn, uint8_t conn_err)
{
	if (!conn_err) {
		int err;

                int conn_num = bt_conn_index(conn);
	        printk("Device connected (%p/%d)\n", conn, conn_num);
                if (connections[conn_num].deviceType == KAT_NONE) {
                        struct bt_le_conn_param param = {
                                .interval_min = 8,
                                .interval_max = 9,
                                .latency = 0, // 100,
                                .timeout = 500, // 600,
                        };
                        err = bt_conn_le_param_update(conn, &param);
                        if (!err) {
                                printk("Sent param update.\n");
                        } else {
                                printk("Error on bt_conn_le_param_update (%d/0x%02x)\n", err, err);
                        }

                        int i;
                        for (i=0; i<numKatDevices; ++i) {
                                struct bt_conn_info info;
                                bt_conn_get_info(conn, &info);
                                if (bt_addr_le_eq(info.le.dst, &katDevices[i])) {
                                        connections[conn_num].deviceType = (tKatDevice)i+1; // Shift by 1 from katDevices array
                                        printk("Connected device of type %s\n", stKatDevice[connections[conn_num].deviceType]);
                                        break;
                                }
                        }
                        if (i==numKatDevices) {
                                printk("Unexpected device connected, disconnect!");
                                bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
                        }
                }
	} else {
	        printk("Connect error (%p/%x)\n", conn, conn_err);
                bt_conn_unref(conn);
        }
        resume_connections();
}

static void device_disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Device disconnected (%p/0x%02x)\n", conn, reason);
        int conn_num = bt_conn_index(conn);
        connections[conn_num].deviceType = KAT_NONE;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = device_connected,
	.disconnected = device_disconnected,
};

/*
  Hack to get ATT stream before ATT/GATT driver.
*/
static int my_att_recv(struct bt_l2cap_chan *chan, struct net_buf *req_buf)
{
        int id = bt_conn_index(chan->conn);
        #if true
        printk("Received packet for %p %s [%d]", chan->conn, stKatDevice[connections[id].deviceType], req_buf->len);
        for (int i = 0; i<req_buf->len; ++i) {
                printk(" %02x", req_buf->data[i]);
        }
        printk("\n");
        #endif
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
struct bt_l2cap_fixed_chan {
	uint16_t		cid;
	int (*accept)(struct bt_conn *conn, struct bt_l2cap_chan **chan);
	bt_l2cap_chan_destroy_t destroy;
};
#define BT_L2CAP_CHANNEL_DEFINE(_name, _cid, _accept, _destroy)         \
	const STRUCT_SECTION_ITERABLE(bt_l2cap_fixed_chan, _name) = {   \
				.cid = _cid,                            \
				.accept = _accept,                      \
				.destroy = _destroy,                    \
			}
#define BT_L2CAP_CID_ATT                0x0004
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
        for (int i = 0; i < numKatDevices; ++i) {
                printk("Adding to accept list device %s...\n", stKatDevice[i+1]);
                err = bt_le_filter_accept_list_add(&katDevices[i]);
                if (err) {
                        printk("bt_le_filter_accept_list_add error (%d / 0x%02d)", err, err);
                }
        }

        resume_connections();

        return 0;
}
