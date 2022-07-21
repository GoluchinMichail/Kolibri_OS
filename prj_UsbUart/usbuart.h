
#ifndef _USBUART_H_
#define _USBUART_H_

#include "kos_kernel_api.h"
#include "cdc-acm.h"

    #pragma pack(push,1)    ///////////////////////////////////////////////////////////////////////////////////////////////

#define USBMAX_PACKET_SIZE_64   64          //  maxpacketsize   +/-   :)
#define STDPOLLINTERVAL         100
#define USB_DIR_IN              0x80        //  seventh bit in 1

#define FTDI_VID    0x0403
#define CP210X_VID  0x10C4

typedef struct ControlRequest {
    uint8_t     bmRequestType;
    uint8_t     bRequest;
    uint16_t    wValue;
    uint16_t    wIndex;
    uint16_t    wLength;
} _ControlRequest;

typedef struct KnownDevice {
    uint16_t    wVID;
    uint16_t    wPID;
    uint8_t    bInterface;
    uint8_t    bIntrPoint; //  interrupt endpoint (status port/modem)
    uint8_t    bOutPoint;
    uint8_t    bInpPoint;
// 2015.04.24 not use    uint8_t    bFuncType;  //  type of operation   (CDC-ACM, Silicon Labs-firmware, FTDI-firmware)
    char    szDescription[64];
} _KnownDevice;

typedef struct Lcp210x_context {    //  2017.02.15
    int     iStatus;
        //  0   -not use
        //  1   -requirement of service
        //  2   -it is serviced
    uint8_t     bmRequestType;
    uint8_t     bRequest;
    uint16_t    wValue;
    uint16_t    wIndex;
    uint16_t    wLength;
    void *buffer;
} _Lcp210x_context;

typedef struct KosPipe {
    int  iEP;                       //  EndpointAddress
    int  iEndpointType;             //  CTRL_TYPE_ENDPOINT ISOC_TYPE_ENDPOINT BULK_TYPE_ENDPOINT INTR_TYPE_ENDPOINT
    void *pipe;                     //  created to USBOpenPipe (...)
    void *pEvent;                   //  created to CreateEvent (...)
    uint32_t IdEvent;               //      ...
    uint32_t iStatus_CallBack;      //  value "status" of callback-procedure
// 2015.03.27    uint32_t bActive;            //  !0 -running transaction   0 -working callback / free
    uint32_t iSimpleCounter;
    void *dev;
// 2017.06.21
    uint32_t iCountRW;               //  read/write of transaction
} _KosPipe;

typedef struct RecSender {
    uint16_t wBytes;
    uint16_t wActualeBytes;         //  wSendBytes == wBytes == 0   =>  issue is finished   :)   buffer is free
    uint8_t bmData [1024 +1];
} _RecSender;

typedef struct DeviceUsbUart {
    int  devnum;    //  условный номер устройства (НЕ USB-адрес !!)
    void *last; //  previous    device
    void *next; //  following   device

    void *context;  //  для CP-210x  ^_^    2017.02.15

    _usb_device_desc_std       DeviceDescriptor;   //  BCD e.t.c   :|    2015.04.09

    uint32_t iFlags;
    //  #define PRESENCE_BIT0   1   //  бит 0   устройство присутствует
    //  #define BUSY_BIT1       2   //  бит 1   устройство захвачено (используется выше стоящим слоем)
    //  #define UART_BIT2       4   //  бит 2   устройство в режиме UART    2017.02.09

    _KosPipe **apPipes;
    //  список ссылок на пипы, последний - NULL

    uint32_t lDescriptor;           //  with BUSY_BIT1  !!!


    _LineCoding LineCoding;         //  used by MSG_SETUP_LINE

    uint8_t bmStatusLine [10];
    uint8_t bChangesStatusLine;     //  simple counter
    uint16_t wStatusLine;           //  status DSR, CTS, CD, RI
        //  bits:    7      3     2       0
        //          CTS    RI   DSR     CDC
        //         0x80    8     2      1
        //  CDC_ACM_SIGNAL_xxx   (in cdc-acm.h)
        //  Table 31: UART State Bitmap Values      "CDC PSTN Subclass"  "Revision 1.2"

    uint8_t bEventsUART;

    uint8_t bmReceiver [USBMAX_PACKET_SIZE_64];
    uint8_t bmIncomingQueue [USBMAX_PACKET_SIZE_64 *16];  //  64*16 = 1024  :)
    _MUTEX  MutexIQ;
    uint16_t iBytesIQ, iHeadIQ, iTailIQ, iCounterOverflowIQ;

    _RecSender SenderStruct;    //  2015.02.27 [16];         //   64*16 =   1024  :)
                                //  один буфер - одна пипа ;)   много пипов - хорошо
                                //  , но не гарантирована последовательность посылки - очень плохо :((

    _ControlRequest ControlRequest;
    //  2017.02.14
    //  2017.02.15  _ControlRequest ControlRequest_CP;

    _KnownDevice  *known;
    void *pipe0;    //  связующее с собственно USB-устройством
} _DeviceUsbUart;

#define INFINITE        -1     //  wait terminations of request !
#define NO_WAIT         0
#define NO_INFINITE     NO_WAIT

//       2017.01.24
//  linux/usb.h
/*static inline unsigned int __create_pipe(struct usb_device *dev,
                unsigned int endpoint)
{
        return (dev->devnum << 8) | (endpoint << 15);
}*/
#define PIPE_CONTROL        0
#define PIPE_BULK           0
#define PIPE_ISOCHRONOUS    0
#define PIPE_INTERRUPT      0
#define __create_pipe(dev,poINT)    poINT
// Create various pipes...
#define usb_sndctrlpipe(dev, endpoint)  \
        ((PIPE_CONTROL << 30) | __create_pipe(dev, endpoint))
#define usb_rcvctrlpipe(dev, endpoint)  \
        ((PIPE_CONTROL << 30) | __create_pipe(dev, endpoint) | USB_DIR_IN)
#define usb_sndisocpipe(dev, endpoint)  \
        ((PIPE_ISOCHRONOUS << 30) | __create_pipe(dev, endpoint))
#define usb_rcvisocpipe(dev, endpoint)  \
        ((PIPE_ISOCHRONOUS << 30) | __create_pipe(dev, endpoint) | USB_DIR_IN)
#define usb_sndbulkpipe(dev, endpoint)  \
        ((PIPE_BULK << 30) | __create_pipe(dev, endpoint))
#define usb_rcvbulkpipe(dev, endpoint)  \
        ((PIPE_BULK << 30) | __create_pipe(dev, endpoint) | USB_DIR_IN)
#define usb_sndintpipe(dev, endpoint)   \
        ((PIPE_INTERRUPT << 30) | __create_pipe(dev, endpoint))
#define usb_rcvintpipe(dev, endpoint)   \
        ((PIPE_INTERRUPT << 30) | __create_pipe(dev, endpoint) | USB_DIR_IN)

#define USB_TYPE_VENDOR			(0x02 << 5)
#define USB_DIR_OUT			0		/* to device */
#define USB_DIR_IN			0x80		/* to host */
#define USB_RECIP_MASK			0x1f
#define USB_RECIP_DEVICE		0x00
#define USB_RECIP_INTERFACE		0x01
#define USB_RECIP_ENDPOINT		0x02
#define USB_RECIP_OTHER			0x03
/* From Wireless USB 1.0 */
#define USB_RECIP_PORT			0x04
#define USB_RECIP_RPIPE		0x05

    #pragma pack(pop)   ///////////////////////////////////////////////////////////////////////////////////////////////

#endif  //  ifndef _USBUART_H_ ...
