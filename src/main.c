#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>
#include <zephyr/types.h>
#include <string.h>
#include <hal/nrf_power.h>
#include <hal/nrf_gpio.h>

// ==================== Определения ====================
#define PWM_NODE DT_NODELABEL(pwm0)
#define PWM_CHANNEL 0
#define PWM_PERIOD_NS 20000000 // 50 Hz

#define BUTTON_NODE DT_ALIAS(sw0)

#define DOUBLE_CLICK_TIMEOUT_MS 500
#define LONG_PRESS_TIMEOUT_MS 2000
#define DEBOUNCE_TIME_MS 50

// NVS настройки
#define NVS_PARTITION storage_partition
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(NVS_PARTITION)
#define NVS_ID_DUTY_CYCLE 1
#define NVS_ID_MOTOR_STATE 2

// ==================== Глобальные переменные ====================
static const struct device *pwm_dev;
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static struct gpio_callback button_cb_data;

static struct nvs_fs nvs;

static struct {
    uint8_t duty_cycle;
    bool motor_on;
    bool pwm_active;
} motor_state = {
    .duty_cycle = 50,
    .motor_on = false,
    .pwm_active = false
};

// Состояние кнопки
static struct {
    int64_t press_start_time;
    int64_t last_press_time;
    bool is_pressed;
    bool waiting_for_second_click;
    bool long_press_triggered;
    struct k_work_delayable double_click_timeout;
    struct k_work_delayable long_press_check;
} button_state = {0};

// ==================== NVS функции ====================
static int nvs_init_storage(void)
{
    int err;
    struct flash_pages_info info;

    nvs.flash_device = NVS_PARTITION_DEVICE;
    if (!device_is_ready(nvs.flash_device)) {
        printk("Flash device not ready\n");
        return -ENODEV;
    }

    nvs.offset = NVS_PARTITION_OFFSET;
    err = flash_get_page_info_by_offs(nvs.flash_device, nvs.offset, &info);
    if (err) {
        printk("Unable to get flash page info\n");
        return err;
    }

    nvs.sector_size = info.size;
    nvs.sector_count = 3;

    err = nvs_mount(&nvs);
    if (err) {
        printk("NVS mount failed: %d\n", err);
        return err;
    }

    printk("NVS mounted successfully\n");
    return 0;
}

static void nvs_save_settings(void)
{
    int err;

    err = nvs_write(&nvs, NVS_ID_DUTY_CYCLE, &motor_state.duty_cycle, 
                    sizeof(motor_state.duty_cycle));
    if (err < 0) {
        printk("Failed to write duty cycle to NVS: %d\n", err);
    }

    err = nvs_write(&nvs, NVS_ID_MOTOR_STATE, &motor_state.motor_on,
                    sizeof(motor_state.motor_on));
    if (err < 0) {
        printk("Failed to write motor state to NVS: %d\n", err);
    }

    printk("Settings saved: duty=%d%%, motor=%s\n",
           motor_state.duty_cycle, motor_state.motor_on ? "ON" : "OFF");
}

static void nvs_load_settings(void)
{
    int err;

    err = nvs_read(&nvs, NVS_ID_DUTY_CYCLE, &motor_state.duty_cycle,
                   sizeof(motor_state.duty_cycle));
    if (err > 0) {
        printk("Loaded duty cycle: %d%%\n", motor_state.duty_cycle);
    } else {
        printk("Using default duty: %d%%\n", motor_state.duty_cycle);
    }

    err = nvs_read(&nvs, NVS_ID_MOTOR_STATE, &motor_state.motor_on,
                   sizeof(motor_state.motor_on));
    if (err > 0) {
        printk("Loaded motor state: %s\n", motor_state.motor_on ? "ON" : "OFF");
    } else {
        motor_state.motor_on = false;
    }
}

// ==================== PWM управление ====================
static void motor_set_pwm(uint8_t duty)
{
    if (duty > 100) duty = 100;

    if (duty == 0 || !motor_state.motor_on) {
        pwm_set(pwm_dev, PWM_CHANNEL, 0, 0, 0);
        if (motor_state.pwm_active) {
            pm_device_action_run(pwm_dev, PM_DEVICE_ACTION_SUSPEND);
            motor_state.pwm_active = false;
            printk("PWM suspended\n");
        }
    } else {
        if (!motor_state.pwm_active) {
            pm_device_action_run(pwm_dev, PM_DEVICE_ACTION_RESUME);
            motor_state.pwm_active = true;
            printk("PWM resumed\n");
        }

        uint32_t pulse_ns = (uint64_t)PWM_PERIOD_NS * duty / 100;
        pwm_set(pwm_dev, PWM_CHANNEL, PWM_PERIOD_NS, pulse_ns, 0);
        printk("Motor PWM: %d%% (%u ns)\n", duty, pulse_ns);
    }
}

static void motor_toggle(void)
{
    motor_state.motor_on = !motor_state.motor_on;
    printk("Motor %s at %d%%\n",
           motor_state.motor_on ? "ON" : "OFF",
           motor_state.duty_cycle);

    motor_set_pwm(motor_state.motor_on ? motor_state.duty_cycle : 0);
    nvs_save_settings();
}

// ==================== Обработка кнопки ====================
static void single_click_handler(void)
{
    printk("Single click: Toggle motor\n");
    motor_toggle();
}

static void double_click_handler(void)
{
    printk("Double click: Reset duty to 50%%\n");
    motor_state.duty_cycle = 50;
    if (motor_state.motor_on) {
        motor_set_pwm(motor_state.duty_cycle);
    }
    nvs_save_settings();
}

static void long_press_handler(void)
{
    printk("Long press: Motor test\n");

    if (motor_state.motor_on) {
        for (int i = 0; i <= 100; i += 10) {
            motor_set_pwm(i);
            k_sleep(K_MSEC(100));
        }
        for (int i = 100; i >= 0; i -= 10) {
            motor_set_pwm(i);
            k_sleep(K_MSEC(100));
        }
        motor_set_pwm(motor_state.duty_cycle);
    }
}

// Work handlers
static void double_click_timeout_work(struct k_work *work)
{
    button_state.waiting_for_second_click = false;
    single_click_handler();
}

static void long_press_check_work(struct k_work *work)
{
    if (button_state.is_pressed && !button_state.long_press_triggered) {
        button_state.long_press_triggered = true;
        long_press_handler();
    }
}

// GPIO ISR
static void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    int64_t now = k_uptime_get();
    int button_value = gpio_pin_get_dt(&button);

    if (button_value == 0) {
        // Кнопка нажата
        int64_t time_since_last = now - button_state.last_press_time;

        if (time_since_last < DEBOUNCE_TIME_MS) {
            return; // Debounce
        }

        button_state.is_pressed = true;
        button_state.press_start_time = now;
        button_state.long_press_triggered = false;

        k_work_reschedule(&button_state.long_press_check,
                          K_MSEC(LONG_PRESS_TIMEOUT_MS));

        if (button_state.waiting_for_second_click) {
            k_work_cancel_delayable(&button_state.double_click_timeout);
            button_state.waiting_for_second_click = false;
            double_click_handler();
        }
    } else {
        // Кнопка отпущена
        button_state.is_pressed = false;
        int64_t press_duration = now - button_state.press_start_time;

        k_work_cancel_delayable(&button_state.long_press_check);

        if (!button_state.long_press_triggered && press_duration < LONG_PRESS_TIMEOUT_MS) {
            button_state.waiting_for_second_click = true;
            k_work_reschedule(&button_state.double_click_timeout,
                              K_MSEC(DOUBLE_CLICK_TIMEOUT_MS));
        }

        button_state.last_press_time = now;
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
    if (len != 1) {
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
                          read_motor_state, write_motor_state, NULL),
);

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

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

int main(void)
{
    int err;

    printk("\n=== Motor Controller Starting ===\n");

    // // Настройка выходов P0.10 и P0.29
    // nrf_gpio_cfg_output(10);
    // nrf_gpio_pin_set(10);
    // printk("P0.10 set HIGH\n");

    // nrf_gpio_cfg_output(29);
    // nrf_gpio_pin_set(29);
    // printk("P0.29 set HIGH\n");

    // // Инициализация NVS
    // err = nvs_init_storage();
    // if (err) {
    //     printk("NVS init failed, using defaults\n");
    // } else {
    //     nvs_load_settings();
    // }

    // // Инициализация PWM
    // pwm_dev = DEVICE_DT_GET(PWM_NODE);
    // if (!device_is_ready(pwm_dev)) {
    //     printk("PWM device not ready\n");
    //     return -1;
    // }

    // // Выключить PWM изначально
    // motor_set_pwm(0);

    // // Инициализация кнопки
    // if (!gpio_is_ready_dt(&button)) {
    //     printk("Button GPIO not ready\n");
    //     return -1;
    // }

    // err = gpio_pin_configure_dt(&button, GPIO_INPUT);
    // if (err) {
    //     printk("Failed to configure button\n");
    //     return -1;
    // }

    // err = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
    // if (err) {
    //     printk("Failed to configure button interrupt\n");
    //     return -1;
    // }

    // gpio_init_callback(&button_cb_data, button_isr, BIT(button.pin));
    // gpio_add_callback(button.port, &button_cb_data);

    // // Инициализация delayed works
    // k_work_init_delayable(&button_state.double_click_timeout, double_click_timeout_work);
    // k_work_init_delayable(&button_state.long_press_check, long_press_check_work);

    // printk("Button configured\n");

    // Инициализация Bluetooth
    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed: %d\n", err);
        return -1;
    }

    printk("Bluetooth initialized\n");

    // Advertising
    struct bt_le_adv_param adv_param = BT_LE_ADV_PARAM_INIT(
        BT_LE_ADV_OPT_CONNECTABLE,
        BT_GAP_ADV_SLOW_INT_MIN,
        BT_GAP_ADV_SLOW_INT_MAX,
        NULL);

    err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed: %d\n", err);
        return -1;
    }

    // printk("Advertising started\n");
    // printk("=== System Ready ===\n");
    // printk("Motor: %s, Duty: %d%%\n",
    //        motor_state.motor_on ? "ON" : "OFF",
    //        motor_state.duty_cycle);

    // // КРИТИЧНО: Разрешить переход в глубокий сон
    // // Без этого система не сможет перейти в System OFF
    // pm_policy_state_lock_put(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
    // pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
    
    printk("Deep sleep enabled\n");

    // Main loop - просто спим, всё управляется прерываниями
    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}