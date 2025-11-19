#include "define.h"

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







int main(void)
{
    int err;

   /* REGOUT0 настройка */
    if (NRF_UICR->REGOUT0 != 5) {
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
        
        NRF_UICR->REGOUT0 = 5;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
        
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
        
        NVIC_SystemReset();
    }

    printk("\n=== Motor Controller Starting ===\n");

    // Настройка выходов P0.10 и P0.29
    nrf_gpio_cfg_output(10);
    nrf_gpio_pin_set(10);
    printk("P0.10 set HIGH\n");

    nrf_gpio_cfg_output(29);
    nrf_gpio_pin_set(29);
    printk("P0.29 set HIGH\n");

    // Инициализация NVS
    err = nvs_init_storage();
    if (err) {
        printk("NVS init failed, using defaults\n");
    } else {
        nvs_load_settings();
    }

    // Инициализация PWM
    pwm_dev = DEVICE_DT_GET(PWM_NODE);
    if (!device_is_ready(pwm_dev)) {
        printk("PWM device not ready\n");
        return -1;
    }

    // // Выключить PWM изначально
    motor_set_pwm(0);

    // Инициализация кнопки
    if (!gpio_is_ready_dt(&button)) {
        printk("Button GPIO not ready\n");
        return -1;
    }

    err = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (err) {
        printk("Failed to configure button\n");
        return -1;
    }

    err = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
    if (err) {
        printk("Failed to configure button interrupt\n");
        return -1;
    }

    gpio_init_callback(&button_cb_data, button_isr, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    // Инициализация delayed works
    k_work_init_delayable(&button_state.double_click_timeout, double_click_timeout_work);
    k_work_init_delayable(&button_state.long_press_check, long_press_check_work);

    printk("Button configured\n");

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