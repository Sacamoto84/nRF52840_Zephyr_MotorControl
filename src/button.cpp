#include "define.h"

#define BUTTON_NODE DT_ALIAS(sw0)

#define DOUBLE_CLICK_TIMEOUT_MS 500
#define LONG_PRESS_TIMEOUT_MS 2000
#define DEBOUNCE_TIME_MS 50

// ==================== Глобальные переменные ====================
const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
struct gpio_callback button_cb_data;

button_state_t button_state = {0};

extern uint8_t duty_cycle;
extern bool motor_on;
extern bool pwm_active;

uButton b(button);

extern "C" void buttonLoop()
{
    // if (b.tick())
    // {
    //     if (b.press())
    //         printk("Press\n");
    //     if (b.click())
    //         printk("Click\n");
    //     if (b.hold())
    //         printk("Hold\n");
    //     if (b.releaseHold())
    //         printk("ReleaseHold\n");
    //     if (b.step())
    //         printk("Step\n");
    //     if (b.releaseStep())
    //         printk("releaseStep\n");
    //     if (b.release())
    //         printk("Release\n");
    //     if (b.hasClicks())
    //     {
    //         printk("Clicks: %d\n", b.getClicks());
    //     }
    //     if (b.timeout())
    //         printk("Timeout\n");
    // }

    b.tick();


    if (b.press())
        printk("Press\n");
    if (b.click())
        printk("Click\n");
    if (b.hold())
        printk("Hold\n");
    if (b.releaseHold())
        printk("ReleaseHold\n");
    if (b.step())
        printk("Step\n");
    if (b.releaseStep())
        printk("releaseStep\n");
    if (b.release())
        printk("Release\n");
    if (b.hasClicks())
    {
        printk("Clicks: %d\n", b.getClicks());
    }
    if (b.timeout())
        printk("Timeout\n");


}







static void long_press_handler(void)
{
    printk("\nДолгое нажатие\n");

    // if (motor_on)
    // {
    //     for (int i = 0; i <= 100; i += 10)
    //     {
    //         motor_set_pwm(i);
    //         k_sleep(K_MSEC(100));
    //     }
    //     for (int i = 100; i >= 0; i -= 10)
    //     {
    //         motor_set_pwm(i);
    //         k_sleep(K_MSEC(100));
    //     }
    //     motor_set_pwm(duty_cycle);
    // }
}

// Work handlers
void double_click_timeout_work(struct k_work *work)
{
    button_state.waiting_for_second_click = false;
    single_click_handler();
}

// GPIO ISR
extern "C" void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{

    printk(BOLD FG(82) "\n%lld: Button ISR \033[0m %d\n", k_uptime_get(), gpio_pin_get_dt(&button));

    // int64_t now = k_uptime_get();
    // int button_value = gpio_pin_get_dt(&button);
    // printk(RESET "Now %lld pin: %d\n " RESET, now, button_value);

    //b.tick();

    // buttonLoop();

    // if (button_value == 1)
    // {
    //     // Кнопка нажата
    //     int64_t time_since_last = now - button_state.last_press_time;

    //     if (time_since_last < DEBOUNCE_TIME_MS)
    //     {
    //         return; // Debounce
    //     }

    //     button_state.is_pressed = true;
    //     button_state.press_start_time = now;
    //     button_state.long_press_triggered = false;

    //     k_work_reschedule(&button_state.long_press_check,
    //                       K_MSEC(LONG_PRESS_TIMEOUT_MS));

    //     if (button_state.waiting_for_second_click)
    //     {
    //         k_work_cancel_delayable(&button_state.double_click_timeout);
    //         button_state.waiting_for_second_click = false;
    //         double_click_handler();
    //     }
    // }
    // else
    // {
    //     // Кнопка отпущена
    //     button_state.is_pressed = false;
    //     int64_t press_duration = now - button_state.press_start_time;

    //     k_work_cancel_delayable(&button_state.long_press_check);

    //     if (!button_state.long_press_triggered && press_duration < LONG_PRESS_TIMEOUT_MS)
    //     {
    //         button_state.waiting_for_second_click = true;
    //         k_work_reschedule(&button_state.double_click_timeout,  K_MSEC(DOUBLE_CLICK_TIMEOUT_MS));
    //     }

    //     button_state.last_press_time = now;
    // }
}

void long_press_check_work(struct k_work *work)
{
    if (button_state.is_pressed && !button_state.long_press_triggered)
    {
        button_state.long_press_triggered = true;
        long_press_handler();
    }
}

// ==================== Обработка кнопки ====================
void single_click_handler(void)
{
    printk("\nSingle click\n");
    // motor_toggle();
}

void double_click_handler(void)
{
    printk("\nDouble двойное нажатие\n");
    // duty_cycle = 50;
    // if (motor_on)
    //{
    //     motor_set_pwm(duty_cycle);
    // }
    // nvs_save_settings();
}
