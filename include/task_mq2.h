#ifndef TASK_MQ2_H
#define TASK_MQ2_H

#include <Arduino.h>

void mq2_init();
int mq2_read_raw();
bool mq2_detected();
float mq2_read_percent();

#endif