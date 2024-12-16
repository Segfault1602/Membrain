#include "cap_touch.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "pico/time.h"

#include "logging.h"

namespace
{
critical_section_t cs;

uint32_t kTouchThreshold = 50000;
constexpr uint32_t kCalibrateTime = 166;
} // namespace

void init_cap_touch()
{
    critical_section_init(&cs);
}

CapPin::CapPin()
    : gpio_(0), samples_(0), total_(0), baseline_count_(0), last_samples_(0), calibrate_flag_(false),
      prev_state_(false), state_(false)
{
}

void CapPin::init(uint32_t gpio, uint32_t samples)
{
    gpio_ = gpio;
    samples_ = samples;
    gpio_init(gpio_);
    gpio_set_dir(gpio_, GPIO_IN);
    gpio_set_pulls(gpio_, true, false);
}

void CapPin::calibrate_pin()
{
    uint16_t j, k = 0;
    gpio_set_dir(gpio_, GPIO_OUT);

    // the idea here is to calibrate for the same number of samples that are specified
    // but to make sure that the value is over a certain number of powerline cycles to
    // average out powerline errors

    unsigned long start = to_ms_since_boot(get_absolute_time());
    gpio_put(gpio_, 0); // set sensorPin output register LOW

    while (to_ms_since_boot(get_absolute_time()) - start < kCalibrateTime)
    { // sample at least 10 power line cycles

        for (unsigned int i = 0; i < samples_; i++)
        { // loop for samples parameter

            gpio_set_dir(gpio_, GPIO_IN); // set sensorPin to INPUT
            gpio_put(gpio_, 1);           // set sensorPin output register HIGH to set pullups
            gpio_set_pulls(gpio_, true, false);

            for (j = 0; j < 500; j++)
            {
                if (gpio_get(gpio_))
                    break;
            }

            total_ += j;

            gpio_put(gpio_, 0);            // Pin output register LOW
            gpio_set_dir(gpio_, GPIO_OUT); // OUTPUT & LOW now
        }
        k++;
    }

    // if pin is grounded (or connected with resistance lower than the pullup resistors,
    // the method will return in about one second.
    if (total_ >= ((unsigned long)samples_ * 450UL * (unsigned long)k))
    {
        LOG_INFO("calibratePin method over timeout, check wiring.\n");
    }
    else
    {
        baseline_count_ = total_ / k;
        LOG_INFO("Calibrated baselineCount = ");
        LOG_INFO("%lu\n", baseline_count_);
    }
}

bool CapPin::read_pin()
{
    critical_section_enter_blocking(&cs);

    gpio_set_dir(gpio_, GPIO_IN);
    gpio_put(gpio_, 1);

    critical_section_exit(&cs);

    uint16_t j;
    total_ = 0;

    gpio_put(gpio_, 1);

    //	if (samples < 1) return 0;	  // defensive programming

    // calibrate first time through after reset or cycling power
    if (calibrate_flag_ == 0 || samples_ != last_samples_)
    {
        calibrate_pin();
        last_samples_ = samples_;
        calibrate_flag_ = 1;
    }

    // I'm using a for loop here instead of while. It adds a timeout automatically
    // the idea is from here http://www.arduino.cc/playground/Code/CapacitiveSensor
    for (unsigned int i = 0; i < samples_; i++)
    { // loop for samples parameter

        critical_section_enter_blocking(&cs);
        gpio_set_dir(gpio_, GPIO_IN); // set sensorPin to INPUT
        gpio_put(gpio_, 1);           // set sensorPin output register HIGH to set pullups
        critical_section_exit(&cs);

        for (j = 0; j < 5000; j++)
        {
            if (gpio_get(gpio_))
                break;
        }

        total_ += j;

        critical_section_enter_blocking(&cs);
        gpio_put(gpio_, 0); // Pin output register LOW
        gpio_set_dir(gpio_, GPIO_OUT);
        critical_section_exit(&cs);
    }

    // if pin is grounded (or connected to ground with resistance
    // lower than the pullup resistors,
    // the method will return in about one second.
    if (total_ >= ((unsigned long)samples_ * 450UL))
    {
        LOG_ERROR("readPin method over timeout, check wiring.\n");
        state_ = false;
    }

    // Serial.println(total);

    return ((total_ - baseline_count_) > kTouchThreshold);
}

bool CapPin::triggered()
{
    prev_state_ = state_;
    state_ = read_pin();

    if (state_ && !prev_state_)
    {
        LOG_INFO("total_ = %lu\n", total_);
    }

    return state_ && !prev_state_;
}

bool CapPin::get_state()
{
    return state_;
}