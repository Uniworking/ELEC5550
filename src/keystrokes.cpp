/**
 * @file main.cpp
 * @brief Example showing how to make an ESP32‑S3 act as a USB keyboard
 * using the TinyUSB stack within the ESP‑IDF framework.  When the device
 * is plugged in to a USB host (e.g. a PC), it will type the entire English
 * alphabet from 'a' through 'z'.  This example is intended for use with
 * PlatformIO and the Espressif IDF framework.  To build the project,
 * include this file in your PlatformIO project and ensure that the
 * TinyUSB device class is enabled in your `sdkconfig`.  The example
 * derives from the official Espressif HID example, which illustrates
 * sending a single 'a' keypress and release【345916611496093†L143-L151】.
 *
 * The TinyUSB stack is wrapped by the `esp_tinyusb` component, which
 * simplifies integrating common USB classes such as HID into ESP‑IDF
 * projects【548160773535636†L116-L129】.  If you are starting from an
 * empty project, run `idf.py add-dependency esp_tinyusb~1.0.0` and
 * enable the HID device class via `menuconfig` as described in the
 * TinyUSB application guide【548160773535636†L116-L129】.
 */

// TinyUSB, FreeRTOS and ESP‑IDF headers are written in C.  When
// including them in a C++ source file we need to wrap the includes
// in an extern "C" block to avoid name mangling.
#ifdef __cplusplus
extern "C" {
#endif
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#ifdef __cplusplus
}
#endif

/**
 * HID report descriptor
 *
 * This descriptor tells the USB host that we implement a boot
 * protocol keyboard.  It leverages TinyUSB helper macros to build
 * the descriptor.  Only a keyboard interface is declared here.
 */
static const uint8_t hid_report_descriptor[] = {
    // Define a single report for a boot protocol keyboard.  The report ID
    // corresponds to the keyboard interface.  See the TinyUSB examples for
    // details【345916611496093†L143-L151】.
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)),
};

/**
 * USB string descriptors
 *
 * Index 0 contains the supported language code (0x0409 for US English).
 * Subsequent entries provide the manufacturer, product name, serial
 * number and interface description respectively.
 */
static const char *hid_string_descriptor[] = {
    (const char[]){0x09, 0x04}, // 0: Supported language is English (0x0409)
    "TinyUSB",                // 1: Manufacturer string
    "ESP32-S3 Keyboard",      // 2: Product string
    "123456",                 // 3: Serial number string (replace with chip ID if desired)
    "HID Keyboard Interface", // 4: Interface description
};

/**
 * Configuration descriptor
 *
 * This configuration defines one configuration with a single HID interface.
 * The endpoint address 0x81 is used for sending IN reports to the host.
 */
static const uint8_t hid_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length,
    // attributes (remote wakeup), and power in mA.
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN),
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, boot protocol, report descriptor length,
    // endpoint address, size of report, and polling interval (in ms).
    TUD_HID_DESCRIPTOR(0, 4, true, sizeof(hid_report_descriptor), 0x81, 8, 10),
};

/**
 * @brief Callback invoked to provide the HID report descriptor.
 *
 * TinyUSB will call this function when the host requests the HID report
 * descriptor.  We return a pointer to our statically allocated
 * descriptor.  The descriptor must remain valid for the duration of
 * the transfer.
 */
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return hid_report_descriptor;
}

/**
 * @brief Callback invoked on GET_REPORT requests from the host.
 *
 * We do not support GET_REPORT in this example, so returning zero
 * causes the control request to be stalled.
 */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                              hid_report_type_t report_type, uint8_t *buffer,
                              uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

/**
 * @brief Callback invoked on SET_REPORT requests or OUT reports.
 *
 * In this simple keyboard example we do not handle output reports (e.g.
 * LED status), so this callback is left empty.  You could add code
 * here to handle LED reports (caps lock/num lock) if desired.
 */
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

/**
 * @brief Send the alphabet as individual key press/release reports.
 *
 * When called, this function iterates through the HID key codes for
 * letters 'A' through 'Z'.  For each letter it sends a key press
 * report and then a release report.  A small delay is inserted
 * between each transmission to allow the host to process the events.
 */
static void send_alphabet(void)
{
    // Array of HID key codes representing letters 'a' through 'z'.
    static const uint8_t letters[26] = {
        HID_KEY_A, HID_KEY_B, HID_KEY_C, HID_KEY_D, HID_KEY_E, HID_KEY_F,
        HID_KEY_G, HID_KEY_H, HID_KEY_I, HID_KEY_J, HID_KEY_K, HID_KEY_L,
        HID_KEY_M, HID_KEY_N, HID_KEY_O, HID_KEY_P, HID_KEY_Q, HID_KEY_R,
        HID_KEY_S, HID_KEY_T, HID_KEY_U, HID_KEY_V, HID_KEY_W, HID_KEY_X,
        HID_KEY_Y, HID_KEY_Z,
    };
    // Each report can contain up to six key codes.  For a single
    // keystroke we only need to populate the first element; the
    // remaining elements are set to zero.
    uint8_t report[6] = {0};

    for (size_t i = 0; i < sizeof(letters); ++i) {
        // Set the first byte to the key code for the current letter.
        report[0] = letters[i];
        // Send the key press report.  The interface protocol ID is
        // HID_ITF_PROTOCOL_KEYBOARD and the second parameter (modifier)
        // is zero because we are not using any modifier keys.
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, report);
        // Hold the key for 50 ms.  This delay corresponds to the delay
        // used in the official example for sending an 'a' keystroke【345916611496093†L143-L151】.
        vTaskDelay(pdMS_TO_TICKS(50));
        // Release the key by sending a report with a NULL pointer.  TinyUSB
        // interprets a NULL report pointer as no keys pressed【345916611496093†L143-L151】.
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
        // Short delay between letters to separate the keystrokes.
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * @brief Main application entry point.
 *
 * The ESP‑IDF runtime calls this function once the scheduler has started.
 * We initialize the TinyUSB device with our descriptors and then wait
 * until the host has enumerated the device (`tud_mounted()` returns
 * true).  Once enumeration completes, the alphabet is sent exactly
 * once.  If you wish to resend the alphabet upon reconnection you
 * can clear the `sent` flag when `tud_mounted()` becomes false.
 */
extern "C" void app_main(void)
{
    static const char *TAG = "usb_keyboard";

    ESP_LOGI(TAG, "USB initialization");
    // Configure the TinyUSB driver.  We provide our string
    // descriptor array and configuration descriptor here.  For simple
    // HID devices the default device descriptor supplied by TinyUSB is
    // sufficient, so `device_descriptor` is set to NULL.  We also
    // indicate that the internal Full‑Speed PHY should be used by
    // setting `external_phy` to false.
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor      = NULL,
        .string_descriptor      = hid_string_descriptor,
        .string_descriptor_count = sizeof(hid_string_descriptor) /
                                    sizeof(hid_string_descriptor[0]),
        .external_phy           = false,
#if (TUD_OPT_HIGH_SPEED)
        .fs_configuration_descriptor = hid_configuration_descriptor,
        .hs_configuration_descriptor = hid_configuration_descriptor,
        .qualifier_descriptor    = NULL,
#else
        .configuration_descriptor = hid_configuration_descriptor,
#endif
    };

    // Install the TinyUSB driver.  This function initializes the USB
    // peripheral and registers our descriptors.  It is the equivalent
    // of the call in the official HID example【345916611496093†L165-L196】.
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB initialization DONE");

    bool sent = false;
    while (true) {
        // Wait until the device has been enumerated by the host.  The
        // tud_mounted() helper returns true once enumeration is complete.
        if (tud_mounted() && !sent) {
            send_alphabet();
            sent = true;
        }
        // Optionally, you could detect a disconnect and reset `sent` to
        // false to allow the alphabet to be sent again on the next
        // connection.
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}