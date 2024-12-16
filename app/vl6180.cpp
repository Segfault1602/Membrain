#include "vl6180.h"

#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include <cmath>

#include "logging.h"

#define VL6180X_ADDR 0x29

// Define some additional registers mentioned in application notes and we use
///! period between each measurement when in continuous mode
#define SYSRANGE__INTERMEASUREMENT_PERIOD 0x001b // P19 application notes

namespace
{
constexpr float kA1 = 0.50;
constexpr float kB0 = 1.f - std::abs(kA1);

float g_prev_output = 0.f;
} // namespace

bool read_byte(uint16_t reg, uint8_t* data)
{
    uint8_t data_write[2];
    data_write[0] = (reg >> 8) & 0xFF; // MSB of register address
    data_write[1] = reg & 0xFF;        // LSB of register address
    int ret = i2c_write_blocking(i2c_default, VL6180X_ADDR, data_write, 2, true);
    if (ret < 0)
    {
        LOG_ERROR("Failed to write to VL6180X\n");
        return false;
    }

    ret = i2c_read_blocking(i2c_default, VL6180X_ADDR, data, 1, false);
    if (ret < 0)
    {
        LOG_ERROR("Failed to read from VL6180X\n");
        return false;
    }

    return true;
}

bool write_byte(uint16_t reg, uint8_t data)
{
    uint8_t txdata[3] = {0};
    txdata[0] = uint8_t(reg >> 8);
    txdata[1] = uint8_t(reg & 0xFF);
    txdata[2] = data;
    int ret = i2c_write_blocking(i2c_default, VL6180X_ADDR, txdata, 3, true);
    if (ret < 0)
    {
        LOG_ERROR("Failed to write to VL6180X\n");
        return false;
    }

    return true;
}

void load_settings()
{
    // private settings from page 24 of app note
    write_byte(0x0207, 0x01);
    write_byte(0x0208, 0x01);
    write_byte(0x0096, 0x00);
    write_byte(0x0097, 0xfd);
    write_byte(0x00e3, 0x00);
    write_byte(0x00e4, 0x04);
    write_byte(0x00e5, 0x02);
    write_byte(0x00e6, 0x01);
    write_byte(0x00e7, 0x03);
    write_byte(0x00f5, 0x02);
    write_byte(0x00d9, 0x05);
    write_byte(0x00db, 0xce);
    write_byte(0x00dc, 0x03);
    write_byte(0x00dd, 0xf8);
    write_byte(0x009f, 0x00);
    write_byte(0x00a3, 0x3c);
    write_byte(0x00b7, 0x00);
    write_byte(0x00bb, 0x3c);
    write_byte(0x00b2, 0x09);
    write_byte(0x00ca, 0x09);
    write_byte(0x0198, 0x01);
    write_byte(0x01b0, 0x17);
    write_byte(0x01ad, 0x00);
    write_byte(0x00ff, 0x05);
    write_byte(0x0100, 0x05);
    write_byte(0x0199, 0x05);
    write_byte(0x01a6, 0x1b);
    write_byte(0x01ac, 0x3e);
    write_byte(0x01a7, 0x1f);
    write_byte(0x0030, 0x00);

    // Recommended : Public registers - See data sheet for more detail
    write_byte(0x0011, 0x10); // Enables polling for 'New Sample ready'
                              // when measurement completes
    write_byte(0x010a, 0x30); // Set the averaging sample period
                              // (compromise between lower noise and
                              // increased execution time)
    write_byte(0x003f, 0x46); // Sets the light and dark gain (upper
                              // nibble). Dark gain should not be
                              // changed.
    write_byte(0x0031, 0xFF); // sets the # of range measurements after
                              // which auto calibration of system is
                              // performed
    write_byte(0x0041, 0x63); // Set ALS integration time to 100ms
    write_byte(0x002e, 0x01); // perform a single temperature calibration
                              // of the ranging sensor

    // Optional: Public registers - See data sheet for more detail
    write_byte(SYSRANGE__INTERMEASUREMENT_PERIOD,
               0x0A);         // Set default ranging inter-measurement
                              // period to 100ms
    write_byte(0x003e, 0x31); // Set default ALS inter-measurement period
                              // to 500ms
    write_byte(0x0014, 0x24); // Configures interrupt on 'New Sample
                              // Ready threshold event'
}

void start_range()
{
    write_byte(VL6180X_REG_SYSRANGE_START, 0x03);
}

void VL6180_Clear_Interrupts()
{
    write_byte(VL6180X_REG_SYSTEM_INTERRUPT_CLEAR, 0x07);
}

bool poll_range()
{
    uint8_t status;
    uint8_t range_status;
    // check the status
    read_byte(VL6180X_REG_RESULT_INTERRUPT_STATUS_GPIO, &status);
    return (status & 0x04);
}

float read_range()
{
    uint8_t status;
    read_byte(VL6180X_REG_RESULT_RANGE_STATUS, &status);
    if (!(status & 0x01))
    {
        LOG_ERROR("Range status not ready\n");
        return 0.f;
    }

    write_byte(VL6180X_REG_SYSRANGE_START, 0x01);

    constexpr uint8_t kMaxPolls = 10;
    uint8_t polls = 0;
    while (!poll_range())
    {
        sleep_ms(1);
        polls++;
        if (polls > kMaxPolls)
        {
            LOG_ERROR("Range poll timeout\n");
            return 0.f;
        }
    }

    uint8_t range = 0;
    read_byte(VL6180X_REG_RESULT_RANGE_VAL, &range);

    VL6180_Clear_Interrupts();

    uint8_t range_status;
    read_byte(VL6180X_REG_RESULT_RANGE_STATUS, &range_status);
    range_status = range_status >> 4;
    if (range_status != 0)
    {
        LOG_ERROR("Range status: %d\n", range_status);
    }

    g_prev_output = kB0 * static_cast<float>(range) + g_prev_output * kA1;
    return g_prev_output;
}

bool init_vl6180x()
{
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    uint8_t rxdata;
    if (read_byte(VL6180X_REG_IDENTIFICATION_MODEL_ID, &rxdata) == false)
    {
        LOG_ERROR("VL6180X not found\n");
        return false;
    }

    LOG_INFO("VL6180X model ID: 0x%02x\n", rxdata);

    // fresh out of reset?
    if (read_byte(VL6180X_REG_SYSTEM_FRESH_OUT_OF_RESET, &rxdata) == false)
    {
        LOG_ERROR("Failed to read fresh out of reset\n");
        return false;
    }

    if (rxdata == 1)
    {
        LOG_INFO("VL6180X fresh out of reset\n");
        load_settings();
        rxdata = 0;
        if (write_byte(VL6180X_REG_SYSTEM_FRESH_OUT_OF_RESET, 0x00) == false)
        {
            LOG_ERROR("Failed to write fresh out of reset\n");
            return false;
        }
    }

    // start_range();
    // while (!poll_range())
    // {
    //     sleep_ms(1);
    // }
    // read_range();
    return true;
}

float vl6180_read()
{
    return read_range();
}