
#ifndef _USBUART_MSG_H_
#define _USBUART_MSG_H_


#include "usb_desc.h"
#include "usbuart.h"

    #pragma pack(push,1)    ///////////////////////////////////////////////////////////////////////////////////////////////

//  user_space   definitions
    uint32_t  sample_request_ioctl_t (void * input, uint32_t inp_size, void * output, uint32_t out_size);
//          result:  0  -ERROR

#define USBUART_CMD_FIRST_DEVICE_USBUART    1
//          sample_request_ioctl   (not_use, not_use, _Request_FindUsbUart * output, sizeof(_Request_FindUsbUart))
#define USBUART_CMD_NEXT_DEVICE_USBUART     (USBUART_CMD_FIRST_DEVICE_USBUART   +1)
//          sample_request_ioctl   (not_use, not_use, _Request_FindUsbUart * output, sizeof(_Request_FindUsbUart))
#define USBUART_CMD_GET_EVENT_CHANGE_LIST   (USBUART_CMD_NEXT_DEVICE_USBUART    +1)
//          sample_request_ioctl   (not_use, not_use, not_use, not_use)
#define USBUART_CMD_OPEN_UART               (USBUART_CMD_GET_EVENT_CHANGE_LIST  +1)
//          sample_request_ioctl   (int devnum, not_use, not_use, not_use)
//          result: HANDLE of device  or 0  :ERROR
#define USBUART_CMD_CLOSE_DEVICE            (USBUART_CMD_OPEN_UART              +1)
//          sample_request_ioctl   (not_use, uint32 Handle, not_use, not_use)
#define USBUART_CMD_GET_BYTE                (USBUART_CMD_CLOSE_DEVICE           +1)
//          sample_request_ioctl   (not_use, uint32 Handle, char *output, 1)
#define USBUART_CMD_GET_PACKET              (USBUART_CMD_GET_BYTE               +1)
//          sample_request_ioctl   (not_use, uint32 Handle, char *output, uint32 size_out)
//          return: count bytes  or  -1 of error
#define USBUART_CMD_SEND_PACKET             (USBUART_CMD_GET_PACKET             +1)
//          sample_request_ioctl   (not_use, uint32 Handle, char *output, uint32 sizeout)
#define USBUART_CMD_GET_MAX_PACKET_LENGTH   (USBUART_CMD_SEND_PACKET            +1)
//          sample_request_ioctl   (not_use, uint32 Handle, uint32 * output, 4)
#define USBUART_CMD_SETUP_LINE              (USBUART_CMD_GET_MAX_PACKET_LENGTH  +1)
//          sample_request_ioctl   (not_use, uint32 Handle, _LineCoding *coding,  sizeof(_LineCoding))
#define USBUART_CMD_SET_STATUS_LINE         (USBUART_CMD_SETUP_LINE             +1)
//          sample_request_ioctl   (not_use, uint32 Handle, uint16 Code, not_use)
#define USBUART_CMD_GET_STATUS_LINE         (USBUART_CMD_SET_STATUS_LINE        +1)
//          sample_request_ioctl   (not_use, uint32 Handle, uint16 *code, 2)
#define USBUART_CMD_EVENT_STATUS            (USBUART_CMD_GET_STATUS_LINE        +1)
//          sample_request_ioctl   (not_use, uint32 Handle, uint8 *code, 1)
    //  response of USBUART_CMD_EVENT_STATUS
    #define EVENT_RECEIVED      1 //  bit 0   -received, there is arrival
    #define EVENT_STATUS_LINE   2 //  bit 1   -change of line status (CTS, DSR, CD, RI e.t.c)
    #define EVENT_SEND_IS_FREE  4 //  bit 2   -possible of send
#define tst_GET_DEVICE_STRUC      (USBUART_CMD_EVENT_STATUS           +1)
//          sample_request_ioctl   (not_use, uint32 Handle, void *output, sizeof(_DeviceUsbUart))
//
//  для прочих USB-шек  2016.10.06
#define USBUART_CMD_OPEN_RAW                (tst_GET_DEVICE_STRUC  +1)
//          sample_request_ioctl   (int devnum, not_use, not_use, not_use)
//          result: HANDLE of device  or 0  :ERROR
#define USBUART_CMD_CTRL_RAW                (USBUART_CMD_OPEN_RAW  +1)
//          sample_request_ioctl   (_ControlRequest *, uint32 Handle, void *bmData, int timeout_HW__endpoint_LW)
#define USBUART_CMD_BULK_RAW                (USBUART_CMD_CTRL_RAW  +1)
//          sample_request_ioctl   (void *bmData, int iSizeData, uint32 Handle, int timeout_HW__endpoint_LW)
#define USBUART_CMD_GET_CONF_DESC           (USBUART_CMD_BULK_RAW +1)
//          sample_request_ioctl   (int devnum, not_use, void *buf, int size)

typedef struct Request_FindUsbUart {
    void   *    device;                 //  reserved
    int iDevNum;
    char        szDescription[64];
    uint32_t    iFlags;                 //  status device
        #define PRESENCE_BIT0   1   //  бит 0   device is present
        #define BUSY_BIT1       2   //  бит 1   device is busy (used in user-space)
        #define UART_BIT2       4   //  бит 2   device is UART mode
    struct usb_device_desc_std DeviceDescriptor;
} _Request_FindUsbUart;

    #pragma pack(pop)   ///////////////////////////////////////////////////////////////////////////////////////////////

#endif  //  #ifndef _USBUART_MSG_H_ ...
