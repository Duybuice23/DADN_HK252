#include "task_mq2.h"
#include "global.h"

void mq2_init() {
    pinMode(MQ2_AO_PIN, INPUT);
    pinMode(MQ2_DO_PIN, INPUT);
    analogReadResolution(12); //12 bit ADC resolution (0-4095) 
}

int mq2_read_analog() {
    return analogRead(MQ2_AO_PIN);
}

int mq2_read_digital() {
    return digitalRead(MQ2_DO_PIN);
}

bool mq2_detected() {
    int value = mq2_read_digital();
    return value; // 0 when gas detected, 1 when not detected (active LOW)
}
