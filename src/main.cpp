#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"
#include "esp_log.h"

void app_main(void)
{
    tusb_init(); // Initialize TinyUSB device stack

    while (1) {
        tud_task(); // TinyUSB background task

        if (tud_hid_ready()) {
            // Send "a" key
            uint8_t keycode[6] = { HID_KEY_A };
            tud_hid_keyboard_report(0, 0, keycode);
            vTaskDelay(pdMS_TO_TICKS(100));

            // Release key
            tud_hid_keyboard_report(0, 0, NULL);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// Required TinyUSB callback
uint16_t tud_hid_get_report_cb(uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t* buffer,
                               uint16_t reqlen)
{
    return 0;
}

void tud_hid_set_report_cb(uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const* buffer,
                           uint16_t bufsize)
{
    // Not used
}
