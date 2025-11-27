#pragma once

#ifdef __cplusplus

#include "uButtonVirt.h"

// Для использования gpio_dt_spec нужно включить Zephyr заголовок
extern "C" {
    #include <zephyr/drivers/gpio.h>
}

class uButton : public uButtonVirt {
   public:
    uButton(const struct gpio_dt_spec& _button) : button(_button) {
    }

    // вызывать в loop. Вернёт true при смене состояния
    bool tick() {
        return uButtonVirt::pollDebounce(readButton());
    }

    // прочитать состояние кнопки
    bool readButton() {
        return gpio_pin_get_dt(&button);
    }

   private:
    struct gpio_dt_spec button;
};

#endif // __cplusplus