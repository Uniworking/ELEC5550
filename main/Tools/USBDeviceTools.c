//#include <stdlib.h>
//#include <stdio.h>
//#include <stdbool.h>
//#include <string.h>
//#include <unistd.h>

#include "esp_log.h"
//#include "esp_err.h"
//#include "errno.h"

//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "freertos/event_groups.h"
//#include "freertos/queue.h"

#include "tinyusb.h"
#include "class/hid/hid_device.h"

#include "Tools/USBDeviceTools.h"

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

static const char *TAG = "DEVICE TOOLS";

typedef enum {
    NONE,
    MOUSE,
    KEYBOARD,
    DATASTICK
} device_type_t;

static device_type_t current_device = NONE;

// -------------------------------- MOUSE --------------------------------

static const uint8_t hid_mouse_descriptor[] = {
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_ITF_PROTOCOL_MOUSE))
};

static const char* hid_mouse_string_desc[] = {
    (char[]){0x09, 0x04}, // English
    "Group 16",
    "Free Space Optical Link: Mouse",
    "000001",
    "Mouse Interface"
};

static const uint8_t hid_mouse_config_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_mouse_descriptor), 0x81, 16, 10),
};

void enumerate_as_mouse(void)
{
    tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = hid_mouse_string_desc,
        .string_descriptor_count = sizeof(hid_mouse_string_desc)/sizeof(hid_mouse_string_desc[0]),
        .external_phy = false,
        .configuration_descriptor = hid_mouse_config_descriptor
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg)); // Starts tinyUSB, if it fails it aborts
    current_device = MOUSE;
}

void send_mouse_report_to_computer(usb_mouse_report_t *report) {
    tud_hid_mouse_report(
        HID_ITF_PROTOCOL_MOUSE,
        report->buttons,
        report->x_displacement,
        report->y_displacement,
        report->wheel,
        0); // horizontal scroll
    //ESP_LOGI(TAG, "Mouse report sent to computer!.");
    //ESP_LOGI(TAG, "X: %+04d\tY: %+04d\tWheel: %+03d\t|L:%c|R:%c|M:%c|\r",
    //       report->x_displacement,
    //       report->y_displacement,
    //       report->wheel,
    //       (report->buttons & 0x01) ? 'o' : ' ',   // Left button (bit 0)
    //       (report->buttons & 0x02) ? 'o' : ' ',   // Right button (bit 1)
    //       (report->buttons & 0x04) ? 'o' : ' ');  // Middle button (bit 2)
}

// -------------------------------- KEYBOARD --------------------------------

static const uint8_t hid_keyboard_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

static const char* hid_keyboard_string_desc[] = {
    (char[]){0x09, 0x04}, // English
    "Group 16",
    "Free Space Optical Link: Keyboard",
    "000001",
    "Keyboard Interface"
};

static const uint8_t hid_keyboard_config_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 1, false, sizeof(hid_keyboard_descriptor), 0x81, 16, 10),
};

void enumerate_as_keyboard(void)
{
    tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = hid_keyboard_string_desc,
        .string_descriptor_count = sizeof(hid_keyboard_string_desc)/sizeof(hid_keyboard_string_desc[0]),
        .external_phy = false,
        .configuration_descriptor = hid_keyboard_config_descriptor
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    current_device = KEYBOARD;
}


void send_keyboard_report_to_computer(usb_keyboard_report_t *report) {
    tud_hid_keyboard_report(
        0,                    // report ID (0 if not used)
        report->modifier,      // modifier keys
        report->keycodes);       // array of 6 keycodes
}

// -------------------------------- GENERAL --------------------------------

uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance; // ignore, single interface

    switch (current_device) {
        case MOUSE:
            return hid_mouse_descriptor;
        case KEYBOARD:
            return hid_keyboard_descriptor;
        default:
            return NULL; // should not happen
    }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
}

void disconnect_device(void) {
    ESP_ERROR_CHECK(tinyusb_driver_uninstall());
    current_device = NONE;
}

bool detect_host() {
    return tud_ready();
}
