#pragma once

#define LED_TASK_PRIORITY   (tskIDLE_PRIORITY + 2UL)
#define LED_TASK_STACK_SIZE 1024

// Colors are defined as GRB
#define DIM_GREEN (0x250000)
#define DIM_BLUE  (0x000025)

enum Pixels : uint32_t
{
    Power = 0,
    USB = 1,
    Midi = 2,
    Pixel_3 = 3,
    Pixel_4 = 4,
    Pixel_5 = 5,
    Pixel_6 = 6,
    Pixel_7 = 7
};

uint32_t urgb_to_u32(uint8_t r, uint8_t g, uint8_t b);

void start_led_task();
void set_led(Pixels pixel, uint32_t color);
void set_led_blinking(Pixels pixel, uint32_t color, uint32_t period, int repeat);
void led_task(void* params);
