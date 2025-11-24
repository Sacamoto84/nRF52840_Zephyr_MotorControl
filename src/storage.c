
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/zms.h>

// https://docs.zephyrproject.org/latest/doxygen/html/group__zms__high__level__api.html

// NVS настройки
#define NVS_PARTITION storage
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(NVS_PARTITION)
#define NVS_ID_DUTY_CYCLE 1
#define NVS_ID_MOTOR_STATE 2

static struct zms_fs zms;

extern uint8_t duty_cycle;
extern bool motor_on;
extern bool pwm_active;

// ==================== NVS функции ====================
int nvs_init_storage(void)
{
    int err;
    struct flash_pages_info info;

    zms.flash_device = NVS_PARTITION_DEVICE;
    if (!device_is_ready(zms.flash_device))
    {
        printk("Flash device not ready\n");
        return -ENODEV;
    }

    // Получаем offset раздела
    zms.offset = NVS_PARTITION_OFFSET;

    // Получаем информацию о странице
    err = flash_get_page_info_by_offs(zms.flash_device, zms.offset, &info);
    if (err)
    {
        printk("Unable to get flash page info\n");
        return err;
    }

    // nRF52840: sector_size = 4096 байт (4 KB)
    zms.sector_size = info.size;

    // Используем 3-4 сектора для NVS (из доступных 8)
    zms.sector_count = 8; // 8 * 4KB = 32 KB для NVS

    printk("Zms init:\n");
    printk("  Flash device: %s\n", zms.flash_device->name);
    printk("  Offset: 0x%08X\n", zms.offset);
    printk("  Sector size: %d bytes\n", zms.sector_size);
    printk("  Sector count: %d\n", zms.sector_count);
    printk("  Total size: %d bytes\n", zms.sector_size * zms.sector_count);

    // Монтируем NVS
    err = zms_mount(&zms);
    if (err)
    {
        printk("NVS mount failed: %d\n", err);
        return err;
    }

    printk("  Free space: %d bytes\n", zms_calc_free_space(&zms));

    printk("NVS mounted successfully\n");
    return 0;
}

void nvs_save_settings(void)
{
    int err;

    err = zms_write(&zms, NVS_ID_DUTY_CYCLE, &duty_cycle, sizeof(duty_cycle));
    if (err < 0)
        printk("Failed to write duty cycle to NVS: %d\n", err);

    err = zms_write(&zms, NVS_ID_MOTOR_STATE, &motor_on, sizeof(motor_on));
    if (err < 0)
        printk("Failed to write motor state to NVS: %d\n", err);

    printk("Settings saved: duty=%d%%, motor=%s\n", duty_cycle, motor_on ? "ON" : "OFF");
}

void nvs_load_settings(void)
{
    int err;

    err = zms_read(&zms, NVS_ID_DUTY_CYCLE, &duty_cycle, sizeof(duty_cycle));
    if (err > 0)
    {
        printk("Loaded duty cycle: %d%%\n", duty_cycle);
    }
    else
    {
        printk("Using default duty: %d%%\n", duty_cycle);
    }

    err = zms_read(&zms, NVS_ID_MOTOR_STATE, &motor_on, sizeof(motor_on));
    if (err > 0)
    {
        printk("Loaded motor state: %s\n", motor_on ? "ON" : "OFF");
    }
    else
    {
        motor_on = false;
    }
}