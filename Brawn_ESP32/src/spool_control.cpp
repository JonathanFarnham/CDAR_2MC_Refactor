#include "spool_control.h"
#include "config.h"
#include <ESP32Servo.h>
#include <math.h>

using namespace SpoolConfig;

//Hardware
static Servo guideServo;

//Encoder (ISR-driven quadrature)
static volatile long spool_ticks = 0;

static void IRAM_ATTR isr_spool()
{
    //Positive Ticks = unspooling if wired backwards flip SPOOL_MOTOR_INVERT
    if (digitalRead(ENCODER_SPOOL_B) == LOW) spool_ticks++;
    else spool_ticks--;
}

//Control State
static float target_wire_mm = 0.0f; //commanded value
static float actual_wire_mm = 0.0f; //integrated from encoder
static float wraps_on_spool = 0.0f; //decreases as unspooled->fractional value
static long last_tick_sample = 0;
static unsigned long lastControlTime = 0;

//Geometry Helpers
static inline float wiresPerLayer()
{
    return SPOOL_USABLE_WIDTH_MM / WIRE_DIAMETER_MM;
}

//Which layer is currently topmost (the wire is leaving from or landing on)
static inline int currentLayer()
{
    int layer = (int)floorf(wraps_on_spool / wiresPerLayer());
    if (layer < 0) layer = 0;
    if (layer >= MAX_LAYERS) layer = MAX_LAYERS - 1;
    return layer;
}

//Wire Length per revolution at the current layer
static inline float currentCircumference()
{
    float r = SPOOL_CORE_RADIUS_MM + (float)currentLayer() * WIRE_DIAMETER_MM + WIRE_RADIUS_MM;
    return 2.0f * PI * r;
}

//Motor Driver
static void setSpoolMotor(int pwm)
{
    if (SPOOL_MOTOR_INVERT) pwm = -pwm;
    pwm = constrain(pwm, -MAX_SPOOL_PWM, MAX_SPOOL_PWM);

    if (pwm > 0)
    {
        digitalWrite(MOTOR_SPOOL_IN1, HIGH);
        digitalWrite(MOTOR_SPOOL_IN2, LOW);
        analogWrite(MOTOR_SPOOL_EN, pwm);
    } else if (pwm < 0)
    {
        digitalWrite(MOTOR_SPOOL_IN1, LOW);
        digitalWrite(MOTOR_SPOOL_IN2, HIGH);
        analogWrite (MOTOR_SPOOL_EN, -pwm);
    } else
    {
        digitalWrite(MOTOR_SPOOL_IN1, LOW);
        digitalWrite(MOTOR_SPOOL_IN2, LOW);
        analogWrite(MOTOR_SPOOL_EN, 0);
    }
}

//Public Functions
void initSpoolSystem()
{
    pinMode(MOTOR_SPOOL_EN, OUTPUT);
    pinMode(MOTOR_SPOOL_IN1, OUTPUT);
    pinMode(MOTOR_SPOOL_IN2, OUTPUT);
    setSpoolMotor(0);

    pinMode(ENCODER_SPOOL_A, INPUT_PULLUP);
    pinMode(ENCODER_SPOOL_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENCODER_SPOOL_A), isr_spool, RISING);

    guideServo.attach(SPOOL_SERVO);

    resetSpoolOrigin();
    Serial.println("Spool System Initialized");
}

void resetSpoolOrigin()
{
    noInterrupts();
    spool_ticks = 0;
    interrupts();

    last_tick_sample = 0;
    actual_wire_mm = 0.0f;
    target_wire_mm = 0.0f;
    wraps_on_spool = INITIAL_WRAPS_ON_SPOOL;

    //Drive Servo to correct starting postion for top layer
    float N_per_layer = wiresPerLayer();
    float pos_in_layer = wraps_on_spool - floorf(wraps_on_spool / N_per_layer) * N_per_layer;
    float frac = pos_in_layer / N_per_layer;
    int angle = (currentLayer() % 2 == 0) ? SERVO_MIN_DEG + (int)(frac * (SERVO_MAX_DEG - SERVO_MIN_DEG)) : SERVO_MAX_DEG - (int)(frac * (SERVO_MAX_DEG - SERVO_MIN_DEG));
    guideServo.write(constrain(angle, SERVO_MIN_DEG, SERVO_MAX_DEG));
}

void setSpoolPositionTarget(float robot_distance_mm)
{
    //Wire paid out must at least be equal to the robots distance from origin
    float t = robot_distance_mm + SLACK_MM;
    if (t < 0.0f) t = 0.0f;
    target_wire_mm = t;
}

void stopSpool()
{
    setSpoolMotor(0);
}

void updateSpoolControl()
{
    unsigned long now = millis();
    if (now - lastControlTime < CONTROL_INTERVAL_MS) return;
    lastControlTime = now;

    //integrate encoder delta into wire pay out estimate
    long current_ticks;
    noInterrupts();
    current_ticks = spool_ticks;
    interrupts();

    long delta_ticks = current_ticks - last_tick_sample;
    last_tick_sample = current_ticks;

    if (delta_ticks != 0)
    {
        float delta_revs = (float)delta_ticks / (float)SPOOL_COUNTS_PER_REV;
        float circ = currentCircumference();

        actual_wire_mm += delta_revs * circ; //positive = unspooled
        wraps_on_spool -= delta_revs; //Unspooling removes wraps

        if (wraps_on_spool < 0.0f) wraps_on_spool = 0.0f; //Out of Wire
    }

    //Update Servo Guide Position
    float N_per_layer = wiresPerLayer();
    float pos_in_layer = wraps_on_spool - floorf(wraps_on_spool / N_per_layer) * N_per_layer;
    float frac = pos_in_layer / N_per_layer; //0.0 -> 1.0

    int servo_angle = (currentLayer() % 2 == 0) ? SERVO_MIN_DEG + (int)(frac * (SERVO_MAX_DEG - SERVO_MIN_DEG)) : SERVO_MAX_DEG - (int)(frac * (SERVO_MAX_DEG - SERVO_MIN_DEG));
    servo_angle = constrain(servo_angle, SERVO_MIN_DEG, SERVO_MAX_DEG);
    guideServo.write(servo_angle);

    //P control on wire-length error
    float error_mm = target_wire_mm - actual_wire_mm;

    if (fabsf(error_mm) < DEADBAND_MM)
    {
        setSpoolMotor(0);
        return;
    }

    //safety -> Dont try to unspool past empty
    if (wraps_on_spool <= 0.0f && error_mm > 0)
    {
        setSpoolMotor(0);
        Serial.println("SPOOL EMPTY");
        return;
    }
    //safety -> dont reel in past the core
    if (wraps_on_spool >= MAX_LAYERS * N_per_layer && error_mm < 0)
    {
        setSpoolMotor(0);
        Serial.println("SPOOL FULL");
        return;
    }

    int pwm = (int)(error_mm * POS_KP);
    setSpoolMotor(pwm);
}

//Telemetry Getters
float getSpoolWirePaidOut() { return actual_wire_mm; }
int getSpoolCurrentLayer() { return currentLayer(); }
float getSpoolWrapsRemaining() { return wraps_on_spool; }