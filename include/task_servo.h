#ifndef TASK_SERVO_H
#define TASK_SERVO_H

#include <Arduino.h>

void servo_init();
void servo_open_gate();
void servo_close_gate();
void servo_set_angle(int angle);

#endif