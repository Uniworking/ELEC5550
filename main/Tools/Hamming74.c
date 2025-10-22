#include "Tools/hamming74.h"
#include <stdint.h>

static int parity_check(uint8_t encoded, uint8_t p) // modify to calculate syndrome (at first 0 in position so just parity, second time through calculates syndrome)
{
    uint8_t pos_mask = (1 << p);                    // 1-based position of parity bit 1,2,4 (000x0xx)
    uint8_t sum = 0;
    for (uint8_t i = 1; i < 8; i++) {               // Iterate over all 1-based positions
        if ((i & pos_mask) != 0) {                  // Check if the i-th position has the p-th position bit set
            sum ^= (encoded >> (i-1)) & 1;
        }
    }
    return sum;
}

static uint8_t calculate_syndrome(uint8_t encoded)
{
    uint8_t syndrome = 0;
    for (uint8_t p = 0; p < 3; p++) {
        syndrome ^= parity_check(encoded, p) << p;
    }
    return syndrome;
}

static uint8_t encode_nibble(const uint8_t nibble)
{
    uint8_t encoded_nibble = 0;
    uint8_t j = 0;
    for (uint8_t i = 0; i < 7; i++) {              // Place data bits in non-parity positions
        if ((i & (i + 1)) == 0) {                  // Skip positions that are powers of 2: indices 0, 1, and 3 (i+1 == 1,2,4)
            continue;
        }
        encoded_nibble |= (((nibble >> j) & 1) << i); // |= bitwise OR
        j++;
    }
    for (uint8_t p = 0; p < 3; p++) {  // Calculate and set the parity bits
        uint8_t parity_pos = (1 << p) - 1;  // 0-based index for parity bit
        encoded_nibble |= parity_check(encoded_nibble, p) << parity_pos;
    }
    return encoded_nibble;
}

static uint8_t decode_nibble(uint8_t encoded)
{
    uint8_t syndrome = calculate_syndrome(encoded); // Calculate the syndrome to detect errors over 7 bits
    if (syndrome != 0) {                         // Correct the error if syndrome is non-zero
        uint8_t error_pos = syndrome - 1;  // Convert to 0-based index
        if (error_pos < 7) {
            encoded ^= 1 << error_pos;  // Flip the incorrect bit
        }
    }

    uint8_t decoded = 0;
    decoded |= (encoded >> 2) & 0x01;  // For Hamming(7,4), the data bits are at indices 2, 4, 5, and 6.
    decoded |= (encoded >> 3) & 0x02;
    decoded |= (encoded >> 3) & 0x04;
    decoded |= (encoded >> 3) & 0x08;
    return decoded;
}

void encode_bytes(const uint8_t *data, uint8_t encoded_length, uint8_t *encoded_bytes)
{
    for (uint8_t i = 0; i < encoded_length; i++) {
        uint8_t nibble;
        if (i % 2 == 0) {                               // if even, extract high nibble
            nibble = data[i / 2] >> 4;   
        } else {                                        // if odd, extract low nibble
            nibble = data[i / 2] & 0x0F;
        }
        encoded_bytes[i] = encode_nibble(nibble);       // Place the encoded byte into the output array.
    }
}

void decode_bytes(const uint8_t *encoded_bytes, uint8_t encoded_length, uint8_t *decoded_bytes) // length should be even
{
    for (uint8_t i = 0; i < encoded_length; i++) {
        uint8_t nibble = decode_nibble(encoded_bytes[i]);
        if (i % 2 == 0) {                           // if even, set high nibble
            decoded_bytes[i / 2] = nibble << 4;     // Sets upper half to decoded bits and lower half to 0
        } else {                            
            decoded_bytes[i / 2] |= nibble;   
        }
    }
}