#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/types.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/types.h>
#include <string.h>

#define PWM_NODE DT_NODELABEL(pwm0)
#define PWM_CHANNEL 0
#define PWM_PERIOD_NS 1000000   // 1 kHz

static const struct device *pwm_dev;
static uint8_t char_value = 0; // Duty cycle 0-100%

// --- PWM helper ---
static void pwm_set_duty(uint8_t duty)
{
    if (duty > 100) duty = 100;
    uint32_t pulse_ns = (uint64_t)PWM_PERIOD_NS * duty / 100;
    pwm_set(pwm_dev, PWM_CHANNEL, PWM_PERIOD_NS, pulse_ns, 0);
}

// --- GATT callbacks ---
static ssize_t read_pwm(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                        void *buf, uint16_t len, uint16_t offset)
{
    const uint8_t *value = attr->user_data;
    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(char_value));
}

static ssize_t write_pwm(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         const void *buf, uint16_t len, uint16_t offset,
                         uint8_t flags)
{
    uint8_t *value = attr->user_data;

    if (len != 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    *value = *((uint8_t*)buf);
    printk("Set PWM duty: %d%%\n", *value);

    pwm_set_duty(*value);

    return len;
}

// --- GATT service ---
BT_GATT_SERVICE_DEFINE(pwm_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(0xABCD)),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0xABCE),
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                           read_pwm, write_pwm, &char_value),
);

// --- BLE callbacks ---
static void connected(struct bt_conn *conn, uint8_t err) { 
    printk("Connected\n");
}

static void disconnected(struct bt_conn *conn, uint8_t reason) { 
    printk("Disconnected\n");
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

// Advertising data - must be static or global
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME)-1),
};

// --- Main ---
int main(void)
{
    int err;

    printk("Starting BLE PWM example\n");

    pwm_dev = DEVICE_DT_GET(PWM_NODE);
    if (!device_is_ready(pwm_dev)) {
        printk("PWM device not ready\n");
        return -1;
    }

    // LED initially off
    pwm_set_duty(0);

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return -1;
    }

    printk("Bluetooth initialized\n");

    // Запуск advertising с connectable параметрами
    struct bt_le_adv_param adv_param = {
        .id = BT_ID_DEFAULT,
        .options = BT_LE_ADV_OPT_CONNECTABLE,
        .interval_min = BT_GAP_ADV_SLOW_INT_MIN,
        .interval_max = BT_GAP_ADV_SLOW_INT_MAX,
    };


    // Advertising - use new API
    err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return -1;
	}

    printk("Advertising started\n");

    // Main loop
    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}