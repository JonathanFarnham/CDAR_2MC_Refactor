#include "mpu_handler.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "config.h"

Adafruit_MPU6050 mpu;

//Volatile flag for the interrupt
volatile bool mpuDataReady = false;

float currentYaw = 0.0;
unsigned long lastMPUTime = 0;
float gyroZ_offset = 0.0;

//ISR
void IRAM_ATTR dmpDataReady()
{
    mpuDataReady = true;
}

void initMPU()
{
    Wire.begin(I2C_SDA, I2C_SCL);
    if (!mpu.begin())
    {
        Serial.println("Failed to find MPU6050");
        return;
    }

    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    //Set ESP pin to receive the interrupt
    pinMode(MPU_INT_PIN, INPUT);

    //Enable Data Ready interrupt
    Wire.beginTransmission(0x68); //default address for MPU
    Wire.write(0x38); //Target the INT_ENABLE register
    Wire.write(0x01); //Write 1 to bit 0 (DATA_RDY_EN)
    Wire.endTransmission();

    //Attach ISR trigger to rising edge
    attachInterrupt(digitalPinToInterrupt(MPU_INT_PIN), dmpDataReady, RISING);

    Serial.println("calibrating gyro: ");
    float sumZ = 0;
    for(int i = 0; i < 200; i++)
    {
        //wait for flag before reading during calibration with timeout
        unsigned long waitStart = millis();
        while(!mpuDataReady && (millis() - waitStart < 20)) { yield(); }
        mpuDataReady = false;

        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        sumZ += g.gyro.z;
    }
    gyroZ_offset = sumZ / 200.0;

    //switch to micros() for higher precision timing with rapid interrupts
    lastMPUTime = micros();
}

void updateMPU()
{
    //Only Execute the slow I2C read if told
    if (mpuDataReady)
    {
        mpuDataReady = false; //Reset flag

        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp); //Clear MPU internal interrupt state

        unsigned long now = micros();
        float dt = (now - lastMPUTime) / 1000000.0; //Delta time in seconds
        lastMPUTime = now;

        float zDegSec = (g.gyro.z - gyroZ_offset) * 57.2958;

        if (abs(zDegSec) > 1.0)
        {
            currentYaw += zDegSec * dt;
        }
    }
}

float getYawAngle()
{
    return currentYaw;
}

void resetYaw()
{
    currentYaw = 0.0;
    lastMPUTime = micros(); //Reset Timer so dt doesnt spike
    mpuDataReady = false; //clear interrupts triggered during delay
}