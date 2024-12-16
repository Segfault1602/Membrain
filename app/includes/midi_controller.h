#pragma once

#define USB_MIDI_TASK_PRIORITY   (tskIDLE_PRIORITY + 2UL)
#define USB_MIDI_TASK_STACK_SIZE 1024

void start_midi_task();