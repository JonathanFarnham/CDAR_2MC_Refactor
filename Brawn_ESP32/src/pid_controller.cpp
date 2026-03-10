#include "pid_controller.h"
#include <Arduino.h>

PIDController::PIDController(float kp, float ki, float kd)
    :_kp(kp), _ki(ki), _kd(kd), _integral(0), _prevError(0) {}

float PIDController::compute(float setpoint, float measured, float dt_seconds)
{
    float error = setpoint - measured;

    //proportional
    float P = _kp * error;

    //integral
    _integral += error * dt_seconds;

    //Anti windup clamping to prevent integral growing infinetly
    float maxIntegral = (_ki > 0) ? (255.0f / _ki) : 0; 
    if (maxIntegral > 0) 
    {
        if (_integral > maxIntegral) _integral = maxIntegral;
        else if (_integral < -maxIntegral) _integral = -maxIntegral;
    }

    float I = _ki * _integral;

    //derivative
    float derivative = (error - _prevError) / dt_seconds;
    float D = _kd * derivative;
    _prevError = error;

    return P + I + D;
}

void PIDController::reset()
{
    _integral = 0;
    _prevError = 0;
}

void PIDController::updateConstants(float kp, float ki, float kd)
{
    _kp = kp; _ki = ki; _kd = kd;
}