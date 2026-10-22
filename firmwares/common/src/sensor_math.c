#include "sensor_math.h"

uint8_t sht_crc(const uint8_t *data, unsigned length)
{
    uint8_t crc = 0xff;
    unsigned i, bit;
    for (i = 0; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (uint8_t)((crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1);
    }
    return crc;
}

int sht_decode(const uint8_t data[6], int16_t *temperature, uint16_t *humidity)
{
    uint32_t raw_t, raw_h;
    if (sht_crc(data, 2) != data[2] || sht_crc(data + 3, 2) != data[5])
        return 0;
    raw_t = ((uint32_t)data[0] << 8) | data[1];
    raw_h = ((uint32_t)data[3] << 8) | data[4];
    *temperature = (int16_t)(-4500 + (int32_t)((17500 * raw_t + 32767) / 65535));
    *humidity = (uint16_t)((10000 * raw_h + 32767) / 65535);
    return 1;
}

int report_due(int32_t value, int32_t previous, unsigned delta,
               unsigned age_seconds, unsigned maximum_seconds, int have_previous)
{
    int32_t difference = value - previous;
    if (difference < 0) difference = -difference;
    return !have_previous || (uint32_t)difference >= delta || age_seconds >= maximum_seconds;
}
