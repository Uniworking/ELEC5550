#include <stdint.h>

// -------------------------------- MOUSE --------------------------------

void enumerate_as_mouse(void);

typedef struct {
    uint8_t buttons;
    int8_t x_displacement;
    int8_t y_displacement;
    int8_t wheel;
} usb_mouse_report_t;

void send_mouse_report_to_computer(usb_mouse_report_t *report);

// -------------------------------- KEYBOARD --------------------------------

void enumerate_as_keyboard(void);

typedef struct {
    uint8_t modifier;    // bitmask for shift, ctrl, alt, etc
    uint8_t reserved;    // always 0
    uint8_t keycodes[6]; // up to 6 keys pressed at once
} usb_keyboard_report_t;

void send_keyboard_report_to_computer(usb_keyboard_report_t *report);

// -------------------------------- GENERAL --------------------------------

// uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance);                                                                           already defined by tinyusb

// uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);    already defined by tinyusb

// void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize); already defined by tinyusb

void disconnect_device(void);

bool detect_host(void);