
#include "define.h"

#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>

// NVS настройки
#define NVS_PARTITION storage
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(NVS_PARTITION)
#define NVS_ID_DUTY_CYCLE 1
#define NVS_ID_MOTOR_STATE 2

static struct nvs_fs nvs;

// ==================== NVS функции ====================
int nvs_init_storage(void)
{
    int err;
    struct flash_pages_info info;

    nvs.flash_device = NVS_PARTITION_DEVICE;
    if (!device_is_ready(nvs.flash_device))
    {
        printk("Flash device not ready\n");
        return -ENODEV;
    }

    nvs.offset = NVS_PARTITION_OFFSET;
    err = flash_get_page_info_by_offs(nvs.flash_device, nvs.offset, &info);
    if (err)
    {
        printk("Unable to get flash page info\n");
        return err;
    }

    nvs.sector_size = info.size;
    nvs.sector_count = 3;

    err = nvs_mount(&nvs);
    if (err)
    {
        printk("NVS mount failed: %d\n", err);
        return err;
    }

    printk("NVS mounted successfully\n");
    return 0;
}

void nvs_save_settings(void)
{
    int err;

    err = nvs_write(&nvs, NVS_ID_DUTY_CYCLE, &motor_state.duty_cycle, sizeof(motor_state.duty_cycle));
    if (err < 0) printk("Failed to write duty cycle to NVS: %d\n", err);

    err = nvs_write(&nvs, NVS_ID_MOTOR_STATE, &motor_state.motor_on, sizeof(motor_state.motor_on));
    if (err < 0) printk("Failed to write motor state to NVS: %d\n", err);
    
    printk("Settings saved: duty=%d%%, motor=%s\n", motor_state.duty_cycle, motor_state.motor_on ? "ON" : "OFF");
}

void nvs_load_settings(void)
{
    int err;

    err = nvs_read(&nvs, NVS_ID_DUTY_CYCLE, &motor_state.duty_cycle, sizeof(motor_state.duty_cycle));
    if (err > 0)
    {
        printk("Loaded duty cycle: %d%%\n", motor_state.duty_cycle);
    }
    else
    {
        printk("Using default duty: %d%%\n", motor_state.duty_cycle);
    }

    err = nvs_read(&nvs, NVS_ID_MOTOR_STATE, &motor_state.motor_on,
                   sizeof(motor_state.motor_on));
    if (err > 0)
    {
        printk("Loaded motor state: %s\n", motor_state.motor_on ? "ON" : "OFF");
    }
    else
    {
        motor_state.motor_on = false;
    }
}