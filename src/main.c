#include "define.h"
#include <zephyr/logging/log.h>

//"NRF52832_XXAA"
// JLinkGDBServer -device NRF52832_XXAA -if SWD -speed 6000 -autoconnect 1 -nogui

// extern uButton b;

extern void button_isr();

extern void buttonLoop();

int main(void)
{
    printk(CLRscr); // очистить экран

    printk(BOLD FG(226) "╔══════════════════════════════════════════╗\n" RESET);
    printk(BOLD FG(226) "║  ZEPHYR + RTT 256 цвет + русский язык    ║\n" RESET);
    printk(BOLD FG(226) "╚══════════════════════════════════════════╝\n" RESET);

    // for (int i = 0; i < 10; i++) {
    //     int color = i * 25;
    //     printk(FG(%d) "  Цвет %3d →  Привет из %s!  " RESET
    //            BG(%d) "  фон %3d  " RESET "\n",
    //            color, color, CONFIG_BOARD, color + 5, color + 5);
    // }

    printk("\n" BOLD UNDERLINE FG(196) "Критическая ошибка!" RESET "  " FG(82) "Успешно!" RESET "  " FG(226) "Предупреждение\n" RESET);

    int err;

    /* REGOUT0 настройка */
    if (NRF_UICR->REGOUT0 != 5)
    {
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
            ;

        NRF_UICR->REGOUT0 = 5;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
            ;

        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
            ;

        NVIC_SystemReset();
    }

    printk("\n=== Motor Controller Starting ===\n");

    // // Настройка выходов P0.10 и P0.29
    // nrf_gpio_cfg_output(10);
    // nrf_gpio_pin_set(10);
    // printk("P0.10 set HIGH\n");

    // nrf_gpio_cfg_output(29);
    // nrf_gpio_pin_set(29);
    // printk("P0.29 set HIGH\n");

    // Инициализация NVS
    err = nvs_init_storage();
    if (err)
    {
        printk("NVS init failed, using defaults\n");
    }
    else
    {
        nvs_load_settings();
    }

    // Инициализация PWM
    pwm_dev = DEVICE_DT_GET(PWM_NODE);
    if (!device_is_ready(pwm_dev))
    {
        printk("PWM device not ready\n");
        return -1;
    }

    // // Выключить PWM изначально
    motor_set_pwm(0);

    // Инициализация кнопки
    if (!gpio_is_ready_dt(&button))
    {
        printk("Button GPIO not ready\n");
        return -1;
    }

    err = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (err)
    {
        printk(BOLD UNDERLINE FG(196) "Не удалось настроить кнопку\n" RESET);
        return -1;
    }

    err = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
    if (err)
    {
        printk(BOLD UNDERLINE FG(196) "Не удалось настроить прерывание кнопки\n" RESET);
        return -1;
    }

    gpio_init_callback(&button_cb_data, button_isr, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    // Инициализация delayed works
    //    k_work_init_delayable(&button_state.double_click_timeout, double_click_timeout_work);
    //    k_work_init_delayable(&button_state.long_press_check, long_press_check_work);

    printk("Button configured\n");

    // Инициализация Bluetooth
    err = bt_enable(NULL);
    if (err)
    {
        printk("Bluetooth init failed: %d\n", err);
        return -1;
    }

    printk("Bluetooth initialized\n");

    ble_start_adv();

    printk("Advertising started\n");

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
    while (1)
    {

        buttonLoop();
       
        // printk("\r" FG(51) "► Uptime: %6u сек" RESET, k_uptime_get_32() / 1000);

        k_sleep(K_MSEC(20));

        //k_sleep(K_FOREVER);

        // k_sleep(K_SECONDS(1));
    }

    return 0;
}