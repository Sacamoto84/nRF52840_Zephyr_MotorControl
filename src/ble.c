#include "define.h"

void connected(struct bt_conn *conn, uint8_t err)
{
    if (err)
    {
        printk("BLE Connection failed: %u\n", err);
    }
    else
    {
        printk("BLE Connected\n");
    }
}

void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("BLE Disconnected (reason: %u)\n", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

void ble_start_adv()
{
    int err;
    // Advertising
    struct bt_le_adv_param adv_param = BT_LE_ADV_PARAM_INIT(
        BT_LE_ADV_OPT_CONNECTABLE,
        BT_GAP_ADV_SLOW_INT_MIN,
        BT_GAP_ADV_SLOW_INT_MAX,
        NULL);

    err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err)
    {
        printk("Advertising failed: %d\n", err);
        return -1;
    }
}

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
    if (len != 1)
    {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    uint8_t new_duty = *((uint8_t *)buf);
    if (new_duty > 100)
    {
        return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
    }

    motor_state.duty_cycle = new_duty;
    printk("BLE: Set duty to %d%%\n", motor_state.duty_cycle);

    if (motor_state.motor_on)
    {
        motor_set_pwm(motor_state.duty_cycle);
    }

    nvs_save_settings();
    return len;
}

static ssize_t read_motor_state(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                void *buf, uint16_t len, uint16_t offset)
{
    uint8_t state = motor_state.motor_on ? 1 : 0;
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &state, sizeof(state));
}

static ssize_t write_motor_state(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len, uint16_t offset,
                                 uint8_t flags)
{
    if (len != 1)
    {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    uint8_t new_state = *((uint8_t *)buf);
    motor_state.motor_on = (new_state != 0);

    printk("BLE: Motor %s\n", motor_state.motor_on ? "ON" : "OFF");
    motor_set_pwm(motor_state.motor_on ? motor_state.duty_cycle : 0);
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
                                              read_motor_state, write_motor_state, NULL), );
