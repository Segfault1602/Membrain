# Membrain

Membrain is a prototyping platform allowing the exploration of various sensors in the context of a drumhead or flexible membrane

## Building

The project uses the Raspberry Pi Pico VSCode extension to build and flash the firmware. The extension can be found [here](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico).

The extension uses CMake so presumably one could build the project using the command line as well but I have not tested it yet.

## Hardware

This firmware is designed to run on the Raspberry Pi Pico 2 and make use of the following external sensors:

- VL6180X Time-of-Flight sensor on Pin 4 and 5. ([available from Adafruit](https://www.adafruit.com/product/3316))
- 3 Linear Hall-effect sensors on Pin 31, 32 and 34 ([datasheet](https://www.allegromicro.com/-/media/files/datasheets/als31001-datasheet.pdf))
- NeoPixel RGB LED strip on Pin 15 ([available from Adafruit](https://www.adafruit.com/product/1426))
- 4 capacitive touch strip on Pin 21, 22, 24 and 25. Any conductive material can be used for this. I went with this conductive yarn [from Adafruit](https://www.adafruit.com/product/603).
- 1 Piezo sensor on Pin 19. [The particular sensor](https://abra-electronics.com/sensors/tilt-sensors/sens-vib-p-piezo-vibration-sensor-high-sensitivity-5v-sensor-module-for-arduino.html) I used came with a breakout board with a digital output which is why it is not connected to the ADC.