#include <stdint.h>             // Header file to declare integer types (needed for uint8_t)
#include "freertos/FreeRTOS.h"  // Header file for the FreeRTOS operating system (needed for QueueHandle_t)
#include "esp_timer.h"


enum headers {          // Define all the headers for messages between state machines and between modules 
    NO_HEADER,          // Used as a placeholder header when no header has been received
    ERROR,              // Used when a header has been received but the error correction detects a random bit error and the header cant be identified. (Needed?)
    HELLO,
    HEARD,
    ACK,
    STATE,              // Used to check for pre-existing state, useful if there was a communication disconnect
    UPDATE,
    REPORT_MOUSE,
    REPORT_KEYBOARD
};

enum updates {          // Define all the message types following an update header 
    HOST_CONNECTED,
    HOST_DISCONNECTED,
    MOUSE_CONNECTED,
    KEYBOARD_CONNECTED,
    DATASTICK_CONNECTED,
    DEVICE_DISCONNECTED
};

enum device { // perhaps use just USBState, or maybe use host and device as separate
    NONE,
    MOUSE,
    KEYBOARD,
    DATASTICK
};

enum USBstate {             // Define all the states of the USB state machine
    UNKNOWN,
    DEVICE_UNKNOWN,
    DEVICE_DATASTICK,
    DEVICE_KEYBOARD,
    DEVICE_MOUSE,
    HOST_UNKNOWN,
    HOST_DATASTICK,
    HOST_KEYBOARD,
    HOST_MOUSE
};

extern volatile uint8_t usb_state;      // Defined in state_machine_usb.c

extern QueueHandle_t usb_to_com_queue;  // Defined in main.c
extern QueueHandle_t com_to_usb_queue;  // Defined in main.c

extern int64_t start_time;
extern int64_t end_time;
extern int64_t duration;

void usb_state_machine(void *arg);      // Defined in state_machine_usb.c
void com_state_machine(void *arg);      // Defined in state_machine_com.c