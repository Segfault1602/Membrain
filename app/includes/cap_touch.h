#pragma once

#include <cstdint>

void init_cap_touch();
void calibrate_pin(unsigned int samples);
int read_touch(unsigned int samples);

class CapPin
{
  public:
    CapPin();

    void init(uint32_t gpio, uint32_t samples);

    bool read_pin();

    bool triggered();
    bool get_state();

    void calibrate_pin();

  private:
    uint32_t gpio_;
    uint32_t samples_;
    uint32_t total_;
    uint32_t calibrate_time_;
    uint32_t baseline_count_;
    uint32_t last_samples_;
    bool calibrate_flag_;
    bool prev_state_;
    bool state_;
};