#include <stdio.h>
#include <string.h>
#include "diagnostic_suite.h"
#include "sensor_math.h"
static void emit(const diag_record *record,unsigned index) {
    uint8_t packet[DIAG_PACKET_BYTES];
    diag_packet(record,index,1,1,packet);
    for(unsigned n=0;n<DIAG_PACKET_BYTES;++n)printf("%02x",packet[n]);
    putchar('\n');
}
int main(void) {
    diag_record record;
    diag_record_begin(&record,0x12345678);
    for(unsigned n=0;n<DIAG_CASES;++n) {
        uint8_t *row=diag_record_row(&record,n);
        const diag_case *spec=diag_case_at(n);
        row[0]=n+1;row[1]=7;row[2]=0;row[3]=0x7f | (n==1 ? 0x80 : 0);
        row[4]=0xf0;row[5]=0x0a;
        row[6]=0x66;row[7]=0x66;row[8]=sht_crc(row+6,2);
        row[9]=0x80;row[10]=0;row[11]=sht_crc(row+9,2);
        row[12]=spec->flags | (spec->location ? 128 : 0);
        row[13]=spec->settle_units;row[14]=spec->prescaler;row[15]=spec->address;
    }
    diag_record_finish(&record);
    emit(&record,DIAG_CASES);
    for(unsigned n=DIAG_CASES;n;--n)emit(&record,n-1);
    emit(&record,0);emit(&record,DIAG_CASES);
    return !diag_record_valid(&record);
}
