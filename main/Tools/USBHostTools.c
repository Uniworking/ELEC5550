#include "usb/usb_host.h"
#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"

#include "esp_log.h"

#include "Tools/USBHostTools.h"
#include "state_machines.h"

static const char *TAG = "HOST TOOLS";

static uint8_t current_device = NONE;

QueueHandle_t app_event_queue = NULL;

typedef struct {
    hid_host_device_handle_t handle;
    hid_host_driver_event_t event;
    void *arg;
} app_event_queue_t;

app_event_queue_t evt_queue; 

uint8_t detect_device(void) {
    return current_device;
}

void keyboard_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg)
{
    uint8_t data[65] = { 0 };
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle,
                                                                  &data[1],
                                                                  64,
                                                                  &data_length));
        data[0] = REPORT_KEYBOARD;
        xQueueSend(usb_to_com_queue, data, 0);
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
        current_device = NONE;
        break;
    default:
        break;
    }
    
}

void mouse_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg)
{
    uint8_t data[65] = { 0 };
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(hid_device_handle,
                                                                  &data[1],
                                                                  64,
                                                                  &data_length));
        data[0] = REPORT_MOUSE;
        xQueueSend(usb_to_com_queue, data, 0);
        //ESP_LOGI(TAG, "Sending HID mouse report to COM SM.");
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
        current_device = NONE;
        break;
    default:
        break;
    }
}

void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                              const hid_host_driver_event_t event,
                              void *arg)
{
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED: {
        hid_host_device_config_t dev_config = { 0 };
        if (dev_params.proto == HID_PROTOCOL_KEYBOARD) {
            dev_config.callback     = keyboard_callback;
            dev_config.callback_arg = NULL;
            current_device = KEYBOARD;
        } else if (dev_params.proto == HID_PROTOCOL_MOUSE) {
            dev_config.callback     = mouse_callback;
            dev_config.callback_arg = NULL;
            current_device = MOUSE;
        } else {
            return;
        }
    
        if (dev_params.proto != HID_PROTOCOL_NONE) {
            ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));   // Initialises the device.
            if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {               // Special initialisation 
                ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT)); 
                if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                    ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
                }
            }
            ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));    // Begins communication with the device and starts polling
        }
        break;
    }
    default:
        break;
    }
}

static void usb_lib_task(void *arg)
{
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));    // Install the host driver
    xTaskNotifyGive(arg);                               // Notifies main that the host driver is installed

    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);   // Handles the USB protocol
    }
}

// Handles the incomming USB data by placing the struct in the queue
void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                              const hid_host_driver_event_t event,
                              void *arg)
{
    const app_event_queue_t evt_queue = {
        .handle = hid_device_handle,
        .event = event,
        .arg = arg
    };
    xQueueSend(app_event_queue, &evt_queue, 0);
}

void host_install(void) {
    BaseType_t task_created; 
    app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));  // Create queue. wouldnt it make more sense to have this before the other tasks are created?

    // Creates a task for USB initialisation and host event handling?
    task_created = xTaskCreatePinnedToCore(usb_lib_task,                    // Task function
                                           "usb_events",                    // Task name (for debugging)
                                           4096,                            // Stack size in bytes (4 KB)
                                           xTaskGetCurrentTaskHandle(),     // Task parameter (argument passed in)
                                           2,                               // Priority
                                           NULL,                            // Task handle (not saved here)
                                           0);                              // Core to pin the task to (core 0)
    assert(task_created == pdTRUE);             // if task creation fails aborts the program
    ulTaskNotifyTake(false, 1000);              // Wait for notification from usb_lib_task to proceed
  
    const hid_host_driver_config_t hid_host_driver_config = {   // Configure and install the HID host driver.
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_callback,       // gets called whenever HID devices connect, disconnect.
        .callback_arg = NULL
    };
    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));
}

void host_uninstall(void) {
    hid_host_uninstall();
    usb_host_uninstall();
}

void handle_hosting(void) {
    if (xQueueReceive(app_event_queue, &evt_queue, pdMS_TO_TICKS(10))) {  // Wait until an item is placed in the queue
            hid_host_device_event(evt_queue.handle,
                                  evt_queue.event,
                                  evt_queue.arg);
    }
}