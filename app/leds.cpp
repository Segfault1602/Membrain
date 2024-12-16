#include "pico/stdlib.h"
#include "pico/time.h"

#include "hardware/pio.h"
#include "ws2812.pio.h"

#include "FreeRTOS.h"
#include "task.h"

#include "leds.h"
#include "logging.h"

#define WS2812_PIN 15
#define IS_RGBW    false
#define NUM_PIXELS 8

namespace
{

struct PixelState
{
    uint32_t color = 0;
    bool is_blinking = false;
    bool blink_state = false;
    int repeat = 0;
    uint32_t period = 0;

    uint32_t _phase = 0;
};

PixelState g_pixels[NUM_PIXELS] = {0};
TaskHandle_t g_led_task_handle;

} // namespace

void pico_set_led(bool led_on)
{
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

static inline void put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline void put_pixels(PixelState* pixels, uint num_pixels)
{
    for (uint i = 0; i < num_pixels; ++i)
    {
        PixelState& pixel = pixels[i];
        if (pixel.is_blinking)
        {
            if (pixel._phase == 0)
            {
                pixel.blink_state = !pixel.blink_state;
            }
            if (pixel.blink_state)
            {
                put_pixel(pixel.color);
            }
            else
            {
                put_pixel(0);
            }
            pixel._phase = (pixel._phase + 1) % pixel.period;

            if (pixel._phase == 0)
            {
                if (pixel.repeat > 0)
                {
                    --pixel.repeat;
                }
                else if (pixel.repeat == 0)
                {
                    pixel.color = 0;
                    pixel.is_blinking = false;
                    pixel.blink_state = false;
                    pixel._phase = 0;
                }
            }
        }
        else
        {
            put_pixel(pixel.color);
        }
    }
    sleep_ms(1);
}

void pattern_snakes(uint len, uint t)
{
    for (uint i = 0; i < len; ++i)
    {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(urgb_to_u32(0xff, 0, 0));
        else if (x >= 15 && x < 25)
            put_pixel(urgb_to_u32(0, 0xff, 0));
        else if (x >= 30 && x < 40)
            put_pixel(urgb_to_u32(0, 0, 0xff));
        else
            put_pixel(0);
    }
}

uint32_t urgb_to_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

void all_off()
{
    for (uint i = 0; i < NUM_PIXELS; ++i)
    {
        put_pixel(0);
    }
    // sleep_ms(1);
    vTaskDelay(pdMS_TO_TICKS(1));
}

void update_leds()
{
    put_pixels(g_pixels, NUM_PIXELS);
    vTaskDelay(pdMS_TO_TICKS(1));
}

void set_led(Pixels pixel, uint32_t color)
{
    g_pixels[static_cast<uint32_t>(pixel)].color = color;
    g_pixels[static_cast<uint32_t>(pixel)].is_blinking = false;
    g_pixels[static_cast<uint32_t>(pixel)].blink_state = false;
    g_pixels[static_cast<uint32_t>(pixel)].period = 0;
    g_pixels[static_cast<uint32_t>(pixel)].repeat = 0;
    g_pixels[static_cast<uint32_t>(pixel)]._phase = 0;
    xTaskNotifyGive(g_led_task_handle);
}

void set_led_blinking(Pixels pixel, uint32_t color, uint32_t period, int repeat)
{
    g_pixels[static_cast<uint32_t>(pixel)].color = color;
    g_pixels[static_cast<uint32_t>(pixel)].is_blinking = true;
    g_pixels[static_cast<uint32_t>(pixel)].blink_state = false;
    g_pixels[static_cast<uint32_t>(pixel)].period = period;
    g_pixels[static_cast<uint32_t>(pixel)].repeat = repeat;
    g_pixels[static_cast<uint32_t>(pixel)]._phase = 0;
    xTaskNotifyGive(g_led_task_handle);
}

void start_led_task()
{
    auto result = xTaskCreate(led_task, "LedTask", LED_TASK_STACK_SIZE, NULL, LED_TASK_PRIORITY, &g_led_task_handle);
    LOG_INFO("Led task created\n");
    vTaskCoreAffinitySet(g_led_task_handle, (1 << 1));

    if (result != pdPASS)
    {
        LOG_ERROR("Failed to create led task\n");
    }
}

void led_task(void* params)
{
    LOG_INFO("Hello from led task\n");
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    static int last_core_id = -1;
    if (portGET_CORE_ID() != last_core_id)
    {
        last_core_id = portGET_CORE_ID();
        LOG_INFO("led task is on core %d\n", last_core_id);
    }

    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    // Show led pattern to show that we are booted and ready
    for (auto t = 0; t < 81; ++t)
    {
        pattern_snakes(NUM_PIXELS, t);
        sleep_ms(10);
    }

    all_off();

    const TickType_t xFrequency = pdMS_TO_TICKS(5);
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, xFrequency);
        update_leds();
    }
}