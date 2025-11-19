#include "define.h"

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("BLE Connection failed: %u\n", err);
    } else {
        printk("BLE Connected\n");
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("BLE Disconnected (reason: %u)\n", reason);
}

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

// ==================== BLE GATT ====================
static ssize_t read_duty_cycle(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             &motor_state.duty_cycle, sizeof(motor_state.duty_cycle));
}

static ssize_t write_duty_cycle(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                const void *buf, uint16_t len, uint16_t offset,
                                uint8_t flags)
{
    if (len != 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    uint8_t new_duty = *((uint8_t *)buf);
    if (new_duty > 100) {
        return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
    }

    motor_state.duty_cycle = new_duty;
    printk("BLE: Set duty to %d%%\n", motor_state.duty_cycle);

    if (motor_state.motor_on) {
        motor_set_pwm(motor_state.duty_cycle);
    }

    nvs_save_settings();
    return len;
}

BT_GATT_SERVICE_DEFINE(motor_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(0xABCD)),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0xABCE),
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                          read_duty_cycle, write_duty_cycle, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0xABCF),
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                          read_motor_state, write_motor_state, NULL),
);

