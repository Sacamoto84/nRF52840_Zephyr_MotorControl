#ifndef ZEPHYR_INCLUDE_STORAGE_FLASH_MAP_H_
#define ZEPHYR_INCLUDE_STORAGE_FLASH_MAP_H_

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

#ifdef __cplusplus
extern "C" {
#endif

// Объявление типа структуры
typedef struct {
    uint8_t duty_cycle;
    bool motor_on;
    bool pwm_active;
} motor_state_t;

extern motor_state_t motor_state;


//storage.c
extern int nvs_init_storage(void);
extern void nvs_load_settings(void);
extern void nvs_save_settings(void);

//pwm.c
extern void motor_set_pwm(uint8_t duty);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_STORAGE_FLASH_MAP_H_ */