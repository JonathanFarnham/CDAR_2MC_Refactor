//Shared Configuration File
#pragma once
#include <Arduino.h>

//UART Communication Pins
#define UART_TX_PIN 17
#define UART_RX_PIN 16
#define UART_BAUD_RATE 115200

//Wifi Settings
#define WIFI_SSID "CDAR_Control"
#define WIFI_PASS "password"

//I2C / MPU-6050 Pins (Brain Only)
#define I2C_SDA 21
#define I2C_SCL 22
#define MPU_INT_PIN 19

//Motor Pin Setup (Brawn Only)
#define MOTOR_LEFT_EN 13
#define MOTOR_LEFT_IN1 12
#define MOTOR_LEFT_IN2 14
#define ENCODER_LEFT_A 21
#define ENCODER_LEFT_B 22

#define MOTOR_RIGHT_EN 27
#define MOTOR_RIGHT_IN1 32
#define MOTOR_RIGHT_IN2 33
#define ENCODER_RIGHT_A 25
#define ENCODER_RIGHT_B 26


//Physics and Encoder Settings
#define COUNTS_PER_REV 28
#define MAX_RPM_HARDWARE 5600
#define MAX_OPERATING_RPM 3000

//Kickstart (not used)
#define KICKSTART_MS 50
#define KICKSTART_PWM 0
#define MIN_REGULATED_RPM 40

//PID Tuning and Wheel Trim
#define PID_KP 0.8
#define PID_KI 0.3
#define PID_KD 0.00 //Removed D value encoders too low resolution to get effective results
#define CALC_INTERVAL 100 //Calc Velocity every 100ms
#define WHEEL_TRIM 1.03 //Reduce Speed of the Left motor to account for tolerances

//Motor Inversion
#define MOTOR_LEFT_INVERT false
#define MOTOR_RIGHT_INVERT true

//Robot Parameters
#define TICKS_PER_FOOT 50
#define TICKS_PER_TURN 54

//Grid Speed Values
#define SPEED_GRID_RPM 300
#define SPEED_TURN_RPM 200
