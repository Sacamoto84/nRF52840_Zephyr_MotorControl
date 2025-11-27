#include "define.h"

#include <zephyr/fs/zms.h>

extern struct zms_fs zms;

extern int zmsSave(uint32_t id, uint8_t data);
extern int zmsRead(uint32_t id, uint8_t *data, uint8_t define);

bool global_cpu_active = false;

bool global_motor_on = false;
bool global_pwm_active = false;

uint8_t global_duty_cycle = 50;

#define NVS_ID_DUTY_CYCLE 1
#define NVS_ID_MOTOR_STATE 2

//-------------------------------
void saveDutyCycle()
{
    zmsSave(NVS_ID_DUTY_CYCLE, global_duty_cycle);
}
//-------------------------------
void readDutyCycle()
{
    zmsRead(NVS_ID_DUTY_CYCLE, &global_duty_cycle, 50);
}
