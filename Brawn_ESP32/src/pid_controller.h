#pragma once

class PIDController
{
    public:
        PIDController(float kp, float ki, float kd);
        float compute(float setpoint, float measured, float dt_seconds);
        void reset();
        void updateConstants(float kp, float ki, float kd);

    private:
        float _kp, _ki, _kd;
        float _integral;
        float _prevError;
};