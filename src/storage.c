
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/zms.h>

// https://docs.zephyrproject.org/latest/doxygen/html/group__zms__high__level__api.html

// NVS настройки
#define NVS_PARTITION storage
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(NVS_PARTITION)

#define ZMS_NUM_SECTORS 8

struct zms_fs zms;

extern uint8_t duty_cycle;
extern bool motor_on;
extern bool pwm_active;

// ==================== NVS функции ====================
/**
 * @brief Инициализация ZMS хранилища
 * @return 0 при успехе, отрицательное значение при ошибке
 */
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
    zms.sector_count = ZMS_NUM_SECTORS; // 8 * 4KB = 32 KB для NVS

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

/**
 * @brief Сохранить байт данных в ZMS
 * @param id Идентификатор записи
 * @param data Данные для сохранения
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int zmsSave(uint32_t id, uint8_t data)
{
    int err = zms_write(&zms, id, &data, 1);

    if (err < 0)
    {
        printk("ZMS write error (id: %lu): %d - ", (unsigned long)id, err);

        switch (err)
        {
        case -EACCES:
            printk("not initialized\n");
            break;
        case -ENXIO:
            printk("device error\n");
            break;
        case -EIO:
            printk("read/write error\n");
            break;
        case -EINVAL:
            printk("invalid length\n");
            break;
        case -ENOSPC:
            printk("no space left\n");
            break;
        default:
            printk("unknown error\n");
            break;
        }
    }

    return err;
}

/**
 * @brief Прочитать байт данных из ZMS
 * @param id Идентификатор записи
 * @param data Указатель для сохранения прочитанных данных
 * @param default_value Значение по умолчанию, если запись не найдена
 * @return 0 при успехе, отрицательное значение при ошибке
 */
int zmsRead(uint32_t id, uint8_t *data, uint8_t default_value)
{

    if (data == NULL)
    {
        printk("ZMS read error: null pointer\n");
        return -EINVAL;
    }

    int err = zms_read(&zms, id, data, 1);
    if (err == 1)
    {
        printk("ZMS read (id: %lu): %u\n", (unsigned long)id, *data);
        return 0;
    }

    // Обработка ошибок
    printk("ZMS read error (id: %lu): %d - ", (unsigned long)id, err);

    switch (err)
    {
    case -EACCES:
        printk("not initialized\n");
        break;
    case -EIO:
        printk("read/write error\n");
        break;
    case -ENOENT:
        printk("entry not found, using default: %u\n", default_value);
        break;
    default:
        printk("unknown error\n");
        break;
    }

    *data = default_value;
    return err;
}
