#include "define.h"

#define PWM_NODE DT_NODELABEL(pwm0)
#define PWM_CHANNEL 0
#define PWM_PERIOD_NS 20000000 // 50 Hz

static const struct device *pwm_dev;

// ==================== PWM управление ====================
void motor_set_pwm(uint8_t duty)
{
    if (duty > 100) duty = 100;

    if (duty == 0 || !motor_state.motor_on) {
        pwm_set(pwm_dev, PWM_CHANNEL, 0, 0, 0);
        if (motor_state.pwm_active) {
            pm_device_action_run(pwm_dev, PM_DEVICE_ACTION_SUSPEND);
            motor_state.pwm_active = false;
            printk("PWM suspended\n");
        }
    } else {
        if (!motor_state.pwm_active) {
            pm_device_action_run(pwm_dev, PM_DEVICE_ACTION_RESUME);
            motor_state.pwm_active = true;
            printk("PWM resumed\n");
        }

        uint32_t pulse_ns = (uint64_t)PWM_PERIOD_NS * duty / 100;
        pwm_set(pwm_dev, PWM_CHANNEL, PWM_PERIOD_NS, pulse_ns, 0);
        printk("Motor PWM: %d%% (%u ns)\n", duty, pulse_ns);
    }
}