#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

// LED configuration
#define LED0_NODE DT_ALIAS(led0)

#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#else
#define LED0_NODE DT_NODELABEL(gpio0)
static const struct gpio_dt_spec led = {
    .port = DEVICE_DT_GET(LED0_NODE),
    .pin = 15,
    .dt_flags = GPIO_ACTIVE_LOW
};
#endif

// Connection state
static struct bt_conn *current_conn = NULL;
static bool is_connected = false;

// Custom GATT Service UUID (16-bit для экономии места в advertising)
// Можно использовать любой UUID от 0x1800 до 0xFEFF
#define BT_UUID_CUSTOM_SERVICE  BT_UUID_DECLARE_16(0xABCD)
#define BT_UUID_CUSTOM_CHAR     BT_UUID_DECLARE_16(0xABCE)

// Данные характеристики
static uint8_t char_value = 0;

// Callback для чтения характеристики
static ssize_t read_char(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         void *buf, uint16_t len, uint16_t offset)
{
    const uint8_t *value = attr->user_data;
    
    printk("Read char: %d\n", *value);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(char_value));
}

// Callback для записи характеристики
static ssize_t write_char(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          const void *buf, uint16_t len, uint16_t offset,
                          uint8_t flags)
{
    uint8_t *value = attr->user_data;
    
    if (offset + len > sizeof(char_value)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    
    memcpy(value + offset, buf, len);
    
    printk("Write char: %d\n", *value);
    
    // Обработка команды
    if (*value == 1) {
        printk("Command: LED ON\n");
        gpio_pin_set_dt(&led, 1);
    } else if (*value == 0) {
        printk("Command: LED OFF\n");
        gpio_pin_set_dt(&led, 0);
    }
    
    return len;
}

// GATT Service Definition
BT_GATT_SERVICE_DEFINE(custom_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_CUSTOM_SERVICE),
    BT_GATT_CHARACTERISTIC(BT_UUID_CUSTOM_CHAR,
                          BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                          BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                          read_char, write_char, &char_value),
);

// Advertising data (минимальный набор)
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

// Убираем scan response, всё включаем в advertising
static const struct bt_data sd[] = {
};

// Connection callback
static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    
    if (err) {
        printk("Connection failed (err %u)\n", err);
        return;
    }
    
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printk("Connected: %s\n", addr);
    
    current_conn = bt_conn_ref(conn);
    is_connected = true;
    
    // Включаем светодиод при подключении
    gpio_pin_set_dt(&led, 1);
}

// Disconnection callback
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];
    
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printk("Disconnected: %s (reason %u)\n", addr, reason);
    
    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
    
    is_connected = false;
    
    // Выключаем светодиод при отключении
    gpio_pin_set_dt(&led, 0);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

int main(void)
{
    int err;
    
    printk("Starting BLE GATT Server\n");
    
    // Инициализация GPIO
    if (!device_is_ready(led.port)) {
        printk("LED device not ready\n");
        return 0;
    }
    
    err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        printk("Error configuring LED pin: %d\n", err);
        return 0;
    }
    
    // Явно выключаем LED при старте
    gpio_pin_set_dt(&led, 0);
    
    printk("LED configured and OFF\n");
    
    // Инициализация Bluetooth
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }
    
    printk("Bluetooth initialized\n");
    
    // Запуск advertising с connectable параметрами
    struct bt_le_adv_param adv_param = {
        .id = BT_ID_DEFAULT,
        .options = BT_LE_ADV_OPT_CONNECTABLE,
        .interval_min = BT_GAP_ADV_SLOW_INT_MIN,
        .interval_max = BT_GAP_ADV_SLOW_INT_MAX,
    };
    
    err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return 0;
    }

    printk("Advertising started\n");
    printk("Device name: %s\n", DEVICE_NAME);
    printk("Waiting for connection...\n");
    
    // Главный цикл - переход в режим ожидания
    while (1) {
        // Система автоматически переходит в режим сна
        // когда нет активности
        k_sleep(K_FOREVER);
    }
    
    return 0;
}
