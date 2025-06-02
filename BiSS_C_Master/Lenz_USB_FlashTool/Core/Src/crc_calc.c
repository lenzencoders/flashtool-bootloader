#include "crc_calc.h"

uint8_t CalculateCRCCircularBuffer(uint8_t *buffer, uint16_t buffer_size, uint8_t start_index, uint8_t length) {
     uint8_t sum = 0;
    for (uint8_t i = 0; i < length; i++) {
        uint8_t index = (start_index + i) % buffer_size;
        sum += buffer[index];
    }
    uint8_t lsb = sum & 0xFF;
    return (uint8_t)(~lsb + 1);
}

uint8_t CalculateCRC(uint8_t *data, uint32_t length) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < length; i++) {
        sum += data[i];
    }
    uint8_t lsb = sum & 0xFF;
    return (uint8_t)(~lsb + 1);
}
