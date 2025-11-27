#include "define.h"

extern uint8_t duty_cycle;
extern bool motor_on;
extern bool pwm_active;

#define PWM_CHANNEL 0
#define PWM_PERIOD_NS 20000000 // 50 Hz

const struct device *pwm_dev;

// ==================== PWM управление ====================
void motor_set_pwm(uint8_t duty)
{
    if (duty > 100) duty = 100;

    if (duty == 0 || !global_motor_on) {
        pwm_set(pwm_dev, PWM_CHANNEL, 0, 0, 0);
        if (global_pwm_active) {
            pm_device_action_run(pwm_dev, PM_DEVICE_ACTION_SUSPEND);
            global_pwm_active = false;
            printk("PWM suspended\n");
        }
    } else {
        if (!global_pwm_active) {
            pm_device_action_run(pwm_dev, PM_DEVICE_ACTION_RESUME);
            global_pwm_active = true;
            printk("PWM resumed\n");
        }

        uint32_t pulse_ns = (uint64_t)PWM_PERIOD_NS * duty / 100;
        pwm_set(pwm_dev, PWM_CHANNEL, PWM_PERIOD_NS, pulse_ns, 0);
        printk("Motor PWM: %d%% (%u ns)\n", duty, pulse_ns);
    }
}

void motor_toggle(void)
{
    motor_on = !motor_on;
    printk("Motor %s at %d%%\n", global_motor_on ? "ON" : "OFF", duty_cycle);
    motor_set_pwm(motor_on ? duty_cycle : 0);
    nvs_save_settings();
}