#include "voltage_reading.h"
#include "config.h"
#include <SPI.h>

//ADS1220 SPI Commands
#define CMD_RESET 0x06
#define CMD_START 0x08
#define CMD_POWERDOWN 0x02
#define CMD_RDATA 0x10
#define CMD_RREG(r, n) (0x20 | ((r) << 2) | ((n) - 1))
#define CMD_WREG(r, n) (0x40 | ((r) << 2) | ((n) - 1))

//ADS1220 Register Configuration
#define ADS1220_REG0_VAL 0x01
#define ADS1220_REG1_VAL 0x40
#define ADS1220_REG2_VAL 0x30
#define ADS1220_REG3_VAL 0x00

//Conversion Constants
#define ADS1220_VREF_V 2.048f
#define ADS1220_FSR_COUNTS 8388608.0f
#define ADS1220_LSB_MV (ADS1220_VREF_V * 1000.0f / ADS1220_FSR_COUNTS)

//Reduced from original 8 to reduce blocking time during grid movement
#define ELECTRODE_NUM_AVG 4

//Use HSPI Bus (SPI controller 1) to avoid any conflict with default VSPI
static SPIClass ads_spi(HSPI);

//Low Level Helpers
static void ads_cs_low() { digitalWrite(ADS1220_CS_PIN, LOW); }
static void ads_cs_high() { digitalWrite(ADS1220_CS_PIN, HIGH); }

static uint8_t ads_transfer(uint8_t data)
{
    return ads_spi.transfer(data);
}

static void ads1220_send_cmd(uint8_t cmd)
{
    ads_cs_low();
    ads_transfer(cmd);
    ads_cs_high();
}

static void ads1220_write_reg(uint8_t reg, uint8_t value)
{
    ads_cs_low();
    ads_transfer(CMD_WREG(reg, 1));
    ads_transfer(value);
    ads_cs_high();
}

static uint8_t ads1220_read_reg(uint8_t reg)
{
    ads_cs_low();
    ads_transfer(CMD_RREG(reg, 1));
    uint8_t val = ads_transfer(0x00);
    ads_cs_high();
    return val;
}

static int32_t ads1220_read_raw()
{
    ads_cs_low();
    ads_transfer(CMD_RDATA);
    uint8_t b2 = ads_transfer(0x00); //MSB
    uint8_t b1 = ads_transfer(0x00);
    uint8_t b0 = ads_transfer(0x00); //LSB
    ads_cs_high();

    int32_t raw = ((int32_t)b2 << 16) | ((int32_t)b1 << 8) | b0;
    if (raw & 0x800000) raw |= 0xFF000000; //sign extend 24-bit -> 32bit
    return raw;
}


//Triggers a single conversion and waits for DRDY to go low (data ready)

static int32_t ads1220_single_conversion()
{
    ads1220_send_cmd(CMD_START);

    unsigned long t0 = millis();
    while (digitalRead(ADS1220_DRDY_PIN) == HIGH)
    {
        if (millis() - t0 > 200)
        {
            Serial.println("ADS1220: DRDY timeout");
            return INT32_MIN;
        }
        delay(1); //yield to free RTOS scheduler while waiting
    }

    return ads1220_read_raw();
}


//Public API
void initElectrodeSensor()
{
    pinMode(ADS1220_CS_PIN, OUTPUT);
    ads_cs_high(); //Deselect Immediately

    pinMode(ADS1220_DRDY_PIN, INPUT);

    //Begin HSPI with th custom pins defined in config
    ads_spi.begin(ADS1220_SCK_PIN, ADS1220_MISO_PIN, ADS1220_MOSI_PIN, ADS1220_CS_PIN);
    ads_spi.setFrequency(1000000); //ADS mac SPI clock 4MHz, using 1MHz for robustness
    ads_spi.setDataMode(SPI_MODE1); //CPOL = 0, CPHA = 1

    //Reset and Configure
    ads1220_write_reg(0, ADS1220_REG0_VAL);
    ads1220_write_reg(1, ADS1220_REG1_VAL);
    ads1220_write_reg(2, ADS1220_REG2_VAL);
    ads1220_write_reg(3, ADS1220_REG3_VAL);

    //Read back to verify wiring
    uint8_t r0 = ads1220_read_reg(0);
    uint8_t r1 = ads1220_read_reg(1);
    uint8_t r2 = ads1220_read_reg(2);
    uint8_t r3 = ads1220_read_reg(3);

    Serial.printf("ADS1220 REG: 0x%02X 0x%02X 0x%02X 0x%02X\n", r0, r1, r2, r3);

    if (r0 == ADS1220_REG0_VAL && r1 == ADS1220_REG1_VAL &&
        r2 == ADS1220_REG2_VAL && r3 == ADS1220_REG3_VAL)
    {
        Serial.println("ADS1220 Electrode Sensor Initialized OK");
    }
    else
    {
        Serial.println("WARNING: ADS1220 Register Mismatch");
    }
}

float readHalfCellPotential_mV()
{
    int64_t accumulator = 0;
    int valid = 0;

    for (int i = 0; i < ELECTRODE_NUM_AVG; i++)
    {
        int32_t code = ads1220_single_conversion();
        if (code != INT32_MIN)
        {
            accumulator += code;
            valid++;
        }
    }

    if (valid == 0) return 0.0f;

    float avg_code = (float)(accumulator / valid);
    return avg_code * ADS1220_LSB_MV;
}

const char* getASTMRiskLabel(float potential_mv)
{
    if (potential_mv < -500.0f) return "Severe Corrosion";
    if (potential_mv < -350.0f) return "High Probability of Active Corrosion (>90% Risk)";
    if (potential_mv <= -200.0f) return "Intermediate Corrosion Risk";
    return "Low Probability of Corrosion (<10% Risk)";
}