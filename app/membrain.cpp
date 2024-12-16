#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include <random>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "tusb.h"

#include "cap_touch.h"
#include "leds.h"
#include "logging.h"
#include "midi_controller.h"
#include "vl6180.h"

#define MAIN_TASK_PRIORITY   (tskIDLE_PRIORITY + 2UL)
#define MAIN_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

void main_task(__unused void* params)
{
    int count = 0;
#if configNUMBER_OF_CORES > 1
    static int last_core_id = -1;
    if (portGET_CORE_ID() != last_core_id)
    {
        last_core_id = portGET_CORE_ID();
        LOG_INFO("main task is on core %d\n", last_core_id);
    }
#endif

    set_led(Pixels::Power, DIM_GREEN);

    while (true)
    {
        tud_task();

        taskYIELD();
    }
}

void vLaunch(void)
{
    TaskHandle_t task;
    xTaskCreate(main_task, "MainThread", MAIN_TASK_STACK_SIZE, NULL, MAIN_TASK_PRIORITY, &task);
    LOG_INFO("Main task created\n");

#if configUSE_CORE_AFFINITY && configNUMBER_OF_CORES > 1
    // we must bind the main task to one core (well at least while the init is called)
    vTaskCoreAffinitySet(task, (1 << 0));
#endif

    tusb_init();

    adc_init();

    init_vl6180x();
    init_cap_touch();

    start_led_task();
    start_midi_task();
    /* Start the tasks and timer running. */
    vTaskStartScheduler();
}

int main()
{
    // busy loop for 1ms

    // sleep_ms(1);
    stdio_init_all();
    logger::init_logging();

    LOG_INFO("Hello from Membrain!\n");
    sleep_ms(1);

    const char* rtos_name;
#if (configNUMBER_OF_CORES > 1)
    rtos_name = "FreeRTOS SMP";
#else
    rtos_name = "FreeRTOS";
#endif

    LOG_INFO("Starting %s on both cores:\n", rtos_name);

    vLaunch();

    while (1)
    {
        // Should never get here
    }

    return 0;
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    /* Check pcTaskName for the name of the offending task,
     * or pxCurrentTCB if pcTaskName has itself been corrupted. */
    (void)xTask;
    (void)pcTaskName;
    printf("Stack Overflow in task %s\n", pcTaskName);
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    LOG_INFO("USB mounted\r\n");
    set_led(Pixels::USB, DIM_GREEN);
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    LOG_INFO("USB unmounted\r\n");
    set_led(Pixels::USB, urgb_to_u32(0xff, 0, 0));
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    printf("USB suspended\r\n");
    (void)remote_wakeup_en;
    set_led(Pixels::USB, urgb_to_u32(0xCC, 0xAA, 0));
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    printf("USB resumed\r\n");
    set_led(Pixels::USB, DIM_GREEN);
}
