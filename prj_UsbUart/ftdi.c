
#include <stdint.h>
#define __u32   uint32_t

//  ftdi.c  linux-3-18

static __u32 ftdi_232bm_baud_base_to_divisor(int baud, int base)
{
    static const unsigned char divfrac[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };
    __u32 divisor;
    /* divisor shifted 3 bits to the left */
    int divisor3 = base / 2 / baud;
    divisor = divisor3 >> 3;
    divisor |= (__u32)divfrac[divisor3 & 0x7] << 14;
    /* Deal with special cases for highest baud rates. */
    if (divisor == 1)
        divisor = 0;
    else if (divisor == 0x4001)
        divisor = 1;
//dev_dbg (my_dev, "ftdi_232bm_baud_base_to_divisor (%u, %u)= %Xh\n", baud, base, divisor);
    return divisor;
}

/*static */__u32 ftdi_232bm_baud_to_divisor(int baud)
{
    return ftdi_232bm_baud_base_to_divisor(baud, 48000000);
}
