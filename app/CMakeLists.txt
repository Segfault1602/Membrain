add_executable(Membrain
    membrain.cpp
    logging.cpp
    leds.cpp
    usb_descriptors.cpp
    midi_controller.cpp
    vl6180.cpp
    cap_touch.cpp
    piezo_trigger.cpp)

pico_set_program_name(Membrain "Membrain")
pico_set_program_version(Membrain "0.1")

# Generate PIO header
pico_generate_pio_header(Membrain ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Membrain 0)
pico_enable_stdio_usb(Membrain 0)
pico_enable_stdio_rtt(Membrain 1)

# Add the standard library to the build
target_link_libraries(Membrain
        pico_stdlib
        pico_async_context_freertos
        FreeRTOS-Kernel-Heap4
        tinyusb_device
        tinyusb_board)

# Add the standard include files to the build
target_include_directories(Membrain PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/includes
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

# Add any user requested libraries
target_link_libraries(Membrain
        hardware_i2c
        hardware_pio
        hardware_adc
        )

pico_add_extra_outputs(Membrain)