# "main.c" "state_machine_usb.c" "state_machine_com.c" "Tools/USBDeviceTools.c" "Tools/USBHostTools.c" "Tools/UARTTools.c" "Tools/Hamming74.c"
idf_component_register(
    SRCS "main.c" "state_machine_usb.c" "state_machine_com.c" "Tools/USBDeviceTools.c" "Tools/USBHostTools.c" "Tools/UARTTools.c" "Tools/Hamming74.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES usb esp_driver_gpio esp_driver_uart esp_timer
    )
                    