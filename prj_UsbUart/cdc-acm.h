
#ifndef _CDC_ACM_H_
#define _CDC_ACM_H_

#include <stdint.h>

#pragma pack(push,1)

//  the source:
//
//  "Universal Serial Bus Class Definitions for Communication Devices" version 1.1
//      January 19, 1999, Section 6.2.13
//      Terms and Abbreviations:
//  CDC     -   Communication Device Class
//  ACM     -   Abstract Control Model

//  ///////////  devices    CDC/ACM  (pl2303 and e.t.c)  /////////////////////////////////////////////////////////////
    //           compatibility:
    //
    //   GMUS-03 USB to COM (Prolific, chip PL-2303)
    //   Nokia 5130
    //   Huawei E1550
    //   AnyData ADU-310A

    typedef struct {
        uint32_t   dDteRate;       //  Data terminal rate, in bits per second
        uint8_t    bCharFormat;    //  Stop bits:
                                //      0 - 1 Stop bit
                                //      1 - 1.5 Stop bits
                                //      2 - 2 Stop bits
        uint8_t    bParityType;    //  Parity
                                //      0 - None
                                //      1 - Odd
                                //      2 - Even
                                //      3 - Mark
                                //      4 - Space
        uint8_t    bDataBits;      //  Data bits (5, 6, 7, 8 or 16)
    } _LineCoding;

    //  "Table 31: UART State Bitmap Values"      "CDC PSTN Subclass"  "Revision 1.2"
    #define CDC_ACM_SIGNAL_CDC             1        //  bit 0
    #define CDC_ACM_SIGNAL_DSR             2        //  bit 1
    #define CDC_ACM_SIGNAL_BREAK           4        //  bit 2  State of break detection mechanism of the device
    #define CDC_ACM_SIGNAL_RING            8        //  bit 3
    #define CDC_ACM_SIGNAL_FRAME        0x10        //  bit 4  A framing error has occurred
    #define CDC_ACM_SIGNAL_PARITY       0x20        //  bit 5  A parity error has occurred
    #define CDC_ACM_SIGNAL_OVERUN       0x40        //  bit 6  Received data has been discarded due to overrun in the device
    #define CDC_ACM_SIGNAL_CTS          0x80        //  bit 7  RESERVED %| for PL-2303 signal CTS
    #define CDC_ACM_MYSIGNAL_SEND_IS_FREE  0x02000

    //  Requests  Abstract Control Model
    #define CDC_ACM_TRANS_TO_DEVICE     0x21
    #define CDC_ACM_TRANS_TO_HOST       0xa1
    #define CDC_ACM_SET_LINE_CODING     0x20
    #define CDC_ACM_GET_LINE_CODING     0x21
    #define CDC_ACM_SET_CONTROL_LINE    0x22
//    //  constants for line governing
    #define CDC_ACM_SET_DTR             0x01
    #define CDC_ACM_SET_RTS             0x02

    #define CDC_ACM_STATUS_BYTE         8

    #define BCD_PL2303_MAGIC_QUIRK      0x0300  //  обидно ГДЕ СтОнДаРтЫ ЙЁ  "мая плакаль" :)) 2015.04.11

#pragma pack(pop)

#endif   //   #ifndef _CDC_ACM_H_
