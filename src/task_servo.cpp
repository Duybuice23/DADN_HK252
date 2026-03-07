#include "task_servo.h"
#include <ESP32Servo.h>

static Servo gateServo;

static const int SERVO_PIN = 17;       
static const int SERVO_OPEN_ANGLE = 80;
static const int SERVO_CLOSE_ANGLE = 10;

void servo_set_angle(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    gateServo.write(angle);
}

void servo_init() {
    // 50Hz default 
    gateServo.setPeriodHertz(50);

    // min/max 
    gateServo.attach(SERVO_PIN, 500, 2400);

    // initially closed barrier
    servo_set_angle(SERVO_CLOSE_ANGLE);
    delay(300);
}

void servo_open_gate() {
    servo_set_angle(SERVO_OPEN_ANGLE);
}

void servo_close_gate() {
    servo_set_angle(SERVO_CLOSE_ANGLE);
}