
#include <stdint.h>
#define __u32   uint32_t

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ftdi_sio.h"
#include "tty_flags.h"
#include "ftdi_sio_ids.h"
#define __u16   uint16_t
#define speed_t uint64_t
#define le16_to_cpu(a)  a
static const char *ftdi_chip_name[] = {
    [SIO] = "SIO",  /* the serial part of FT8U100AX */
    [FT8U232AM] = "FT8U232AM",
    [FT232BM] = "FT232BM",
    [FT2232C] = "FT2232C",
    [FT232RL] = "FT232RL",
    [FT2232H] = "FT2232H",
    [FT4232H] = "FT4232H",
    [FT232H]  = "FT232H",
    [FTX]     = "FT-X"
};

typedef struct usb_device_descriptor {
    uint16_t idProduct;
} _usb_device_descriptor;

struct usb_serial {
    struct usb_device       *dev;
};

struct usb_device {
    struct usb_device_descriptor descriptor;
};

struct tty_struct {
    uint64_t    baud;
};

struct ftdi_private {
    enum ftdi_chip_type chip_type;
                /* type of device, either SIO or FT8U232AM */
//  my_style:   !!! ???
//    int baud_base;      /* baud base clock for divisor setting */
//    int custom_divisor; /* custom_divisor kludge, this is for
//                   baud_base (different from what goes to the
//                   chip!) */
//    __u16 last_set_data_urb_value ;
                /* the last data state set - needed for doing
                 * a break
                 */
//    int flags;      /* some ASYNC_xxxx flags are supported */
//    unsigned long last_dtr_rts; /* saved modem control outputs */
//    char prev_status;        /* Used for TIOCMIWAIT */
//    char transmit_empty;    /* If transmitter is empty or not */
//    __u16 interface;    /* FT2232C, FT2232H or FT4232H port interface
//                   (0 for FT232/245) */

//    speed_t force_baud; /* if non-zero, force the baud rate to
//                   this value */
//    int force_rtscts;   /* if non-zero, force RTS-CTS to always
//                   be enabled */

//    unsigned int latency;       /* latency setting in use */
//    unsigned short max_packet_size;
//    struct mutex cfg_lock; /* Avoid mess by parallel calls of config ioctl() and change_speed() */
};

struct usb_serial_port {
    struct usb_serial   *serial;
    struct ftdi_private *ftdi;
};

static __u32 ftdi_2232h_baud_base_to_divisor(int baud, int base)
{
    static const unsigned char divfrac[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };
    __u32 divisor;
    int divisor3;

    /* hi-speed baud rate is 10-bit sampling instead of 16-bit */
    divisor3 = base * 8 / (baud * 10);

    divisor = divisor3 >> 3;
    divisor |= (__u32)divfrac[divisor3 & 0x7] << 14;
    /* Deal with special cases for highest baud rates. */
    if (divisor == 1)
        divisor = 0;
    else if (divisor == 0x4001)
        divisor = 1;
    /*
     * Set this bit to turn off a divide by 2.5 on baud rate generator
     * This enables baud rates up to 12Mbaud but cannot reach below 1200
     * baud with this bit set
     */
    divisor |= 0x00020000;
    return divisor;
}

struct ftdi_private *usb_get_serial_port_data (struct usb_serial_port *port) {
    return port->ftdi;
}

int tty_get_baud_rate (struct tty_struct * tty) {
    return tty->baud;
}

void tty_encode_baud_rate (struct tty_struct * tty, int i_baud, int o_baud) {
}

void dev_dbg (void *notuse/*struct device * notuse*/, char *frmt, ...) {
    va_list args;
    va_start(args, frmt);
    vprintf (frmt, args);
    va_end(args);
}

static unsigned short int ftdi_232am_baud_base_to_divisor(int baud, int base)
{
    unsigned short int divisor;
    /* divisor shifted 3 bits to the left */
    int divisor3 = base / 2 / baud;
    if ((divisor3 & 0x7) == 7)
        divisor3++; /* round x.7/8 up to x+1 */
    divisor = divisor3 >> 3;
    divisor3 &= 0x7;
    if (divisor3 == 1)
        divisor |= 0xc000;
    else if (divisor3 >= 4)
        divisor |= 0x4000;
    else if (divisor3 != 0)
        divisor |= 0x8000;
    else if (divisor == 1)
        divisor = 0;    /* special case for maximum baud rate */
    return divisor;
}

struct device * my_dev;

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
dev_dbg (my_dev, "ftdi_232bm_baud_base_to_divisor (%u, %u)= %Xh\n", baud, base, divisor);
    return divisor;
}

static unsigned short int ftdi_232am_baud_to_divisor(int baud)
{
     return ftdi_232am_baud_base_to_divisor(baud, 48000000);
}

static __u32 ftdi_232bm_baud_to_divisor(int baud)
{
     return ftdi_232bm_baud_base_to_divisor(baud, 48000000);
}

static __u32 ftdi_2232h_baud_to_divisor(int baud)
{
     return ftdi_2232h_baud_base_to_divisor(baud, 120000000);
}

static __u32 get_ftdi_divisor(struct tty_struct *tty,
                        struct usb_serial_port *port)
{
    struct ftdi_private *priv = usb_get_serial_port_data(port);
    struct device *dev = NULL;//&port->dev;
    __u32 div_value = 0;
    int div_okay = 1;
    int baud;

    /*
     * The logic involved in setting the baudrate can be cleanly split into
     * 3 steps.
     * 1. Standard baud rates are set in tty->termios->c_cflag
     * 2. If these are not enough, you can set any speed using alt_speed as
     * follows:
     *    - set tty->termios->c_cflag speed to B38400
     *    - set your real speed in tty->alt_speed; it gets ignored when
     *      alt_speed==0, (or)
     *    - call TIOCSSERIAL ioctl with (struct serial_struct) set as
     *  follows:
     *      flags & ASYNC_SPD_MASK == ASYNC_SPD_[HI, VHI, SHI, WARP],
     *  this just sets alt_speed to (HI: 57600, VHI: 115200,
     *  SHI: 230400, WARP: 460800)
     * ** Steps 1, 2 are done courtesy of tty_get_baud_rate
     * 3. You can also set baud rate by setting custom divisor as follows
     *    - set tty->termios->c_cflag speed to B38400
     *    - call TIOCSSERIAL ioctl with (struct serial_struct) set as
     *  follows:
     *      o flags & ASYNC_SPD_MASK == ASYNC_SPD_CUST
     *      o custom_divisor set to baud_base / your_new_baudrate
     * ** Step 3 is done courtesy of code borrowed from serial.c
     *    I should really spend some time and separate + move this common
     *    code to serial.c, it is replicated in nearly every serial driver
     *    you see.
     */

    /* 1. Get the baud rate from the tty settings, this observes
          alt_speed hack */

    baud = tty_get_baud_rate(tty);
    dev_dbg(dev, "%s - tty_get_baud_rate reports speed %d\n", __func__, baud);

    /* 2. Observe async-compatible custom_divisor hack, update baudrate
       if needed */

    /*
my_style:   !!! ???
    if (baud == 38400 &&
        ((priv->flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST) &&
         (priv->custom_divisor)) {
        baud = priv->baud_base / priv->custom_divisor;
        dev_dbg(dev, "%s - custom divisor %d sets baud rate to %d\n",
            __func__, priv->custom_divisor, baud);
    }*/

    /* 3. Convert baudrate to device-specific divisor */
dev_dbg (dev, "get_ftdi_divisor 1\n");
    if (!baud)
        baud = 9600;
    switch (priv->chip_type) {
    case SIO: /* SIO chip */
        switch (baud) {
        case 300: div_value = ftdi_sio_b300; break;
        case 600: div_value = ftdi_sio_b600; break;
        case 1200: div_value = ftdi_sio_b1200; break;
        case 2400: div_value = ftdi_sio_b2400; break;
        case 4800: div_value = ftdi_sio_b4800; break;
        case 9600: div_value = ftdi_sio_b9600; break;
        case 19200: div_value = ftdi_sio_b19200; break;
        case 38400: div_value = ftdi_sio_b38400; break;
        case 57600: div_value = ftdi_sio_b57600;  break;
        case 115200: div_value = ftdi_sio_b115200; break;
        } /* baud */
        if (div_value == 0) {
            dev_dbg(dev, "%s - Baudrate (%d) requested is not supported\n",
                __func__,  baud);
            div_value = ftdi_sio_b9600;
            baud = 9600;
            div_okay = 0;
        }
        break;
    case FT8U232AM: /* 8U232AM chip */
        if (baud <= 3000000) {
            div_value = ftdi_232am_baud_to_divisor(baud);
        } else {
            dev_dbg(dev, "%s - Baud rate too high!\n", __func__);
            baud = 9600;
            div_value = ftdi_232am_baud_to_divisor(9600);
            div_okay = 0;
        }
        break;
    case FT232BM: /* FT232BM chip */
    case FT2232C: /* FT2232C chip */
    case FT232RL: /* FT232RL chip */
dev_dbg (dev, "get_ftdi_divisor 2\n");
    case FTX:     /* FT-X series */
        if (baud <= 3000000) {
            __u16 product_id = le16_to_cpu(
                port->serial->dev->descriptor.idProduct);
dev_dbg (dev, "get_ftdi_divisor 3\n");
            if (((FTDI_NDI_HUC_PID == product_id) ||
                 (FTDI_NDI_SPECTRA_SCU_PID == product_id) ||
                 (FTDI_NDI_FUTURE_2_PID == product_id) ||
                 (FTDI_NDI_FUTURE_3_PID == product_id) ||
                 (FTDI_NDI_AURORA_SCU_PID == product_id)) &&
                (baud == 19200)) {
                baud = 1200000;
            }
dev_dbg (dev, "get_ftdi_divisor 4\n");
my_dev = dev;
            div_value = ftdi_232bm_baud_to_divisor(baud);
        } else {
            dev_dbg(dev, "%s - Baud rate too high!\n", __func__);
            div_value = ftdi_232bm_baud_to_divisor(9600);
            div_okay = 0;
            baud = 9600;
        }
        break;
    case FT2232H: /* FT2232H chip */
    case FT4232H: /* FT4232H chip */
    case FT232H:  /* FT232H chip */
        if ((baud <= 12000000) && (baud >= 1200)) {
            div_value = ftdi_2232h_baud_to_divisor(baud);
        } else if (baud < 1200) {
            div_value = ftdi_232bm_baud_to_divisor(baud);
        } else {
            dev_dbg(dev, "%s - Baud rate too high!\n", __func__);
            div_value = ftdi_232bm_baud_to_divisor(9600);
            div_okay = 0;
            baud = 9600;
        }
        break;
    } /* priv->chip_type */

    if (div_okay) {
        dev_dbg(dev, "%s - Baud rate set to %d (divisor 0x%lX) on chip %s\n",
            __func__, baud, (unsigned long)div_value,
            ftdi_chip_name[priv->chip_type]);
    }

    tty_encode_baud_rate(tty, baud, baud);
    return div_value;
}

#define DWORD   uint32_t
//DWORD ftdi_set_baudrate (DWORD);

void main (int argc, char **argv) {
    if (argc > 1) {
        unsigned lBaudRate = atol(argv[1]);
        unsigned lDevisor;

        struct ftdi_private     Ftdi;
        struct usb_serial       UsbSerial;
        struct usb_serial_port  UsbSerialPort;
        struct usb_device       UsbDevice;
        struct tty_struct       Tty;

        memset (&UsbSerial,0-1,sizeof(UsbSerial));
        memset (&UsbSerialPort,0-1,sizeof(UsbSerialPort));
        memset (&UsbDevice,0-1,sizeof(UsbDevice));
        memset (&Tty,0-1,sizeof(Tty));
        memset (&Ftdi,0-1,sizeof(Ftdi));
        // /////////////////////////////////////////////////////////////////////
        UsbSerialPort.serial = &UsbSerial;
        UsbSerialPort.ftdi = &Ftdi;
        UsbSerial.dev = &UsbDevice;

        // /////////////////////////////////////////////////////////////////////
        UsbDevice.descriptor.idProduct = 0x6001;
        Tty.baud = lBaudRate;   //  <-  !!!!!!!  ^_^
        //  "baud" извлекается из структуры "tty"   2015.03.06
        // предыдущие два параметра - "как в кино, где всё логично и романтично" :)   а вот далее ...
        Ftdi.chip_type = FT232RL;
        //  2015.03.06   chip_type задаётся в
        //      proc "ftdi_determine_type"  ::: version = "le16_to_cpu(udev->descriptor.bcdDevice);"
        //          и там уж куча "развилок" :((
        // /////////////////////////////////////////////////////////////////////

        puts ("");
        lDevisor = get_ftdi_divisor (&Tty, &UsbSerialPort);
        printf (
            "\nalso:\n"
            "   BaudRate= %u\n"
            "   divisor = %4.4X.%4.4X == %u.\n"
            , lBaudRate, lDevisor>>16, lDevisor & 0xFFFF, lDevisor
        );
    } else
        puts (
        "\nUsage:\n"
        "   ftdi.exe  BaudRate\n"
        "\n"
        );
}
