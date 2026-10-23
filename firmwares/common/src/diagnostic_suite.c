#include "diagnostic_suite.h"
#include <string.h>

static const diag_case cases[DIAG_CASES] = {
#ifdef SENSOR_GPIO_I2C_DIAGNOSTICS
    /* GPIO cases encode half-period in 10 us units instead of prescaler.
     * LONG_SETTLE changes the settle unit from 100 us to 1 ms. */
    {0,0x44,31,1,0,0x2416},
    {0,0x44,10,1,DIAG_FLAG_GPIO,0x2416},
    {0,0x44,31,1,0,0x2416},
    {0,0x44,50,1,DIAG_FLAG_GPIO,0x2416},
    {0,0x44,10,1,DIAG_FLAG_GPIO|DIAG_FLAG_NO_PULLUP,0x2416},
    {0,0x44,10,200,DIAG_FLAG_GPIO,0x2416},
    {0,0x44,10,200,DIAG_FLAG_GPIO|DIAG_FLAG_LONG_SETTLE,0x2416},
    {0,0x44,10,200,DIAG_FLAG_GPIO|DIAG_FLAG_LONG_SETTLE|DIAG_FLAG_SOFT_RESET,0x2416},
    {0,0x44,31,1,DIAG_FLAG_NO_FILTER,0x2416},
    {0,0x44,255,1,DIAG_FLAG_NO_FILTER,0x2416},
    {0,0x44,31,200,DIAG_FLAG_LONG_SETTLE,0x2416},
    {0,0x44,10,1,DIAG_FLAG_GPIO|DIAG_FLAG_CLEAR_BUS,0x2416},
    {0,0x44,10,1,DIAG_FLAG_GPIO,0x240b},
    {0,0x44,10,1,DIAG_FLAG_GPIO,0x2400},
    {0,0x44,10,1,DIAG_FLAG_GPIO,0x2416},
    {0,0x44,31,1,0,0x2416}
#else
    {0,0x44,31,1,0,0x2416},
    {0,0x44,31,1,DIAG_FLAG_LED,0x2416},
    {0,0x44,63,1,0,0x2416},
    {0,0x44,127,1,0,0x2416},
    {0,0x44,255,1,0,0x2416},
    {0,0x44,31,50,0,0x2416},
    {0,0x44,31,200,0,0x2416},
    {0,0x44,31,1,DIAG_FLAG_NO_PULLUP,0x2416},
    {0,0x45,31,1,0,0x2416},
    {1,0x44,31,1,0,0x2416},
    {1,0x45,31,1,0,0x2416},
    {0,0x44,31,1,DIAG_FLAG_CLEAR_BUS,0x2416},
    {0,0x44,31,1,DIAG_FLAG_SOFT_RESET,0x2416},
    {0,0x44,31,1,0,0x240b},
    {0,0x44,31,1,0,0x2400},
    {0,0x44,31,1,0,0x2416}
#endif
};
static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void write32(uint8_t *p, uint32_t value) {
    unsigned n;
    for (n=0;n<4;++n) p[n]=(uint8_t)(value>>(8*n));
}
static uint32_t checksum(const diag_record *record) {
    uint32_t hash=2166136261UL;
    unsigned n;
    for (n=0;n<DIAG_RECORD_BYTES;++n) {
        uint8_t byte=(n>=12 && n<16) ? 0 : record->bytes[n];
        hash=(hash^byte)*16777619UL;
    }
    return hash;
}
const diag_case *diag_case_at(unsigned index) {return index<DIAG_CASES ? &cases[index] : 0;}
void diag_record_begin(diag_record *record, uint32_t run) {
    memset(record,0,sizeof(*record));
    memcpy(record->bytes,"SHTD",4); record->bytes[4]=DIAG_PROTOCOL; record->bytes[5]=DIAG_CASES;
    write32(record->bytes+8,run);
}
uint8_t *diag_record_row(diag_record *record, unsigned index) {
    return index<DIAG_CASES ? record->bytes+16+index*DIAG_ROW_BYTES : 0;
}
void diag_record_finish(diag_record *record) {
    record->bytes[6]=1; write32(record->bytes+12,checksum(record));
}
int diag_record_valid(const diag_record *record) {
    unsigned n;
    if (!(memcmp(record->bytes,"SHTD",4)==0 && (record->bytes[4]==1 || record->bytes[4]==2) &&
        record->bytes[5]==DIAG_CASES && record->bytes[6]==1 &&
        read32(record->bytes+12)==checksum(record))) return 0;
    for (n=0;n<DIAG_CASES;++n)
        if (record->bytes[16+n*DIAG_ROW_BYTES] != n+1) return 0;
    return 1;
}
uint32_t diag_record_run(const diag_record *record) {return read32(record->bytes+8);}
void diag_packet(const diag_record *record, unsigned index, unsigned durable,
                 unsigned replay, uint8_t packet[DIAG_PACKET_BYTES]) {
    memset(packet,0,DIAG_PACKET_BYTES);
    packet[0]=0xd1; packet[1]=record->bytes[4]; write32(packet+2,diag_record_run(record));
    packet[6]=index<DIAG_CASES ? (uint8_t)index : 0xff;
    packet[7]=(durable ? 1 : 0) | (record->bytes[6] ? 2 : 0) | (replay ? 4 : 0);
    if (index<DIAG_CASES) memcpy(packet+8,record->bytes+16+index*DIAG_ROW_BYTES,DIAG_ROW_BYTES);
    else {
        packet[8]=DIAG_CASES;
        memcpy(packet+12,record->bytes+12,4);
    }
}
