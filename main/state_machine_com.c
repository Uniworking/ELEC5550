#include "state_machines.h"
#include "esp_log.h"
#include "esp_random.h"
#include "driver/uart.h"

#include "Tools/UARTTools.h"

static const char *TAG = "COM SM";    // Tag used for ESP logging

#define UART_PORT UART_NUM_1          // Define which UART port to use
#define BAUD_RATE 1000000             // Define the baud rate for UART communication
#define HB_PERIOD 1000                // Heartbeat period in milliseconds
#define MIN_BACKOFF_MS   100          // Minimum backoff in milliseconds
#define MAX_BACKOFF_MS   1000         // Maximum backoff in milliseconds

static uint8_t header;                // Variable to hold the received header
static uint8_t message[9];            // Buffer to hold messages (max size set by keyboard report)

enum COM_STATE {                      // Define all the states of the communication state machine
    BACKOFF,                             // Backoff state attempts to estabblish half-duplex communication
    READ,                                // Read state waits for incoming messages
    WRITE                                // Write state sends outgoing messages
};

void com_state_machine(void *arg) {   // Communication state machine function
    uint8_t com_state = BACKOFF;      // Initialise the communication state to BACKOFF
    uart_init(BAUD_RATE);             // Initialise UART drivers with defined baud rate
    while(1) {
        switch (com_state) {          // Switch statement defining behaviour for the different communication states
            // -------------------------------- BACKOFF STATE --------------------------------
            case BACKOFF:
                vTaskDelay(pdMS_TO_TICKS(10));       // delay and flush to avoid reading reflected signal
                uart_flush(UART_PORT);
                uint32_t backoff = MIN_BACKOFF_MS + (esp_random() % (MAX_BACKOFF_MS - MIN_BACKOFF_MS)); // Generate backoff time
                ESP_LOGW(TAG, "Reading for %d ms.", backoff);
                header = read_header(backoff);        // Attempt to read a header with timeout defined by the backoff time
                if (header == HELLO) {                // HELLO header received
                    ESP_LOGW(TAG, "Received HELLO, transmitting HEARD and updating state to READ.");
                    vTaskDelay(pdMS_TO_TICKS(15));    // delay to wait for other side to flush
                    send_header((uint8_t)HEARD);      // Transmit HEARD header
                    com_state = READ;                 // Update communication state to READ
                } else if (header == HEARD) {         // HEARD header received
                    ESP_LOGW(TAG, "Received HEARD, transmitting STATE and updating state to READ.");
                    message[0] = (uint8_t)STATE;      // Prepare STATE message
                    message[1] = usb_state;
                    vTaskDelay(pdMS_TO_TICKS(15));    // delay to wait for other side to flush
                    send_data(message, 2);            // Transmit STATE message
                    com_state = READ;                 // Update communication state to READ
                } else if (header == NO_HEADER) {     // No header received
                    ESP_LOGW(TAG, "No header received, transmitting HELLO.");
                    vTaskDelay(pdMS_TO_TICKS(15));    // delay to wait for other side to flush
                    send_header((uint8_t)HELLO);      // Transmit HELLO header
                } else if (header == ERROR) {         // ERROR header received
                    ESP_LOGW(TAG, "Error header received, transmitting HELLO.");
                    vTaskDelay(pdMS_TO_TICKS(15));    // delay to wait for other side to flush
                    send_header((uint8_t)HELLO);      // Transmit HELLO header
                }
                break;
            // -------------------------------- WRITE STATE --------------------------------
            case WRITE:
                BaseType_t QueueFlag = pdFAIL; // Flag to check if a message was received from the usb state machine
                switch (usb_state) {
                    case UNKNOWN:              // In unknown or host states, wait for up to the
                    case HOST_UNKNOWN:         // heartbeat period to receive a message from 
                    case HOST_DATASTICK:       // the usb state machine
                    case HOST_KEYBOARD:
                    case HOST_MOUSE:           
                        QueueFlag = xQueueReceive(usb_to_com_queue, &message, pdMS_TO_TICKS(HB_PERIOD));
                        break;
                    case DEVICE_UNKNOWN:       // In device states, check but do not 
                    case DEVICE_DATASTICK:     // wait to receive a message from 
                    case DEVICE_KEYBOARD:      // the usb state machine
                    case DEVICE_MOUSE:
                        QueueFlag = xQueueReceive(usb_to_com_queue, &message, 0);
                        break;
                }
                if (QueueFlag == pdPASS) {     // If a message was received from the usb state machine
                    if (message[0] == UPDATE) {                  // If the message is an update
                        ESP_LOGW(TAG, "Received an update from the usb state machine, transmitting it");
                        send_data(message, 2); // Transmit full update (1 header + 1 data byte)
                    } else if (message[0] == REPORT_MOUSE) {     // If the message is a mouse report
                        send_data(message, 5); // Transmit full mouse report (1 header + 4 data bytes)
                    } else if (message[0] == REPORT_KEYBOARD) {  // If the message is a keyboard report
                        send_data(message, 9); // Transmit full keyboard report (1 header + 8 data bytes)
                    }
                } else {                       // If no message was received from the usb state machine
                    send_header((uint8_t)ACK); // Transmit ACK header as a heartbeat
                }
                com_state = READ;              // Update communication state to READ
                break;
            // -------------------------------- READ STATE --------------------------------
            case READ:
                uart_flush(UART_PORT);                      // Flush UART to avoid reading reflected signal
                message[0] = read_header(2*HB_PERIOD);      // Attempt to read a header with timeout defined by twice the heartbeat period
                if (message[0] == ACK) {                    // If an ACK header is received
                    com_state = WRITE;                      // Update communication state to WRITE  
                } else if (message[0] == UPDATE) {          // If an UPDATE header is received
                    ESP_LOGW(TAG, "Received UPDATE, sending to USB state machine and updating comm state to WRITE.");
                    read_data(&message[1], 1, 10);          // Read the rest of the update message (1 data byte)
                    xQueueSend(com_to_usb_queue, message, portMAX_DELAY); // Send the full update message to the usb state machine
                    com_state = WRITE;                      // Update communication state to WRITE
                }  else if (message[0] == REPORT_MOUSE) {   // If a REPORT_MOUSE header is received
                    read_data(&message[1], 4, 10);          // Read the rest of the mouse report message (4 data bytes)
                    xQueueSend(com_to_usb_queue, message, portMAX_DELAY); // Send the full mouse report message to the usb state machine
                    com_state = WRITE;                      // Update communication state to WRITE
                }  else if (message[0] == REPORT_KEYBOARD) {// If a REPORT_KEYBOARD header is received
                    read_data(&message[1], 8, 10);          // Read the rest of the keyboard report message (8 data bytes)
                    xQueueSend(com_to_usb_queue, message, portMAX_DELAY); // Send the full keyboard report message to the usb state machine
                    com_state = WRITE;                      // Update communication state to WRITE
                }  else if (message[0] == STATE) {          // If a STATE header is received
                    ESP_LOGW(TAG, "Received STATE, comparing with own state and deciding what to do.");
                    read_data(&message[1], 1, 10);          // Read the rest of the state message (1 data byte)
                    uint8_t desired_state = UNKNOWN;
                    switch (usb_state) {                    // Map own usb state to desired state of other device
                        case UNKNOWN:           desired_state = UNKNOWN;            break;
                        case DEVICE_UNKNOWN:    desired_state = HOST_UNKNOWN;       break;
                        case DEVICE_DATASTICK:  desired_state = HOST_DATASTICK;     break;
                        case DEVICE_KEYBOARD:   desired_state = HOST_KEYBOARD;      break;
                        case DEVICE_MOUSE:      desired_state = HOST_MOUSE;         break;
                        case HOST_UNKNOWN:      desired_state = DEVICE_UNKNOWN;     break;
                        case HOST_DATASTICK:    desired_state = DEVICE_DATASTICK;   break;
                        case HOST_KEYBOARD:     desired_state = DEVICE_KEYBOARD;    break;
                        case HOST_MOUSE:        desired_state = DEVICE_MOUSE;       break;
                    }
                    if (message[1] == desired_state) {      // Compare received state with what own usb state desires
                        ESP_LOGW(TAG, "States match, moving on.");
                        com_state = WRITE;                  // If they match, update communication state to WRITE
                    } else {                                // If states do not match, return a state message and abort to UNKNOWN
                        ESP_LOGW(TAG, "States do not match, aborting to UNKNOWN.");
                        message[0] = (uint8_t)STATE;        // Prepare STATE message
                        message[1] = usb_state;
                        send_data(message, 2);              // Transmit STATE message
                        xQueueSend(com_to_usb_queue, message, portMAX_DELAY); // Send message to usb state machine to return to UNKNOWN        
                    }
                } else {                                    // If an unexpected or no header is received, return to BACKOFF
                    ESP_LOGW(TAG, "Timeout or received an unexpected header, returning state to BACKOFF.");
                    com_state = BACKOFF;
                }
                break;
        }
    }
}