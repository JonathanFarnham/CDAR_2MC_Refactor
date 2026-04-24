#pragma once
#include <Arduino.h>

//Spool Geometry Configuration - edit to match spool and wire used
namespace SpoolConfig
{
    //Wire Parameters
    constexpr WIRE_DIAMETER_MM = 1.60f;
    constexpr WIRE_RADIUS_MM = WIRE_DIAMETER_MM / 2.0f;

    //Spool Parameters
    constexpr float SPOOL_CORE_RADIUS_MM = 15.0f; //Drum Radius (no wire)
    constexpr SPOOL_USABLE_WIDTH_MM = 40.0f; //width of the spool drum (will define how many wraps per layer occur)
    constexpr int MAX_LAYERS = 30;

    //Starting State
    //How many full wraps of wire are on the spool at system start
    constexpr float INITIAL_WRAPS_ON_SPOOL = 10.0f * (SPOOL_USABLE_WIDTH_MM / WIRE_DIAMETER_MM);

    //Servo Guide Sweep
    constexpr int SERVO_MIN_DEG = 0;
    constexpr int SERVO_MAX_DEG = 180;

    //Encoder
    constexpr int SPOOL_COUNTS_PER_REV = 28; //how many encoder ticks occur per revolution of spool

    //Contol
    constexpr float POS_KP = 2.0f; //PWM per mm of error
    constexpr int MAX_SPOOL_PWM = 180;
    constexpr float DEADBAND_MM = 3.0f;
    constexpr float SLACK_MM = 50.0f;
    constexpr bool SPOOL_MOTOR_INVERT = false;
    constexpr int CONTROL_INTERVAL_MS = 50;
}

//Public API
void initSpoolSystem();
void updateSpoolControl(); //call every loop
void setSpoolPositionTarget(float robot_distance_mm); //from master
void resetSpoolOrigin(); //at run start
void stopSpool();

//Telemetry
float getSpoolWirePaidOut();
int getSpoolCurrentLayer();
float getSpoolWrapsRemaining();