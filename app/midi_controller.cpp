#include "midi_controller.h"

#include "hardware/adc.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "FreeRTOS.h"
#include "task.h"
#include "tusb.h"

#include <algorithm>

#include "cap_touch.h"
#include "leds.h"
#include "logging.h"
#include "piezo_trigger.h"
#include "vl6180.h"

namespace
{
TaskHandle_t g_midi_task_handle;

struct NoteTrigger
{
    CapPin pin;
    uint8_t note;
    uint8_t velocity;
    bool state;
    Pixels led;
};

// Constants
constexpr size_t kNumTouchPins = 4;
constexpr uint8_t g_touchGpios[kNumTouchPins] = {16, 17, 18, 19};
constexpr Pixels g_touchPixels[kNumTouchPins] = {Pixels::Pixel_3, Pixels::Pixel_4, Pixels::Pixel_5, Pixels::Pixel_6};
constexpr uint8_t kStartNote = 37;

constexpr float kHallA1 = 0.80;
constexpr float kHallB0 = 1.f - std::abs(kHallA1);
constexpr float kAdcNormalizationFactor = 1.f / 2048.f;

constexpr uint8_t kPiezoGpio = 14;
constexpr uint32_t kPiezoGateTime = 10;

constexpr uint16_t kMaxPitchBend = 8191;
constexpr float kPitchBendHysteresis = 0.01f;

constexpr float kVl6120MinRange = 5.0f;
constexpr float kVl6120MaxRange = 17.0f;
constexpr float kVl6120RangeScale = 1.f / (kVl6120MaxRange - kVl6120MinRange);
constexpr uint32_t kVl6180FreqMs = 100;
// ----------------

// Variables
NoteTrigger g_touch[kNumTouchPins];

PiezoTrigger g_piezo;
uint32_t g_piezo_last_trigger = 0;
bool g_piezo_note_on = false;

float g_prev_hall_output = 0.f;
float g_last_hall_value_sent = 0.f;

uint32_t g_vl6180_last_read = 0;
uint8_t g_vl6180_last_cc = 0;
// ----------------

void handle_vl6180()
{
    auto now = to_ms_since_boot(get_absolute_time());
    if (now - g_vl6180_last_read < kVl6180FreqMs)
    {
        return;
    }
    g_vl6180_last_read = now;
    float raw_range = vl6180_read();
    float range = std::clamp(raw_range, kVl6120MinRange, kVl6120MaxRange);
    range = 1.f - (range - kVl6120MinRange) * kVl6120RangeScale;
    // LOG_INFO("Range: %f, raw: %f\n", range, raw_range);

    uint8_t msg[3];
    uint8_t midi_val = range * 127;
    if (midi_val != g_vl6180_last_cc)
    {
        msg[0] = 0xB1;     // CC message - Channel 1
        msg[1] = 21;       // CC Number
        msg[2] = midi_val; // cc value
        tud_midi_n_stream_write(0, 0, msg, 3);
        g_vl6180_last_cc = midi_val;
    }
}

void handle_pitch_bend()
{
    uint8_t msg[3];

    adc_select_input(0);
    float hall1 = (adc_read() - 2048.f) * kAdcNormalizationFactor;
    adc_select_input(1);
    float hall2 = (adc_read() - 2048.f) * kAdcNormalizationFactor;
    adc_select_input(2);
    float hall3 = (adc_read() - 2048.f) * kAdcNormalizationFactor;

    float hall_value = hall3 + 2.f * hall2 + 3.f * hall1;
    hall_value /= 6.f;
    hall_value = kHallB0 * hall_value + g_prev_hall_output * kHallA1;
    g_prev_hall_output = hall_value;

    bool send_cc = std::abs(g_prev_hall_output - g_last_hall_value_sent) > kPitchBendHysteresis;

    if (g_prev_hall_output > 0.08f && send_cc)
    {
        g_last_hall_value_sent = g_prev_hall_output;

        uint16_t pitch_bend = 8192 + kMaxPitchBend * g_prev_hall_output;
        assert(pitch_bend <= 16383);

        // send pitchbend message
        msg[0] = 0xE0; // Pitch Bend - Channel 1
        msg[1] = pitch_bend & 0x7F;
        msg[2] = (pitch_bend >> 7) & 0x7F;
        tud_midi_n_stream_write(0, 0, msg, 3);
    }
}

void handle_piezo_trigger()
{
    auto now = to_ms_since_boot(get_absolute_time());
    uint8_t msg[3];

    if (g_piezo.triggered())
    {
        if (g_piezo_note_on)
        {
            msg[0] = 0x80; // Note Off - Channel 1
            msg[1] = 36;   // Note Number
            msg[2] = 0;    // Velocity
            tud_midi_n_stream_write(0, 0, msg, 3);
            g_piezo_note_on = false;
        }
        set_led_blinking(Pixels::Midi, DIM_BLUE, 10, 1);

        msg[0] = 0x90; // Note On - Channel 1
        msg[1] = 36;   // Note Number
        msg[2] = 127;  // Velocity
        tud_midi_n_stream_write(0, 0, msg, 3);
        g_piezo_last_trigger = now;
        g_piezo_note_on = true;
    }

    if (g_piezo_note_on && (now - g_piezo_last_trigger) > kPiezoGateTime)
    {
        msg[0] = 0x80; // Note Off - Channel 1
        msg[1] = 36;   // Note Number
        msg[2] = 0;    // Velocity
        tud_midi_n_stream_write(0, 0, msg, 3);
        g_piezo_note_on = false;
    }
}

void handle_touch_pad()
{
    uint8_t msg[3];

    for (size_t i = 0; i < kNumTouchPins; i++)
    {
        auto& touch = g_touch[i];

        if (touch.pin.triggered())
        {
            touch.state = true;
            set_led(touch.led, DIM_BLUE);
            if (g_touchGpios[i] == 19)
            {
                msg[0] = 0xB1; // CC message - Channel 1
                msg[1] = 20;   // CC Number
                msg[2] = 127;  // cc value
                tud_midi_n_stream_write(0, 0, msg, 3);
            }
            else
            {
                LOG_INFO("Touch detected for note %d\n", touch.note);
                msg[0] = 0x90;       // Note On - Channel 1
                msg[1] = touch.note; // Note Number
                msg[2] = 127;        // Velocity
                tud_midi_n_stream_write(0, 0, msg, 3);
            }
        }
        if (touch.state && !touch.pin.get_state())
        {
            set_led(touch.led, 0);
            touch.state = false;
            if (g_touchGpios[i] == 19)
            {
                msg[0] = 0xB1; // CC message - Channel 1
                msg[1] = 20;   // CC Number
                msg[2] = 0;    // cc value
                tud_midi_n_stream_write(0, 0, msg, 3);
            }
            else
            {
                msg[0] = 0x80;       // Note Off - Channel 1
                msg[1] = touch.note; // Note Number
                msg[2] = 0;          // Velocity
                tud_midi_n_stream_write(0, 0, msg, 3);
                LOG_INFO("Touch released for note %d\n", touch.note);
            }
        }
    }
}

void midi_task(void)
{
    // float range = vl6180_read();
    // constexpr float kMinRange = 9.0f;
    // constexpr float kMaxRange = 23.0f;
    // range = std::clamp(range, kMinRange, kMaxRange);

    handle_pitch_bend();

    handle_piezo_trigger();

    handle_touch_pad();

    handle_vl6180();
}
} // namespace

void usb_midi_task(void* params)
{
    LOG_INFO("Hello from USB MIDI task\n");

    uint32_t max_time = 0;
    uint32_t min_time = 0xffffffff;
    uint32_t avg_time = 0;
    uint32_t count = 0;

    while (true)
    {
        auto now = to_ms_since_boot(get_absolute_time());
        midi_task();
        auto elapsed = to_ms_since_boot(get_absolute_time()) - now;

        if (elapsed > max_time)
        {
            max_time = elapsed;
        }
        if (elapsed < min_time)
        {
            min_time = elapsed;
        }
        avg_time += elapsed;
        count++;

        if (count > 1000)
        {
            LOG_INFO("USB MIDI task max time: %d\n", max_time);
            LOG_INFO("USB MIDI task min time: %d\n", min_time);
            LOG_INFO("USB MIDI task avg time: %d\n", avg_time / count);
            max_time = 0;
            min_time = 0xffffffff;
            avg_time = 0;
            count = 0;
        }

        taskYIELD();
    }
}

void start_midi_task()
{
    for (size_t i = 0; i < kNumTouchPins; i++)
    {
        g_touch[i].pin.init(g_touchGpios[i], 2000);
        g_touch[i].pin.calibrate_pin();
        g_touch[i].note = kStartNote + i;
        g_touch[i].velocity = 127;
        g_touch[i].state = false;
        g_touch[i].led = g_touchPixels[i];
    }

    g_piezo.init(kPiezoGpio);

    adc_gpio_init(26);
    adc_gpio_init(27);
    adc_gpio_init(28);

    auto result = xTaskCreate(usb_midi_task, "UsbMidiTask", USB_MIDI_TASK_STACK_SIZE, NULL, USB_MIDI_TASK_PRIORITY,
                              &g_midi_task_handle);
    LOG_INFO("USB MIDI task created\n");
    vTaskCoreAffinitySet(g_midi_task_handle, (1 << 0));

    if (result != pdPASS)
    {
        LOG_ERROR("Failed to create USB MIDI task\n");
    }
}