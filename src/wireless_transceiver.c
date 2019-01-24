// mongoose-os components
#include "mgos.h"
#include "mgos_uart.h"
#include "mgos_bt_gatts.h"
#include "mgos_bt_gattc.h"

// esp-idf components
#include "esp_bt.h"

#define NUM_CELLS 80

enum transceiver_modes {
    MODE_TX = 0,
    MODE_RX = 1
};

typedef struct {
    uint8_t throttle;                       // throttle [0, 100]
    float cell_voltages[NUM_CELLS];         // voltages of each cell
    float cell_temperatures[NUM_CELLS];     // temperatures of each cell (deg C)
    float current;                          // battery current
    float speed;                            // vehicle speed (mph)
    uint8_t checksum;                       // used when converting to buffer to send over BT
    // TODO(cullen): Add more fields as necessary
} telemetry_data_t;

telemetry_data_t telemetry_data;

int comm_uart_no;

static void uart_dispatcher(int uart_no, void *args) {
    // return if the uart number is not correct
    if (uart_no != comm_uart_no) return;

    // get the number of available bytes
    size_t rx_av = mgos_uart_read_avail(uart_no);

    // if there are bytes in the receive buffer, process them
    if (rx_av > 0) {
        // TODO(cullen): Add some code here
    }
}

static enum mgos_bt_gatt_status bt_service_ev(struct mgos_bt_gatts_conn *c, enum mgos_bt_gatts_ev ev,
                                                  void *ev_arg, void *handler_arg) {
    enum mgos_bt_gatt_status ret = MGOS_BT_GATT_STATUS_REQUEST_NOT_SUPPORTED;
    switch (ev) {
        case MGOS_BT_GATTS_EV_CONNECT: {
            ret = MGOS_BT_GATT_STATUS_OK;
            break;
        }
        case MGOS_BT_GATTS_EV_DISCONNECT: {
            break;
        }
        default:
            break;
    }
    return ret;
}

static enum mgos_bt_gatt_status get_telemetry_data(struct mgos_bt_gatts_conn *c, enum mgos_bt_gatts_ev ev,
                                                   void *ev_arg, void *handler_arg) {

    if (ev != MGOS_BT_GATTS_EV_READ) return MGOS_BT_GATT_STATUS_REQUEST_NOT_SUPPORTED;

    struct mgos_bt_gatts_read_arg *ra = (struct mgos_bt_gatts_read_arg *) ev_arg;

    // convert the telemetry data struct into a char buffer
    char *buf = (char *)&telemetry_data;
    size_t buf_size = sizeof(telemetry_data_t);

    // generate the checksum
    uint8_t checksum = 0;
    for (int i = 0; i < buf_size - 1; i++) {
        checksum += buf[i];
    }

    // add the checksum to the buffer
    buf[buf_size - 1] = checksum;

    // Send a response with the telemetry data
    mgos_bt_gatts_send_resp_data(c, ra, mg_mk_str_n(buf, buf_size));

    return MGOS_BT_GATT_STATUS_OK;
}

static const struct mgos_bt_gatts_char_def transceiver_bt_service[] = {
    // only one characteristic for this service: get telemetry data
    {
     .uuid = "b721abf5-8885-4608-a701-7ca3da19653c",
     .prop = MGOS_BT_GATT_PROP_RWNI(1, 0, 0, 0),  // read only service
     .handler = get_telemetry_data
    },
    {.uuid = NULL}
};

enum mgos_app_init_result mgos_app_init(void) {
    // pull values from config
    comm_uart_no = mgos_sys_config_get_transceiver_uart_no();

    // get the default UART config struct
    struct mgos_uart_config cfg;
    mgos_uart_config_set_defaults(comm_uart_no, &cfg);

    // set the baud rate and buffer size params
    cfg.baud_rate = 115200;
    cfg.rx_buf_size = 64;
    cfg.tx_buf_size = 64;

    // if the uart fails to configure properly, error out of the app.
    if (!mgos_uart_configure(comm_uart_no, &cfg)) {
        LOG(LL_ERROR, ("Could not configure UART"));
        return MGOS_APP_INIT_ERROR;
    }

    // increase the bluetooth transmission power to the maximum (+9dBm)
    if (esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9) != ESP_OK) {
        LOG(LL_ERROR, ("Could not increase BT Tx power"));
        return MGOS_APP_INIT_ERROR;
    }

    if (mgos_sys_config_get_transceiver_mode() == MODE_RX) {
        // set the UART dispatcher and enable RX
        mgos_uart_set_dispatcher(comm_uart_no, uart_dispatcher, NULL);
        mgos_uart_set_rx_enabled(comm_uart_no, true);

        mgos_bt_gatts_register_service("409bf102-81e0-4561-9c17-bece746bfb87",
        (enum mgos_bt_gatt_sec_level) 0, transceiver_bt_service, bt_service_ev, NULL);

    } else {
        // TODO(cullen): Setup the GATT client here
    }

    return MGOS_APP_INIT_SUCCESS;
}
