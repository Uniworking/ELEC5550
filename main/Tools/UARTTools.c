#include "Tools/UARTTools.h"

#include "driver/uart.h"

#include "Tools/Hamming74.h"
#include "state_machines.h"

#define UART_PORT UART_NUM_1
#define TX_PIN    17
#define RX_PIN    18

void uart_init(int baud_rate) {
    const uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_driver_install(UART_PORT, 1024, 0, 0, NULL, 0);
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}



void send_header(uint8_t header) {
    uint8_t encoded_header[2];
    encode_bytes(&header, 2, encoded_header);
    uart_write_bytes(UART_PORT, (const char *)encoded_header, 2);
    uart_flush(UART_PORT);
}

void send_data(const uint8_t *data, uint8_t length) {
    uint8_t encoded_bytes[2*length];
    encode_bytes(data, 2*length, encoded_bytes);
    uart_write_bytes(UART_PORT, (const char *)encoded_bytes, 2*length);
    uart_flush(UART_PORT);
}


uint8_t read_header(int ms_to_wait) {     // change to return success / fail instead of error header?
    int len;
    uint8_t encoded_header[2];
    uint8_t header;
    len = uart_read_bytes(UART_PORT, encoded_header, 2, pdMS_TO_TICKS(ms_to_wait));
    if (len == 2) {
        decode_bytes(encoded_header, 2, &header);
    } else if (len == 1) {
        header = ERROR;
    } else {
        header = NO_HEADER;
    }
    return header;
}

uint8_t read_data(uint8_t *data, uint8_t length, int ms_to_wait) {
    uint8_t len;
    uint8_t encoded_bytes[2*length];
    len = uart_read_bytes(UART_PORT, encoded_bytes, 2*length, pdMS_TO_TICKS(ms_to_wait));
    if (len == 2*length) {
        decode_bytes(encoded_bytes, 2*length, data);
    } 
    return len;
}