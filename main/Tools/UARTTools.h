#include "driver/uart.h"

#include "Hamming74.h"

extern const uint8_t error_header;

void uart_init(int baud_rate);

void send_header(uint8_t header);

void send_data(const uint8_t *data, uint8_t length);

uint8_t read_header(int ms_to_wait);

uint8_t read_data(uint8_t *data, uint8_t length, int ms_to_wait);