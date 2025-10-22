#include <stdint.h>

void encode_bytes(const uint8_t *data, uint8_t encoded_length, uint8_t *encoded_bytes);

void decode_bytes(const uint8_t *encoded_bytes, uint8_t encoded_length, uint8_t *decoded_bytes);