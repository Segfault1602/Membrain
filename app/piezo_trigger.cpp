#include "piezo_trigger.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "logging.h"

PiezoTrigger::PiezoTrigger() : gpio_(0), prev_state_(false), state_(false)
{
}

void PiezoTrigger::init(uint32_t gpio)
{
    gpio_ = gpio;
    gpio_init(gpio_);
    gpio_set_dir(gpio_, GPIO_IN);

    // Errata 9: need external pulldown for rp2350
    gpio_set_pulls(gpio_, false, false);
}

bool PiezoTrigger::triggered()
{
    auto now = to_ms_since_boot(get_absolute_time());

    if (now - last_trigger_ < 10)
    {
        return false;
    }

    prev_state_ = state_;
    state_ = gpio_get(gpio_);
    if (state_ && !prev_state_)
    {
        LOG_DEBUG("Piezo triggered\n");
        last_trigger_ = now;
        return true;
    }

    return false;
}