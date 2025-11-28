#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <nrfx_saadc.h> // Для доступа к калибровке

// Получаем устройство ADC
#define ADC_NODE DT_NODELABEL(adc)
static const struct device *adc_dev = DEVICE_DT_GET(ADC_NODE);

// Настройки канала (должны совпадать с Device Tree)
#define ADC_CHANNEL_ID 5
#define ADC_RESOLUTION 12 // 12-bit разрешение
#define ADC_GAIN ADC_GAIN_1_6
#define ADC_REFERENCE ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 5)

// Буфер для хранения результата
static int16_t sample_buffer;

// Конфигурация канала
static struct adc_channel_cfg channel_cfg = {
    .gain = ADC_GAIN,
    .reference = ADC_REFERENCE,
    .acquisition_time = ADC_ACQUISITION_TIME,
    .channel_id = ADC_CHANNEL_ID,
    .differential = 0};

// Конфигурация последовательности чтения
static struct adc_sequence sequence = {
    .channels = BIT(ADC_CHANNEL_ID),
    .buffer = &sample_buffer,
    .buffer_size = sizeof(sample_buffer),
    .resolution = ADC_RESOLUTION,
};

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <hal/nrf_saadc.h>

// ... ваши определения ...

/**
 * @brief Калибровка SAADC через регистры
 */
void adc_calibrate_registers(void)
{
    printk("Starting SAADC calibration via registers...\n");

    NRF_SAADC_Type *saadc = NRF_SAADC;

    saadc->EVENTS_CALIBRATEDONE = 0;
    saadc->TASKS_CALIBRATEOFFSET = 1;

    int timeout = 1000;
    while (saadc->EVENTS_CALIBRATEDONE == 0 && timeout > 0) {
        k_busy_wait(1000);
        timeout--;
    }

    if (timeout == 0) {
        printk("SAADC calibration timeout!\n");
        return;
    }

    saadc->EVENTS_CALIBRATEDONE = 0;

    printk("SAADC calibration completed\n");
}

void adc_setup_registers(void)
{
    NRF_SAADC_Type *saadc = NRF_SAADC;

    printk("Configuring SAADC via registers (14-bit oversampled)...\n");

    // 1. Включаем SAADC
    saadc->ENABLE = (SAADC_ENABLE_ENABLE_Enabled << SAADC_ENABLE_ENABLE_Pos);

    // 2. Настройка канала 5
    saadc->CH[5].CONFIG =
        (SAADC_CH_CONFIG_RESP_Bypass << SAADC_CH_CONFIG_RESP_Pos) |
        (SAADC_CH_CONFIG_RESN_Bypass << SAADC_CH_CONFIG_RESN_Pos) |
        (SAADC_CH_CONFIG_GAIN_Gain1_5 << SAADC_CH_CONFIG_GAIN_Pos) |
        (SAADC_CH_CONFIG_REFSEL_Internal << SAADC_CH_CONFIG_REFSEL_Pos) |
        (SAADC_CH_CONFIG_TACQ_40us << SAADC_CH_CONFIG_TACQ_Pos) |
        (SAADC_CH_CONFIG_MODE_SE << SAADC_CH_CONFIG_MODE_Pos) |
        (SAADC_CH_CONFIG_BURST_Enabled << SAADC_CH_CONFIG_BURST_Pos);

    // 3. Пины — AIN5
    saadc->CH[5].PSELP = SAADC_CH_PSELP_PSELP_AnalogInput5;
    saadc->CH[5].PSELN = SAADC_CH_PSELN_PSELN_NC;

    // 4. Физическое разрешение = 12 бит
    saadc->RESOLUTION =
        (SAADC_RESOLUTION_VAL_12bit << SAADC_RESOLUTION_VAL_Pos);

    // 5. OVERSAMPLE = 4x → 14 бит
    saadc->OVERSAMPLE =
        (SAADC_OVERSAMPLE_OVERSAMPLE_Over256x << SAADC_OVERSAMPLE_OVERSAMPLE_Pos);

    // 6. Режим запуска выборок вручную
    saadc->SAMPLERATE =
        (SAADC_SAMPLERATE_MODE_Task << SAADC_SAMPLERATE_MODE_Pos);

    printk("SAADC configured\n");

    // 7. Калибровка
    //adc_calibrate_registers();

    k_sleep(K_MSEC(5));
}

/**
 * @brief Чтение ADC через регистры (пример)
 */
int16_t adc_read_registers(void)
{
    NRF_SAADC_Type *saadc = NRF_SAADC;
    volatile int16_t result;

    // 1. Очистить события
    saadc->EVENTS_STARTED = 0;
    saadc->EVENTS_END = 0;
    saadc->EVENTS_DONE = 0;

    // 2. Настроить буфер результата
    saadc->RESULT.PTR = (uint32_t)&result;
    saadc->RESULT.MAXCNT = 1;

    // 3. Запустить START задачу
    saadc->TASKS_START = 1;

    // Ждём события STARTED
    while (saadc->EVENTS_STARTED == 0)
        ;
    saadc->EVENTS_STARTED = 0;

    // 4. Запустить SAMPLE задачу
    saadc->TASKS_SAMPLE = 1;

    // Ждём события END
    while (saadc->EVENTS_END == 0)
        ;
    saadc->EVENTS_END = 0;

    // 5. Остановить SAADC
    saadc->TASKS_STOP = 1;

    // Ждём события STOPPED
    while (saadc->EVENTS_STOPPED == 0)
        ;
    saadc->EVENTS_STOPPED = 0;

    return result + 2;
}

/**
 * @brief Инициализация с регистрами
 */
int adc_init(void)
{
    // int ret;

    // if (!device_is_ready(adc_dev)) {
    //     printk("ADC device not ready\n");
    //     return -1;
    // }

    // // Стандартная настройка через Zephyr API
    // ret = adc_channel_setup(adc_dev, &channel_cfg);
    // if (ret < 0) {
    //     printk("ADC channel setup failed: %d\n", ret);
    //     return ret;
    // }

    // // Дополнительная калибровка через регистры
    // adc_calibrate_registers();

    // printk("ADC initialized with register calibration\n");

    adc_setup_registers();

    return 0;
}