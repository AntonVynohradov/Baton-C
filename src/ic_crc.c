/**
 * @file  ic_crc.c
 * @brief CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF), bitwise, no table.
 *        At 9600 bit/s the cost is negligible and it saves 512 B of flash.
 */
#include "ic_internal.h"

uint16_t ic_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    while (len--)
    {
        crc = (uint16_t)(crc ^ ((unsigned)(*data++) << 8));
        for (uint8_t i = 0; i < 8u; i++)
        {
            crc = ((crc & 0x8000u) != 0u) ? (uint16_t)(((unsigned)crc << 1) ^ 0x1021u)
                                       : (uint16_t)((unsigned)crc << 1);
        }
    }
    return crc;
}
