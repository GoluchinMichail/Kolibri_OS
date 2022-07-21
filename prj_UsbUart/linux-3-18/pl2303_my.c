#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define speed_t uint64_t

static speed_t pl2303_encode_baud_rate_divisor(unsigned char buf[4],
                                speed_t baud)
{
    unsigned int tmp;

    /*
     * Apparently the formula is:
     * baudrate = 12M * 32 / (2^buf[1]) / buf[0]
     */
    tmp = 12000000 * 32 / baud;
    buf[3] = 0x80;
    buf[2] = 0;
    buf[1] = (tmp >= 256);
    while (tmp >= 256) {
        tmp >>= 2;
        buf[1] <<= 1;
    }
    buf[0] = tmp;

    return baud;
}

int main (int argc, char **argv) {
    if (argc > 1) {
        unsigned char Bytes4[4];
        unsigned long baud = atol(argv[1]);
        pl2303_encode_baud_rate_divisor (Bytes4, baud);
        printf ("%u.%Xh ==> %02.2X.%02.2X.%02.2X.%02.2X\n", baud, baud, Bytes4[3], Bytes4[2], Bytes4[1], Bytes4[0]);
    }
    return 0;
}
