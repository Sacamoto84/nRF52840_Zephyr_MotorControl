#ifndef DEFINE_H_
#define DEFINE_H_

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

#include <stdint.h>
#include <stdbool.h>

#include "uButton.h"

// Удобные макросы (можно положить в отдельный .h)
#define CLRscr "\033[2J\033[H"
#define FG(color) "\033[38;5;" #color "m"
#define BG(color) "\033[48;5;" #color "m"
#define RESET "\033[0m"
#define BOLD "\033[1m"
#define UNDERLINE "\033[4m"

#define PWM_NODE DT_NODELABEL(pwm0)

// Состояние кнопки
typedef struct {
    int64_t press_start_time;
    int64_t last_press_time;
    bool is_pressed;
    bool waiting_for_second_click;
    bool long_press_triggered;
    struct k_work_delayable double_click_timeout;
    struct k_work_delayable long_press_check;
} button_state_t;

extern button_state_t button_state;


//storage.c
extern int nvs_init_storage(void);
extern void nvs_load_settings(void);
extern void nvs_save_settings(void);

//pwm.c
extern const struct device *pwm_dev;
extern void motor_set_pwm(uint8_t duty);
extern void motor_toggle(void);


//button.c 
extern const struct gpio_dt_spec button;
extern  struct gpio_callback button_cb_data;
extern void double_click_handler(void);
extern void single_click_handler(void);
extern void long_press_check_work(struct k_work *work);
//extern void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
extern void double_click_timeout_work(struct k_work *work);


//ble.c
extern void ble_start_adv(void);


//global.c
extern uint8_t global_duty_cycle;
extern bool global_motor_on;
extern bool global_pwm_active;

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_STORAGE_FLASH_MAP_H_ */