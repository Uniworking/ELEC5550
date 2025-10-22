#include "state_machines.h"
#include "esp_log.h"

#include "Tools/USBDeviceTools.h"
#include "Tools/USBHostTools.h"

static const char *TAG = "USB SM";                  // Tag used for ESP logging

extern volatile uint8_t usb_state = UNKNOWN;        // Variable shared with communication state machine to hold current usb state

void usb_state_machine(void *arg) {                 // USB state machine function
    ESP_LOGI(TAG, "Initialising usb state machine");
    uint8_t header = NO_HEADER;                     // Variable to hold received header
    uint8_t received_data[10] = {0};                // Buffer to hold received messages (1 header + 9 data bytes)
    uint8_t transmit_data[10] = {0};                // Buffer to hold messages to be transmitted (1 header + 9 data bytes)
    uint8_t wait_time = 0;                          // Variable wait time to prevent watchdog timer triggering
    enumerate_as_keyboard();                        // Temporary method to detect host, want to use phy but cant get it working
    while (1) {
        if (xQueueReceive(com_to_usb_queue, &received_data, wait_time) == pdPASS) {
            header = received_data[0];              // Extract header from received message
        } else {
            header = NO_HEADER;                     // If no message received set header to NO_HEADER
        }
        switch (usb_state) {
            case UNKNOWN:
                // -------------------------------- CHECK EXIT CONDITIONS --------------------------------
                if (header == UPDATE) {         // Receive an update that the other device has detected a host, thus this device is hosting a device.
                    if (received_data[1] == HOST_CONNECTED) {
                        disconnect_device();    // Uninstall device drivers
                        usb_state = HOST_UNKNOWN;
                        ESP_LOGI(TAG, "Beginning host behaviour.");
                        host_install();         // Install host drivers
                    }
                }
                if (detect_host()) {            // Detect a host
                    ESP_LOGI(TAG, "Detected a host, informing the com state machine.");
                    transmit_data[0] = UPDATE;
                    transmit_data[1] = HOST_CONNECTED;
                    xQueueSend(usb_to_com_queue, transmit_data, portMAX_DELAY);
                    usb_state = DEVICE_UNKNOWN;
                    wait_time = 10;
                }
                // -------------------------------- STATE BEHAVIOUR --------------------------------
                vTaskDelay(pdMS_TO_TICKS(10));  // Small delay to prevent watchdog timer triggering
                break;

            case DEVICE_UNKNOWN:
                // -------------------------------- CHECK EXIT CONDITIONS --------------------------------
                if (header == UPDATE) {         // Receive an update that the other device has detected a device connection
                    disconnect_device();
                    if (received_data[1] == MOUSE_CONNECTED) {
                        usb_state = DEVICE_MOUSE;
                        ESP_LOGI(TAG, "Beginning mouse behaviour.");
                        enumerate_as_mouse();
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    } else if (received_data[1] == KEYBOARD_CONNECTED) {
                        usb_state = DEVICE_KEYBOARD;
                        ESP_LOGI(TAG, "Beginning keyboard behaviour.");
                        enumerate_as_keyboard();
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    } else if (received_data[1] == DATASTICK_CONNECTED) {
                        usb_state = DEVICE_DATASTICK;
                        ESP_LOGI(TAG, "Beginning datastick behaviour.");
                        // enumerate as data stick
                    }
                } else if (!(detect_host())) { // Host disconnected
                    usb_state = UNKNOWN;
                    ESP_LOGI(TAG, "Host disconnected.");
                    transmit_data[0] = UPDATE;
                    transmit_data[1] = HOST_DISCONNECTED;
                    xQueueSend(usb_to_com_queue, transmit_data, portMAX_DELAY);
                }
                // -------------------------------- STATE BEHAVIOUR --------------------------------
                break;

            case DEVICE_DATASTICK:
                // -------------------------------- CHECK EXIT CONDITIONS --------------------------------
                if (header == UPDATE) {             // Receive an update that the other device has detected a device disconnection
                    if (received_data[1] == DEVICE_DISCONNECTED) {
                        disconnect_device();        // Uninstall device drivers
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        usb_state = DEVICE_UNKNOWN;
                        enumerate_as_keyboard();    // Temporary method to detect host, want to use phy but cant get it working
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                } else if (!(detect_host())) { // Host disconnected
                    disconnect_device();
                    usb_state = UNKNOWN;
                    ESP_LOGI(TAG, "Host disconnected.");
                    transmit_data[0] = UPDATE;
                    transmit_data[1] = HOST_DISCONNECTED;
                    xQueueSend(usb_to_com_queue, transmit_data, portMAX_DELAY);
                    enumerate_as_keyboard(); // Temporary method to detect host, want to use phy but cant get it working
                }
                // -------------------------------- STATE BEHAVIOUR --------------------------------
                break;
            case DEVICE_KEYBOARD:
                // -------------------------------- STATE BEHAVIOUR --------------------------------
                if (header == REPORT_KEYBOARD) {
                    send_keyboard_report_to_computer((usb_keyboard_report_t *) &received_data[1]);
                }
                // -------------------------------- CHECK EXIT CONDITIONS --------------------------------
                if (header == UPDATE) {
                    if (received_data[1] == DEVICE_DISCONNECTED) {
                        disconnect_device();
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        usb_state = DEVICE_UNKNOWN;
                        enumerate_as_keyboard(); // Temporary method to detect host, want to use phy but cant get it working
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                } else if (!(detect_host())) { // Host disconnected
                    disconnect_device();
                    usb_state = UNKNOWN;
                    ESP_LOGI(TAG, "Host disconnected.");
                    transmit_data[0] = UPDATE;
                    transmit_data[1] = HOST_DISCONNECTED;
                    xQueueSend(usb_to_com_queue, transmit_data, portMAX_DELAY);
                    enumerate_as_keyboard(); // Temporary method to detect host, want to use phy but cant get it working
                }
                break;
            case DEVICE_MOUSE:
                // -------------------------------- STATE BEHAVIOUR --------------------------------
                if (header == REPORT_MOUSE) {
                    //ESP_LOGI(TAG, "Received a mouse report.");
                    send_mouse_report_to_computer((usb_mouse_report_t *) &received_data[1]);
                }
                // -------------------------------- CHECK EXIT CONDITIONS --------------------------------
                if (header == UPDATE) {
                    if (received_data[1] == DEVICE_DISCONNECTED) {
                        ESP_LOGI(TAG, "Received an update, uninstalling mouse drivers.");
                        disconnect_device();
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        usb_state = DEVICE_UNKNOWN;
                        enumerate_as_keyboard(); // Temporary method to detect host, want to use phy but cant get it working
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                } else if (!(detect_host())) { // Host disconnected
                    disconnect_device();
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    usb_state = UNKNOWN;
                    ESP_LOGI(TAG, "Host disconnected.");
                    transmit_data[0] = UPDATE;
                    transmit_data[1] = HOST_DISCONNECTED;
                    xQueueSend(usb_to_com_queue, transmit_data, portMAX_DELAY);
                    enumerate_as_keyboard(); // Temporary method to detect host, want to use phy but cant get it working
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                
                break;
            case HOST_UNKNOWN:
                // -------------------------------- CHECK EXIT CONDITIONS --------------------------------
                if (detect_device() == NONE) {
                    // Stay in HOST_UNKNOWN
                } else if (detect_device() == MOUSE) {
                    ESP_LOGI(TAG, "Mouse detected.");
                    usb_state = HOST_MOUSE;
                    transmit_data[0] = UPDATE;
                    transmit_data[1] = MOUSE_CONNECTED;
                    xQueueSend(usb_to_com_queue, transmit_data, portMAX_DELAY);
                } else if (detect_device() == KEYBOARD) {
                    ESP_LOGI(TAG, "Keyboard detected.");
                    usb_state = HOST_KEYBOARD;
                    transmit_data[0] = UPDATE;
                    transmit_data[1] = KEYBOARD_CONNECTED;
                    xQueueSend(usb_to_com_queue, transmit_data, portMAX_DELAY);
                } else if (detect_device() == DATASTICK) {
                    ESP_LOGI(TAG, "Datastick detected.");
                    usb_state = HOST_DATASTICK;
                    transmit_data[0] = UPDATE;
                    transmit_data[1] = DATASTICK_CONNECTED;
                    xQueueSend(usb_to_com_queue, transmit_data, portMAX_DELAY);
                }
                if (header == UPDATE) { // Receive an update that the other device has detected a host disconnection, thus this device no longer needs to host a device
                    if (received_data[1] == HOST_DISCONNECTED) {
                        host_uninstall();
                        usb_state = UNKNOWN;
                        enumerate_as_keyboard(); // Temporary method to detect host, want to use phy but cant get it working
                    }
                }
                // -------------------------------- STATE BEHAVIOUR --------------------------------
                handle_hosting();
                break;
            case HOST_DATASTICK:
                // -------------------------------- CHECK EXIT CONDITIONS --------------------------------
                if (detect_device() == NONE) {
                    usb_state = HOST_UNKNOWN;
                }
                if (header == UPDATE) { // Receive an update that the other device has detected a host disconnection, thus this device no longer needs to host a device
                    if (received_data[1] == HOST_DISCONNECTED) {
                        host_uninstall();
                        usb_state = UNKNOWN;
                        enumerate_as_keyboard(); // Temporary method to detect host, want to use phy but cant get it working
                    }
                }
                // -------------------------------- STATE BEHAVIOUR --------------------------------
                handle_hosting();
                break;
            case HOST_KEYBOARD:
                // -------------------------------- CHECK EXIT CONDITIONS --------------------------------
                if (detect_device() == NONE) {
                    usb_state = HOST_UNKNOWN;
                    ESP_LOGI(TAG, "Keyboard disconnected.");
                    transmit_data[0] = UPDATE;
                    transmit_data[1] = DEVICE_DISCONNECTED;
                    xQueueSend(usb_to_com_queue, transmit_data, portMAX_DELAY);
                }
                if (header == UPDATE) { // Receive an update that the other device has detected a host disconnection, thus this device no longer needs to host a device
                    if (received_data[1] == HOST_DISCONNECTED) {
                        host_uninstall();
                        usb_state = UNKNOWN;
                        enumerate_as_keyboard(); // Temporary method to detect host, want to use phy but cant get it working
                    }
                }
                // -------------------------------- STATE BEHAVIOUR --------------------------------
                handle_hosting();
                break;
            case HOST_MOUSE:
                // -------------------------------- CHECK EXIT CONDITIONS --------------------------------
                if (detect_device() == NONE) {
                    usb_state = HOST_UNKNOWN;
                    ESP_LOGI(TAG, "Mouse disconnected.");
                    transmit_data[0] = UPDATE;
                    transmit_data[1] = DEVICE_DISCONNECTED;
                    xQueueSend(usb_to_com_queue, transmit_data, portMAX_DELAY);
                }
                if (header == UPDATE) { // Receive an update that the other device has detected a host disconnection, thus this device no longer needs to host a device
                    if (received_data[1] == HOST_DISCONNECTED) {
                        ESP_LOGI(TAG, "Received host disconnected update, uninstalling host drivers.");
                        host_uninstall();
                        usb_state = UNKNOWN;
                        enumerate_as_keyboard(); // Temporary method to detect host, want to use phy but cant get it working
                    }
                }
                // -------------------------------- STATE BEHAVIOUR --------------------------------
                handle_hosting();
                break;
        }
    }
}