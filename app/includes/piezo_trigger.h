#pragma once

#include <cstdint>

class PiezoTrigger
{
  public:
    PiezoTrigger();

    void init(uint32_t gpio);

    bool triggered();
    bool get_state();

  private:
    uint32_t gpio_;
    bool prev_state_;
    bool state_;

    uint32_t last_trigger_;
};