#include "freertos/FreeRTOS.h"  // Header file for the FreeRTOS operating system
#include "state_machines.h"     // Header file for both the usb state machine and the communication state machine

QueueHandle_t usb_to_com_queue; // FreeRTOS Queue that will be used to pass messages from the USB state machine to the communication state machine
QueueHandle_t com_to_usb_queue; // FreeRTOS Queue that will be used to pass messages from the communication state machine to the USB state machine

void app_main(void) {
    usb_to_com_queue = xQueueCreate(10, 9); // Initialise the queue to hold (number of messages, bytes per message)
    com_to_usb_queue = xQueueCreate(10, 9); // Initialise the queue to hold (number of messages, bytes per message)
    xTaskCreatePinnedToCore(usb_state_machine, "USB SM", 4096, NULL, 2, NULL, 0); // (Run the usb_state_machine function (declared in state_machines.h) as a FreeRTOS task, Name the task "USB SM", Allocate 4096 bytes of memory, Dont provide a pointer for any additional parameters, Set task priority to 2, Don't request a handle, Pin the task to core 0)
    xTaskCreatePinnedToCore(com_state_machine, "COM SM", 4096, NULL, 2, NULL, 1); // (Run the com_state_machine function (declared in state_machines.h) as a FreeRTOS task, Name the task "COM SM", Allocate 4096 bytes of memory, Dont provide a pointer for any additional parameters, Set task priority to 2, Don't request a handle, Pin the task to core 1)
}