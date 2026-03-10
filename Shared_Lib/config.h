//Shared Configuration File
#pragma once

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

//PID Tuning
#define PID_KP 0.25  // Tuned for power
#define PID_KI 0.00
#define PID_KD 0.01
#define CALC_INTERVAL 20 // Calculate velocity every 20ms

//Motor Inversion
#define MOTOR_LEFT_INVERT true
#define MOTOR_RIGHT_INVERT true // Fixed runaway issue

//Robot Parameters **NEED UPDATES
#define TICKS_PER_FOOT 282
#define TICKS_PER_TURN 145

//Grid Speed Values
#define SPEED_GRID_RPM 1000
#define SPEED_TURN_RPM 600
