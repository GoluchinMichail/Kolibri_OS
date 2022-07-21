
//#include <stdlib.h>
#include "cdc-acm.h"
#include "cp210x.h"
#include "ftdi.h"
#include "ch340.h"
#include "usb_desc.h"
#include "kos_kernel_api.h"
#include "windef.h"

#include "usbuart.h"
#include "usbuart_msg.h"
#include "clib_simple.h"

#ifndef MIN
    //  2017.07.06
    //  GCC  warning: implicit declaration of function "min" [-Wimplicit-function-declaration]
    #define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

// ////////  for inverse compatibility with COM-port (PC serial port / RS-232)  ///////////
//  FTDI    signals compatible with COM-style
//  CP210x  signals compatible with COM-style
// ////////
    //  COM-port    status of modem
    #define MODEM_SIGNAL_CTS  0x10
    #define MODEM_SIGNAL_DSR  0x20
    #define MODEM_SIGNAL_RING 0x40
    #define MODEM_SIGNAL_CDC  0x80
    //  COM-port    status of line
    //                              1       data is ready
    #define COM_SIGNAL_OVERUN       2
    #define COM_SIGNAL_PARITY       4
    #define COM_SIGNAL_FRAME        8
    #define COM_SIGNAL_BREAK        0x10
    #define COM_SHIFT_EMPTY         0x20    //  shift register of transmitter is empty
    #define COM_TRANS_EMPTY         0x40    //  register of transmitter is empty
    #define COM_SIGNAL_TIMEOUT      0x80    //  device unconnected :(
// ////////////////////////////////////////////////////////////////////////////////////////

uint32_t ftdi_232bm_baud_to_divisor (int baud);

static _KnownDevice Known_Device[] = {
//     VID    PID     Intrface
//                     \  IntrPoint
//                     \   \ OutPoint
//                     \   \  \ InpPoint
    {0x10C4, 0xEA60,   0,  0, 1, 1,   "port  CP210x USB to UART Bridge Virtual COM Port, Silicon Labs"},
    {0x0403, 0x6001,   0,  0, 2, 1,   "port  FT232  USB-Serial  UART"},
    {0x0403, 0x6015,   0,  0, 2, 1,   "port  FT230X USB-Serial  UART"},
    {0x067B, 0x2303,   0,  1, 2, 3,   "port  PL-2303  USB to UART Bridge Virtual COM Port, Prolific"},
// 2022.05.20   проверить не могу - колибри плохо себя ведёт "на дачном большом" :))
{0x1a86, 0x7523,       0,  1, 2, 2,   "QinHeng Electronics CH340 serial converter"},
    {0x16D5, 0x6501,   0,  1, 2, 2,   "modem AnyData ADU-310A     CDMA-450"},
    {0x12D1, 0x1001,   0,  1, 2, 2,   "modem Huawei E1550"},
    /* 2021.06.03  %\
    {0x1a86, 0x7523,   0,  0, 1, 1,   "CH-341 %/"},*/
//  std CDC\ACM device :  IntrPoint != 0
//  {0x19D2, 0x0016,   1,  3, 2, 2,   "modem ZTE"},
//  {0x19D2, 0x0016,   2,  5, 3, 4,   "modem ZTE"},
};
static _KnownDevice *knownDevice = Known_Device;

static _DeviceUsbUart *rootDevice = NULL;

static BOOL booChangeList = FALSE; //  fact of change the list of devices

typedef int __stdcall _ProcCmd (ioctl_t *request);
//  return 0    is set ERROR
typedef struct {
    int iCmd;
    _ProcCmd * proc;
} _UsbUart_Procs;

//--
static _KosPipe * GetCreatePipe (_DeviceUsbUart *dev, int iEP, int iType, int iSizeMaxPacket, int iInterval) {
    _KosPipe * endp = NULL;
    int iPresentCount = 0, i;
    //dbg (_ERR, "GetCreatePipe (ep=%u,type=%u,size=%u,time=%u)\n", iEP, iType, iSizeMaxPacket, iInterval);
    //#define ALWAYS_CREATE 0x100 //  2017.01.30
    //if ((iEP & ALWAYS_CREATE) ==0)
    if (dev->apPipes) for (i=0;dev->apPipes[i];i++) {
        if (dev->apPipes[i]->iEP==iEP
        &&  dev->apPipes[i]->iEndpointType == iType
        ) {
            endp = dev->apPipes[i];
            break;
        }
        iPresentCount++;
    }
    //iEP &= ~ ALWAYS_CREATE; //  2017.01.30
    if (endp==NULL) {
        _KosPipe **newpipes;
        int iCount = (iPresentCount+1) +1;
        //dbg (_ERR, "CreatePipe (ep=%u,type=%u,size=%u,time=%u)\n", iEP, iType, iSizeMaxPacket, iInterval);
        //dbg (_ERR, "    new Count = %u\n", iCount);
        newpipes = coreKmalloc (sizeof(*dev->apPipes) *iCount);
        if (newpipes) {
            memset (newpipes, 0, sizeof(*dev->apPipes) *iCount);
            if (iPresentCount) {
                memcpy (newpipes, dev->apPipes, sizeof(*dev->apPipes) * iPresentCount);
                coreKfree (dev->apPipes);
            }
            dev->apPipes = newpipes;
            dev->apPipes [iCount-1] = NULL;    //  set ended
            dev->apPipes [iCount-2] = coreKmalloc (sizeof(**dev->apPipes));
            if (dev->apPipes[iCount-2]) {
                endp = dev->apPipes [iCount-2];
                memset (endp, 0, sizeof(**dev->apPipes));
                //  закрытие ресурса с ep=0 (аналог pipe0) приводит к краху !!
                //  2017.01.31
                //if ((iEP&(~USB_DIR_IN))==0 && iType==CTRL_TYPE_ENDPOINT) {
                //    endp->pipe = dev->pipe0;
                //} else {
                    endp->pipe = coreUSBOpenPipe (dev->pipe0, iEP, iSizeMaxPacket, iType, iInterval);
                //}
                if (endp->pipe) {
                    endp->dev = dev;    //  2017.01.21
                    endp->iEP = iEP;
                    endp->iEndpointType = iType;
                    endp->pEvent = coreCreateEvent (MANUAL_DESTROY,NULL,&endp->IdEvent);
                    if (!endp->pEvent) {
                        coreKfree (dev->apPipes[iCount-2]);
                        dev->apPipes[iCount-2] = NULL;
                        dbg (_ERR, "-error CreateEvent\n");
                    }
                } else {
                    coreKfree (dev->apPipes[iCount-2]);
                    dev->apPipes[iCount-2] = NULL;
                    dbg (_ERR, "-error USBOpenPipe\n");
                }
            } else
                dbg (_ERR, "-error malloc  * apPipes\n");
        } else  // if (newpipes) ...
            dbg (_ERR, "-error malloc apPipes\n");
        if (endp) {
            char Str[48];
            snprintf (Str, sizeof(Str), "  created pipe  %p  ", endp);
            if (iType==CTRL_TYPE_ENDPOINT)
                strcat (Str, "  ctrl");
            else if (iType==BULK_TYPE_ENDPOINT)
                strcat (Str, "  bulk");
            else if (iType==INTR_TYPE_ENDPOINT)
                strcat (Str, "  intr");
            else
                strcat (Str, "  ishr");
            if (iEP & USB_DIR_IN)
                strcat (Str, "  Recv_");
            else
                strcat (Str, "  Send_");
            snprintf (&Str[strlen(Str)], 4, "%i\n", iEP & ( ~ USB_DIR_IN));
            dbg (_ERR, Str);
        }
    } // if (endp==NULL) ...
    if (endp)
        ;//dbg (_ERR, "    -OK \n");
    else
        dbg (_ERR, "    -ERROR\n");

    return endp;
}

//--
static void PutChar_Incoming (_DeviceUsbUart *dev, BYTE Byte) {
    if (dev->iBytesIQ < sizeof(dev->bmIncomingQueue)) {
//dbg ("   PutChar '%C'\n", Byte);
        dev->bmIncomingQueue [dev->iTailIQ] = Byte;
        dev->iBytesIQ ++;
        dev->iTailIQ ++;
        if (dev->iTailIQ >= sizeof(dev->bmIncomingQueue))
            dev->iTailIQ = 0;
        dev->bEventsUART |= EVENT_RECEIVED;  //  there is arrival
    } else {
        dev->iCounterOverflowIQ ++;
//dbg ("      CounterOverflowIQ\n");
    }
}

//--
static BOOL GetChar_Incoming (_DeviceUsbUart *dev, BYTE *byte) {
    BOOL iResult = FALSE;
//dbg ("GetChar ...\n");
    if (dev->iBytesIQ) {
        *byte = dev->bmIncomingQueue [dev->iHeadIQ];
//dbg ("... OK getchar\n");
        dev->iBytesIQ --;
        dev->iHeadIQ ++;
        if (dev->iHeadIQ >= sizeof(dev->bmIncomingQueue))
            dev->iHeadIQ = 0;
        if (dev->iBytesIQ == 0)
            dev->bEventsUART &= ~ EVENT_RECEIVED;  //  clear (set first bit   -   there is arrival)
        iResult = TRUE;
    }
    return iResult;
}

//--
/*static */void __stdcall Callback_Ctrl DeF (void* pipe, int status, void* buffer, int length, void* calldata) {
    _KosPipe *endp = calldata;
    if (status) {
        dbg (_WARN, "Callback_Ctrl  status= %Xh\n", status);
    }
    endp->iStatus_CallBack = status;
    endp->iSimpleCounter ++;
    endp->iCountRW = length;  // 2017.06.21
    if (endp->pEvent)// 2015.04.01
        coreRaiseEvent ((void*)endp->pEvent, endp->IdEvent, 0, NULL);
}

static int usb_bulk_msg (_DeviceUsbUart *dev, unsigned int pipe, void *data, WORD size, int timeout, void *callback);

//--
static void __stdcall Callback_BulkOut DeF (void* pipe, int status, void* buffer_not_use, int length, void* calldata) {
    _KosPipe *endp = calldata;
    _DeviceUsbUart *dev = endp->dev;

    if (status) {
        dbg (_WARN, "Callback_BulkOut  status= %Xh\n", status);
    }
    dev->SenderStruct.wActualeBytes += length;
    if (dev->SenderStruct.wActualeBytes < dev->SenderStruct.wBytes) {
        BYTE *bNextBuffer = & dev->SenderStruct.bmData [dev->SenderStruct.wActualeBytes];
        length = dev->SenderStruct.wBytes  -  dev->SenderStruct.wActualeBytes;
        if (length > USBMAX_PACKET_SIZE_64)
            length = USBMAX_PACKET_SIZE_64;
//dbg ("do_send %u bytes\n", length);
        usb_bulk_msg (dev, endp->iEP, bNextBuffer, length, NO_WAIT, Callback_BulkOut);
    } else {
//dbg ("back OUT status= %X  len= %i  '%C'\n", status, length, *((char*) buffer));
        endp->iSimpleCounter ++;

        //  marked of free buffer
        dev->SenderStruct.wBytes = 0;
        dev->SenderStruct.wActualeBytes = 0;
        dev->bEventsUART |= EVENT_SEND_IS_FREE; //  sender is free

        if (endp->pEvent)// 2015.04.01
            coreRaiseEvent ((void*)endp->pEvent, endp->IdEvent, 0, NULL);
    }
}

/*static */int usb_control_msg DeF (_DeviceUsbUart *dev, unsigned int pipe,
       BYTE request,  BYTE requesttype, WORD value, WORD index, void *data, WORD size, int timeout, void *callback) {
    int iResult = 0;
    _KosPipe *endp = GetCreatePipe (dev, pipe, CTRL_TYPE_ENDPOINT, USBMAX_PACKET_SIZE_64, 0);
    if (endp) {
        memset (&dev->ControlRequest,0,sizeof(dev->ControlRequest));
        dev->ControlRequest.bmRequestType = requesttype;
        dev->ControlRequest.bRequest = request;
        dev->ControlRequest.wValue = value;
        dev->ControlRequest.wIndex = index;
        dev->ControlRequest.wLength = size;
        //  pipe    - known of characteristic destination ENDPOINT !!!   (bulk, interrupt and isochor)
        endp->iCountRW = 0;
        iResult = (int) coreUSBControlTransferAsync (endp->pipe, &(dev->ControlRequest)
            , data, size, callback, endp, 0);
        if (iResult) {
            if (timeout==INFINITE) {
                coreWaitEvent (endp->pEvent, endp->IdEvent);
            } else if (timeout != NO_WAIT)
                coreWaitEventTimeout (endp->pEvent, endp->IdEvent, timeout);
            iResult = (int)endp;
        } else
            dbg (_ERR, "-ERROR  coreTransferAync\n");
    } else
        dbg (_ERR, "-ERROR usb_control_msg  GetCReatePipe\n");
    return iResult;
}

//--
static int usb_bulk_msg DeF (_DeviceUsbUart *dev, unsigned int pipe, void *data, WORD size, int timeout, void *callback) {
    int iResult;
    _KosPipe *endp = GetCreatePipe (dev, pipe, BULK_TYPE_ENDPOINT, USBMAX_PACKET_SIZE_64, 0);
    if (endp) {
        //  pipe    - known of characteristic destination ENDPOINT !!!   (bulk, interrupt and isochor)
        endp->iCountRW = 0;
        iResult = (int) coreUSBNormalTransferAsync (endp->pipe, data, size, callback, endp, POSSIBLE_LESS);
        if (iResult) {
            if (timeout==INFINITE)
                coreWaitEvent (endp->pEvent, endp->IdEvent);
            else
                coreWaitEventTimeout (endp->pEvent, endp->IdEvent, 0);// timeout);
            iResult = (int) endp;
        }
    }
    return iResult;
}

//--
static void __stdcall Callback_Intr DeF (void* pipe, int status, void* buffer, int length, void* calldata) {
    _KosPipe *endp = calldata;
    _DeviceUsbUart *dev = endp->dev;

//    endp->bActive = 0;
    if (status) {
        dbg (_WARN, "Callback_Intr  status= %Xh\n", status);
    }
    endp->iStatus_CallBack = status;
    endp->iSimpleCounter ++;
    if (status != USB_STATUS_CLOSED
        && (dev->iFlags & PRESENCE_BIT0)
        && (dev->iFlags & BUSY_BIT1)
    ) {//  ups - disconnected !

        if (CP210X_VID == dev->known->wVID) {
//  CP210x
            int booRestart;
            if (status==0 && length>1) {
                WORD *wStatus = (WORD*) &(dev->bmStatusLine);
                if (dev->wStatusLine != *wStatus) {
                    dev->wStatusLine = *wStatus;
                    dev->bChangesStatusLine++;
                    dev->bEventsUART |= EVENT_STATUS_LINE;
                    if (endp->pEvent)// 2015.04.01
                        coreRaiseEvent ((void*)endp->pEvent, endp->IdEvent, 0, NULL);
                }
            }
            booRestart = -1;    //  YES
            if (dev->context) {
                _Lcp210x_context  *context = dev->context;
                if (context->iStatus==1) {
                //  requirement of service
//  static int usb_control_msg DeF (_DeviceUsbUart *dev, unsigned int pipe,
//       BYTE request,  BYTE requesttype, WORD value, WORD index, void *data, WORD size, int timeout, void *callback) {
                    context->iStatus = 2;   //  принято в обработку
                    booRestart = 0; //  NOoooo
                    usb_control_msg (dev,  endp->iEP
                        , context->bRequest, context->bmRequestType
                        , context->wValue, context->wIndex
                        , context->buffer, context->wLength, NO_WAIT, Callback_Intr
                    );
                } else if (context->iStatus==2) {
                //  it is serviced
                    context->iStatus = 0;   //  ended :)
                }
            }   //  if (dev->context) ...
            //  restart ...
            //  2015.03.11  see below ( Callback_BulkInp )
// 2017.02.14
// 2017.02.15
            if (booRestart)
                usb_control_msg (dev,  endp->iEP,
                    CP210X_REQUEST_GET_MDMSTS, CP210X_REQTYPE_TO_HOST,
                    0,dev->known->bInterface, &(dev->bmStatusLine), 1, NO_WAIT, Callback_Intr
                );
        } else {
//  CDC/ACM
            if (!status && (length > CDC_ACM_STATUS_BYTE)) {
                WORD *statline = (WORD*) &(dev->bmStatusLine[CDC_ACM_STATUS_BYTE]);
                if (dev->wStatusLine != *statline) {
                    dev->wStatusLine = *statline;
                    //  changed status line !
dbg (_WARN, "changed status line : %Xh\n", dev->wStatusLine);
                    dev->bChangesStatusLine++;
                    dev->bEventsUART |= EVENT_STATUS_LINE;
                    if (endp->pEvent)// 2015.04.01
                        coreRaiseEvent ((void*)endp->pEvent, endp->IdEvent, 0, NULL);
                }
            }
//            endp->bActive = -1;
            //  "Intr-pipe" не перезапускает транзакцию, приходится всё делать самому
            //      :))  2015.01.20
            //  ЕСТЬ СОМНЕНИЕ - КАЖЕТСЯ ОНО РАБОТАЕТ - ПРОВЕРИТЬ    2015.03.17
            //  работает. не перезапускается. ВСЁ САМОМУ , рука устала :)) 2015.03.17
            usb_bulk_msg (dev, endp->iEP, dev->bmStatusLine, sizeof(dev->bmStatusLine), NO_WAIT, Callback_Intr);
        }
    } else {    // if  busy && presence  ...
        //  2017.02.15
        if (CP210X_VID == dev->known->wVID && dev->context) {
            _Lcp210x_context  *context = dev->context;
            context->iStatus = 0;
        }
        dbg (_WARN, "NO RESTART  INTR\n");
    }
}

//--
static void __stdcall Callback_BulkInp DeF (void* pipe, int status, void* buffer, int length, void* calldata) {
    _KosPipe *endp = calldata;
    _DeviceUsbUart *dev = endp->dev;
    char *chars = (char *) buffer;
//dbg ("back IN status= %X  len= %i  '%C'\n", status, length, *chars);
    if (status)
        dbg (_WARN, "Callback_BulkInp  status= %Xh\n", status);
    if (dev->known->bIntrPoint==0) {
        if (FTDI_VID == dev->known->wVID) {
//  FTDI
            if (length >= 2) {
                //  *chars     -STATUS LINE !! 2015.03.04
                //  details at the end  in file:  ftdi_sio.h   linux-3-18
                //  bits:    7   6   5    4
                //          CD  RI  DSR  CTS
                //  важно: там-же описан формат записи - то лажа !! 2015.04.15
                WORD *status = (WORD *) chars;
                if (dev->wStatusLine != *status) {
                    dev->wStatusLine = *status;
                    dev->bChangesStatusLine++;
                    dev->bEventsUART |= EVENT_STATUS_LINE;
//dbg ("dev->bStatusLine= %Xh\n", dev->bStatusLine);
                    {_KosPipe *endp =
                        GetCreatePipe (dev, dev->known->bIntrPoint | USB_DIR_IN, INTR_TYPE_ENDPOINT, USBMAX_PACKET_SIZE_64, 0);
                    //  этот ресурс пока НЕ СОЗДАЁТСЯ !!  2015.03.05
                    if (endp->pEvent)// 2015.04.01
                        coreRaiseEvent ((void*)endp->pEvent, endp->IdEvent, 0, NULL);
                    }
                }
                chars += 2;
                length -= 2;
            }
        }
    }
    if (length) {
dbg (_WARN, "received %u bytes\n", length);
        /////////////////////////////////////////////////////////////////////////
        coreMutexLock (&dev->MutexIQ);
        {int i; for (i=0;i<length;i++)
            PutChar_Incoming (dev, chars[i]);
        }
        coreMutexUnlock (&dev->MutexIQ);
        /////////////////////////////////////////////////////////////////////////
//        endp->bActive = 0;
        endp->iSimpleCounter ++;
        if (endp->pEvent)// 2015.04.01
            coreRaiseEvent ((void*)endp->pEvent, endp->IdEvent, 0, NULL);
    }

    if (
    status != USB_STATUS_CLOSED
    && (dev->iFlags & PRESENCE_BIT0)    //  ZERO  ==>  ups - disconnected !
    && (dev->iFlags & BUSY_BIT1)        //  device opened !
    ) {
//        endp->bActive = -1;
        usb_bulk_msg (dev, endp->iEP, dev->bmReceiver, sizeof(dev->bmReceiver), NO_WAIT, Callback_BulkInp);
    } else
        dbg (_WARN, "NO RESTART BulkInp !!!\n");

}

//--
static _DeviceUsbUart * SeekUartDevice DeF (_DeviceUsbUart * seek_dev) {
    _DeviceUsbUart *result = NULL;    //  ERROR   !
    if (rootDevice) {
        _DeviceUsbUart *dev = rootDevice;
        //      goto first
        for (;dev->last;dev = dev->last);
        //      goto ended
        while (1) {
            if (dev == seek_dev) {
                result = dev; //  not 0  OK  :)
                break;
            } else if (dev->next)
                dev = dev->next;
            else // dev->next == NULL
                break;
        }
        //  2015.01.22  !?  uartDevice = dev;
    }
    return result;
}

static _DeviceUsbUart * SeekToDescriptor DeF (DWORD lDescriptor) {
    _DeviceUsbUart *result = NULL;    //  ERROR   !
    if (rootDevice) {
        _DeviceUsbUart *dev = rootDevice;
        //      goto first
        for (;dev->last;dev = dev->last);
        //      goto ended
        while (1) {
            if (dev->lDescriptor == lDescriptor) {
                result = dev; //  not 0  OK  :)
                break;
            } else if (dev->next)
                dev = dev->next;
            else // dev->next == NULL
         break;
        }
        //  2015.01.22  !?  uartDevice = dev;
    }
    return result;
}

static void ClosePipes (_DeviceUsbUart *dev) {
    int i;
    if (dev->apPipes) {
        for (i=0;dev->apPipes[i];i++) {
            _KosPipe *endp = dev->apPipes[i];
            if (dev->iFlags & PRESENCE_BIT0) {
            // ТОЛЬКО если устройство ещё тут  :)_
                //if (endp->pipe != dev->pipe0)
                //  2017.01.21  ^_^
                // трубу связанную с нулевой точкой - закрывать нельзя, иначе - КРАХ !
                if (endp->iEP & (~USB_DIR_IN))
                // NO null endpoint !!!
                    coreUSBClosePipe (endp->pipe);
                endp->pipe = 0;
            }
            if (endp->pEvent) {
                coreDestroyEvent (endp->pEvent, endp->IdEvent);
                endp->pEvent = 0;
                endp->IdEvent = 0;
            }
            coreKfree (dev->apPipes[i]);
            dev->apPipes[i] = NULL;
        }
    }
    coreKfree (dev->apPipes);
    dev->apPipes = NULL;

}

//--

static int __stdcall ProcCmd_GET_BYTE (ioctl_t *request) {
    int iResult = 0;    //  set ERROR
    if (request->output && request->out_size == 1) {
        DWORD lDesc = request->inp_size;
        _DeviceUsbUart *dev = SeekToDescriptor (lDesc);
        if (dev && (dev->iFlags & BUSY_BIT1) && (dev->iFlags & PRESENCE_BIT0) && (dev->iFlags&UART_BIT2)) {
            coreMutexLock (&dev->MutexIQ);
            if (dev->bEventsUART & EVENT_RECEIVED) {
                iResult = GetChar_Incoming (dev, request->output);
            }
            coreMutexUnlock (&dev->MutexIQ);
        }
    }
    return iResult;
}

static int __stdcall ProcCmd_GET_BYTES (ioctl_t *request) {
    int iResult = 0;    //  set ERROR
    if (request->output) {
        DWORD lDesc = request->inp_size;
        _DeviceUsbUart *dev = SeekToDescriptor (lDesc);
        if (dev && (dev->iFlags & BUSY_BIT1) && (dev->iFlags & PRESENCE_BIT0) && (dev->iFlags&UART_BIT2)) {
            unsigned i;
            char *bOutp = request->output;
            coreMutexLock (&dev->MutexIQ);
            for (i=0;i < request->out_size;i++) {
                if (GetChar_Incoming (dev, &bOutp[i]) == -1)
                    break;
            }
            iResult = i;
            coreMutexUnlock (&dev->MutexIQ);
        }
    }
    return iResult;
}

static int __stdcall ProcCmd_GET_MAX_PACKET_LENGTH (ioctl_t *request) {
    int iResult = 0;    //  set ERROR
    if (request->output && request->out_size ==4) {
        _DeviceUsbUart *dev = SeekToDescriptor (request->inp_size);
        if (dev && (dev->iFlags&UART_BIT2)) {
            DWORD *dword = (DWORD *) request->output;
            *dword = sizeof(dev->SenderStruct.bmData)  -1;
            iResult = -1;
        }
    }
    return iResult;
}

static int __stdcall ProcCmd_EVENT_STATUS (ioctl_t *request) {
    int iResult = 0;    //  set ERROR
    if (request->output && request->out_size == 1) {
        DWORD lDesc = request->inp_size;
        _DeviceUsbUart *dev = SeekToDescriptor (lDesc);
        if (dev && (dev->iFlags&UART_BIT2)) {
            BYTE * byte = request->output;
            *byte = dev->bEventsUART;
            iResult = -1;   //  set OK
        }
    }
    return iResult;
}

static int __stdcall ProcCmd_GET_STATUS_LINE (ioctl_t *request) {
    int iResult = 0;    //  set ERROR
    WORD * status_for_user = request->output;
    if (request->output && request->out_size == sizeof(*status_for_user)) {
        DWORD lDesc = request->inp_size;
        _DeviceUsbUart *dev = SeekToDescriptor (lDesc);
        if (dev && (dev->iFlags&UART_BIT2)) {
            if (FTDI_VID == dev->known->wVID
            ||  CP210X_VID == dev->known->wVID
            ) {
//  FTDI    signals compatible with COM-style
//  CP210x  signals compatible with COM-style
//  COM-style ==> CDC/ACM style   ^_^
                WORD wStatusModem = 0;
                if (dev->wStatusLine & MODEM_SIGNAL_CTS)     //  CTS
                    wStatusModem |= CDC_ACM_SIGNAL_CTS;
                if (dev->wStatusLine & MODEM_SIGNAL_DSR)     //  DSR
                    wStatusModem |= CDC_ACM_SIGNAL_DSR;
                if (dev->wStatusLine & MODEM_SIGNAL_RING)    //  RIng
                    wStatusModem |= CDC_ACM_SIGNAL_RING;
                if (dev->wStatusLine & MODEM_SIGNAL_CDC)     //  CD
                    wStatusModem |= CDC_ACM_SIGNAL_CDC;
                *status_for_user = wStatusModem;
            } else {
                *status_for_user = dev->wStatusLine;
            }
            dev->bEventsUART &= ~ EVENT_STATUS_LINE;
            //if (dev->bEventsUART & EVENT_SEND_IS_FREE)
            if (dev->SenderStruct.wBytes==0)
                *status_for_user |= CDC_ACM_MYSIGNAL_SEND_IS_FREE;   //  sender is busy
            iResult = -1;   //  set OK
        }
    }
    return iResult;
}

static int __stdcall ProcCmd_CLOSE_DEVICE (ioctl_t *request) {
    int iResult = 0;    //  set ERROR
    DWORD lDesc = (DWORD) request->inp_size;
    _DeviceUsbUart *dev = SeekToDescriptor (lDesc);
    dbg (_WARN, "CloseDevice (%Xh)\n", lDesc);
    if (dev) {
        if (dev->known->bIntrPoint==0) {    //  CP210x  !!!
            if (CP210X_VID == dev->known->wVID) {
            //  выключить устройство CP210x
                //  не выключать 2016.12.20
                //  подозрение что ресурсы usb-стека затыкаются :((  дерьмо ^_^
                //  2016.12.23  ресурсы закрываю, теперь как-будто работает
dbg (_WARN, "CP210X_UART_DISABLE\n");
            }
        }
        dev->iFlags &= ~ BUSY_BIT1;
        dev->iFlags &= ~ UART_BIT2;
        ClosePipes (dev);
        if (dev->context) {
            coreKfree (dev->context);
            dev->context = NULL;
        }
        dev->lDescriptor = 0;
        iResult = (int) dev;    //  OK :)
        //dbg (_WARN, "   -OK\n");
    } else
        dbg (_ERR, "   -ERROR\n");
    //dbg (_WARN, "CloseDevice (%Xh)= %Xh\n", lDesc, iResult);
    return iResult;
}

static int __stdcall ProcCmd_SEND_PACKET (ioctl_t *request) {
    int iResult = 0;    //  set ERROR
    DWORD lDesc = request->inp_size;
    _DeviceUsbUart *dev = SeekToDescriptor (lDesc);
    if (dev && (dev->iFlags & BUSY_BIT1) && (dev->iFlags & PRESENCE_BIT0) && (dev->iFlags&UART_BIT2)) {
        _KosPipe *endp = GetCreatePipe (dev, dev->known->bOutPoint, BULK_TYPE_ENDPOINT, USBMAX_PACKET_SIZE_64, 0);
        if (request->out_size <= sizeof(dev->SenderStruct.bmData)-1) {
            if (dev->SenderStruct.wBytes == 0) {
            // OK buffer :)
                _RecSender *sender = & dev->SenderStruct;
                DWORD lSendBytes = sender->wBytes = request->out_size;
                memcpy (sender->bmData, request->output, sender->wBytes);
                if (sender->wBytes > USBMAX_PACKET_SIZE_64)
                    lSendBytes = USBMAX_PACKET_SIZE_64;
                sender->wActualeBytes = 0;
                dev->bEventsUART &= ~ EVENT_SEND_IS_FREE;   //  sender is busy
                usb_bulk_msg (dev, usb_sndbulkpipe(dev, dev->known->bOutPoint), sender->bmData, lSendBytes, NO_WAIT, Callback_BulkOut);
                iResult = -1;   //  set OK
            } else {
                dbg (_ERR, "-no free buffer for issue ! :(\n");
dbg (_WARN, "  SenderStruct.wbytes= %u %Xh\n", dev->SenderStruct.wBytes, dev->SenderStruct.bmData[0]);
            }
        } else
            dbg (_ERR, "-big data ! :(\n");
    }
    return iResult;
}

static void Lcp210x__usb_control_msg (_DeviceUsbUart *dev, BYTE request,  BYTE requesttype, WORD value, WORD index, void *data, WORD size) {
//  2017.02.16
//  примочка ^_^ для CP210x
//  состояние линий! и соответственно управление - ведётся  по нулевой точке, видимо происходит столкновения-гонки в CleverMouse-стеке
//  здесь, в поток опроса состояния линий вклиниваю управляющий запрос ^_^
    _Lcp210x_context * context = dev->context;
    if (context && context->iStatus==0) {
        context->bRequest = request;
        context->bmRequestType = requesttype;
        context->wValue = value;
        context->wIndex = index;
        context->buffer = data;
        context->wLength = size;
        //  требование о прогоне спец.запроса
        context->iStatus = 1;
        while (context->iStatus) {
            ;// ждём-с      sleep не доступен :(
        }
        //  готово
    }  else
        dbg (_ERR, "   LCP210x contest is null :((\n");
}

static int __stdcall ProcCmd_SET_STATUS_LINE (ioctl_t *request) {
    int iResult = 0;    //  set ERROR
    DWORD lDesc = request->inp_size;
    _DeviceUsbUart *dev = SeekToDescriptor (lDesc);
    if (dev && (dev->iFlags & BUSY_BIT1) && (dev->iFlags & PRESENCE_BIT0) && (dev->iFlags&UART_BIT2)) {
        //  "(WORD) ((DWORD)" иначе gcc выдаёт "warning" на уменьшение значения (4 байта => 2 байта)   2015.02.12
        WORD wValue = (WORD) ((DWORD) request->output);
        if (dev->known->bIntrPoint==0) {
            if (CP210X_VID == dev->known->wVID)
            //  CP210x  !!!
                //usb_control_msg (dev,  usb_sndctrlpipe(dev,0), CP210X_SET_MHS, CP210X_TRANS_TO_DEVICE
                //    , wValue | CP210X_CONTROL_WRITE_RTS | CP210X_CONTROL_WRITE_DTR,
                //    dev->known->bInterface, NULL, 0, INFINITE, Callback_Ctrl);
                //  2017.02.16
                Lcp210x__usb_control_msg (dev, CP210X_SET_MHS, CP210X_TRANS_TO_DEVICE
                    , wValue | CP210X_CONTROL_WRITE_RTS | CP210X_CONTROL_WRITE_DTR, dev->known->bInterface, NULL, 0
                );
            else {
            //  FTDI
                usb_control_msg (dev,  usb_sndctrlpipe(dev,0), FTDI_SIO_MODEM_CTRL, FTDI_SIO_SET_DATA_REQUEST_TYPE
                    , wValue | FTDI_CONTROL_WRITE_RTS | FTDI_CONTROL_WRITE_DTR,
                    dev->known->bInterface, NULL, 0, INFINITE, Callback_Ctrl);
            }
        } else
            usb_control_msg (dev,  usb_sndctrlpipe(dev,0), CDC_ACM_SET_CONTROL_LINE, CDC_ACM_TRANS_TO_DEVICE
                , wValue, dev->known->bInterface, NULL, 0, INFINITE, Callback_Ctrl);
        iResult = -1;   //  set OK
    }
    return iResult;
}

static int __stdcall ProcCmd_SETUP_LINE (ioctl_t *request) {
    int iResult = 0;    //  set ERROR
    if (request->output && request->out_size == sizeof(_LineCoding)) {
    //  setup line  (speed and format)
        DWORD lDesc = request->inp_size;
        _DeviceUsbUart *dev = SeekToDescriptor (lDesc);
        if (dev && (dev->iFlags & BUSY_BIT1) && (dev->iFlags & PRESENCE_BIT0) && (dev->iFlags&UART_BIT2)) {
            _LineCoding *coding = & dev->LineCoding; // 2015.03.27  OK :))
            memcpy (coding, request->output, sizeof (_LineCoding));
dbg (_WARN, "setup line  (speed and format)\n");
dbg (_WARN, "   sizeof= %u\n", sizeof(*coding));
dbg (_WARN, "   speed = %u\n", coding->dDteRate);
dbg (_WARN, "   format= %Xh\n", coding->bCharFormat);
dbg (_WARN, "   parity= %Xh\n", coding->bParityType);
dbg (_WARN, "   data  = %u\n", coding->bDataBits);
            iResult = -1; // set OK
            if (dev->known->bIntrPoint==0) {    //  CP210x !!! FTDI !!!
                if (CP210X_VID == dev->known->wVID) {
                    WORD wCodes;
dbg (_WARN, "MODE  CP210x\n");

                    //  set       DataBits  Parity  Stops
                    //      bits:   ba98     7654   3210
                    wCodes = coding->bDataBits & 0x0F;
                    wCodes <<= 4;
                    wCodes |= coding->bParityType & 0x0F;
                    wCodes <<= 4;
                    wCodes |= coding->bCharFormat & 0x0F;
dbg (_WARN, "   set coding = %02.2Xh\n", wCodes);
        //  2017.02.13  итио ^_^    linux-3-18\cp210x.c     proc.cp210x_set_config
                    //usb_control_msg (dev,  usb_sndctrlpipe(dev,0), CP210X_SET_LINE_CTL, CP210X_TRANS_TO_DEVICE
                    //    , wCodes,dev->known->bInterface, NULL, 0, INFINITE, Callback_Ctrl);
                    Lcp210x__usb_control_msg (dev, CP210X_SET_LINE_CTL, CP210X_TRANS_TO_DEVICE, wCodes, dev->known->bInterface, NULL, 0);

                    //  set speed
dbg (_WARN, "   set speed\n");
                    //usb_control_msg (dev, usb_sndctrlpipe(dev,0), CP210X_SET_BAUDRATE, CP210X_TRANS_TO_DEVICE
                    //    , 0,dev->known->bInterface, &(coding->dDteRate), 4, INFINITE, Callback_Ctrl);
                    Lcp210x__usb_control_msg (dev, CP210X_SET_BAUDRATE, CP210X_TRANS_TO_DEVICE, 0, dev->known->bInterface, &(coding->dDteRate), sizeof(coding->dDteRate));

dbg (_WARN, "<< MODE  CP210x\n");
                } else if (FTDI_VID == dev->known->wVID) {
                    DWORD wCodes;
dbg (_WARN, "MODE  FTDI\n");
                    //  set speed
                    {
                    DWORD dCodes =
                        ftdi_232bm_baud_to_divisor (coding->dDteRate);
dbg (_WARN, "   set code speed %u ==> %Xh\n", coding->dDteRate, dCodes);
                    usb_control_msg (dev, usb_sndctrlpipe(dev,0)
                        ,FTDI_SIO_SET_BAUDRATE_REQUEST
                        ,FTDI_SIO_SET_DATA_REQUEST_TYPE
                        ,dCodes, dCodes >> 16
                        ,NULL, 0, INFINITE, Callback_Ctrl);
                    }
                    //  set         Stop  Parity   Data_Bits
                    //      bits:    CB    A98     7654 3210
                    wCodes  = coding->bCharFormat & 3;
                    wCodes <<= 3;
                    wCodes |= coding->bParityType & 7;
                    wCodes <<= 8;
                    wCodes |= coding->bDataBits;
                    usb_control_msg (dev, usb_sndctrlpipe(dev,0),
                        FTDI_SIO_SET_DATA_REQUEST,
                        FTDI_SIO_SET_DATA_REQUEST_TYPE,
                        wCodes, dev->known->bInterface,
                        NULL, 0, INFINITE, Callback_Ctrl);
                }
            } else {

				if (0x1a86 == dev->known->wVID) {
dbg (_WARN, "MODE  CH34x\n");
					int DivCH340 = MY_ch341_get_divisor (coding->dDteRate);
    				usb_control_msg (dev,  usb_sndctrlpipe(dev,0), CH341_REQ_WRITE_REG, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_OUT
					, CH341_REG_DIVISOR << 8 | CH341_REG_PRESCALER, DivCH340, NULL, 0, INFINITE, Callback_Ctrl);

					#define LCR_STD		CH341_LCR_ENABLE_RX | CH341_LCR_ENABLE_TX | CH341_LCR_CS8
    				usb_control_msg (dev,  usb_sndctrlpipe(dev,0), CH341_REQ_WRITE_REG, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_OUT
					, CH341_REG_LCR2 << 8 | CH341_REG_LCR, LCR_STD, NULL, 0, INFINITE, Callback_Ctrl);

				} else {
dbg (_WARN, "MODE  CDC/ACM\n");
                	usb_control_msg (dev,  usb_sndctrlpipe(dev,0), CDC_ACM_SET_LINE_CODING, CDC_ACM_TRANS_TO_DEVICE
                    	, 0,dev->known->bInterface, coding, sizeof(_LineCoding), INFINITE, Callback_Ctrl);
				}
            }
        } else
dbg (_WARN, "err DEV Desc=%Xh !\n", lDesc);
    } else
dbg (_WARN, "err OUT !\n");
    return iResult;
}

static _DeviceUsbUart * SetBusy (ioctl_t *request) {
    _DeviceUsbUart *resDev = NULL; //  set ERROR
    if (rootDevice) {
        _DeviceUsbUart *dev = rootDevice;
        int boolCmpOK;
        // //////////////////////////////////////////////////////////////////////////////////////
        //  find device of named    ...
        //  goto first
        for (;dev->last;dev = dev->last);
        //  goto ended
        boolCmpOK = 0;
        while (1) {
            if (dev->devnum == (int)request->input) {
                boolCmpOK = 1;
                break;
            } else if (dev->next)
                dev = dev->next;
            else
                break;
        }
        // //////////////////////////////////////////////////////////////////////////////////////
        if (boolCmpOK) {
            if (dev->iFlags & PRESENCE_BIT0) {
                if ((dev->iFlags & BUSY_BIT1) == 0) {

                    dev->iFlags |= BUSY_BIT1;
                    dev->lDescriptor = (DWORD) dev + coreGetPid();
                    //  !!  only not zero !!!
                    resDev = dev;
                } else
                    dbg (_ERR, "device \"%u\" BUSY :(\n", request->input);
            } else
                dbg (_ERR, "device \"%u\" NO PRESENCE :(\n", request->input);
        }
    }   //  if (rootDevice) ...
    return resDev;
}

//--
static int __stdcall ProcCmd_OPEN_UART (ioctl_t *request) {
    _DeviceUsbUart *dev = SetBusy (request);
dbg (_WARN, "openUART ...\n");
    if (dev) {
        dev->iFlags |= UART_BIT2;
                        if (dev->known->bIntrPoint==0) {
dbg (_WARN, "dev->known->bIntrPoint = 0\n");
                            if (CP210X_VID == dev->known->wVID) {
                            //  CP210x  !!!
                                //  включить устройство CP210x  а-то оно до сих пор недовключено :))
dbg (_WARN, "enable CP210x\n");
                                usb_control_msg (dev,  usb_sndctrlpipe(dev, 0),
                                    CP210X_IFC_ENABLE, CP210X_TRANS_TO_DEVICE,
                                    CP210X_UART_ENABLE,
                                    dev->known->bInterface, NULL, 0, INFINITE, Callback_Ctrl
                                );
                                dev->context = coreKmalloc (sizeof(_Lcp210x_context));
                                if (dev->context)
                                    memset (dev->context,0,sizeof(_Lcp210x_context));
                                //  попытка использовать BULK_IN поток для опроса состояния линии -
                                //  провалилась: usbstack вызывает BULK_callback ТОЛЬКО при сбое или поступлении данных,
                                //  а это -  затык  :(((    вот такой подвох :)     2015.03.11
//dbg (_WARN, "start reader of StatusLine\n");
                                //usb_bulk_msg (dev, usb_rcvbulkpipe(dev,0),
                                //    &(dev->bmStatusLine), 1,//sizeof(dev->bmStatusLine),
                                //    NO_WAIT, Callback_Intr);
//  2017.02.14  всё также ^_^ столкновения с CTRL-запросами
//  2017.02.15
//            usb_control_msg_CP (dev,  usb_rcvbulkpipe(dev,0),
//                CP210X_REQUEST_GET_MDMSTS, CP210X_REQTYPE_TO_HOST,
//                0,dev->known->bInterface, &(dev->bmStatusLine), 1, NO_WAIT, Callback_Intr
//            );
                                //  start  reading of status line
                                usb_control_msg (dev,  usb_rcvbulkpipe(dev,0),
                                    CP210X_REQUEST_GET_MDMSTS, CP210X_REQTYPE_TO_HOST,
                                    0,dev->known->bInterface, &(dev->bmStatusLine), 1, NO_WAIT, Callback_Intr
                                );
                            } else {
                                //BYTE bTimer;
                                //  magic actions  :)   шайтан-дери  :))
                                //  похоже влияет на частоту оповещения, при отсутствии данных, о состоянии линии и модема
                                //      (первые два байта во входном пакете)     2015.03.04  "ftdi_sio.h" linux-3-18
                                //      ( эти два байта поступают даже при отсутствии входных данных !!! )
                                //  важно: там-же описан формат записи - то лишь для "мультипортовок" !! 2015.04.15
                                // usb_control_msg (dev, INDEX_EP_CTRL, 0x0A, 0xC0, 0, 0,  &bTimer,1, INFINITE);
                                // usb_control_msg (dev, INDEX_EP_CTRL, 9,    0x40, 1, 0,  NULL,0,    INFINITE);
dbg (_WARN, "reset FTDI\n");
                                usb_control_msg (dev, usb_sndctrlpipe(dev,0),
                                    FTDI_SIO_RESET_REQUEST, FTDI_SIO_SET_DATA_REQUEST_TYPE,
                                    FTDI_SIO_RESET_SIO, dev->known->bInterface, NULL, 0, INFINITE, Callback_Ctrl
                                );
                            }
                        } else {
dbg (_WARN, "dev->known->bIntrPoint != 0\n");
                            //_KosPipe *endp =
                            //& dev->pipes [INDEX_EP_INTR];
                            //if (dev->DeviceDescriptor.bcdDevice == 0x0300) {
                            // bcdDevice - там НУЛИ %|  2015.04.10
                            //  видимо дескриптор запрашивается не весь, а надо :)
                            //  потому - вот костыль:  bcdUSB ТОЛЬКО pl2303_широкий :((
                            //if (dev->DeviceDescriptor.bcdUSB == 0x0200) {
                            // ...  :)
                            // запрос дескриптора устройства    2015.04.11
                            usb_control_msg (dev, usb_rcvctrlpipe(dev,0),
                                CDC_ACM_TRANS_TO_HOST, 0x80,
                                1, 0,
                                &(dev->DeviceDescriptor), sizeof(dev->DeviceDescriptor), INFINITE, Callback_Ctrl
                            );
                            if (dev->DeviceDescriptor.bcdDevice == BCD_PL2303_MAGIC_QUIRK) {
                                // "старый" pl2303 после quirk-запроса НЕ РАБОТАЕТ :((  2015.04.11
dbg (_WARN, "magic PL-2303  :|\n");
                                //  source in   kernel linux-3-18   pl2303.c    func. "pl2303_startup"  2015.04.09
                                //  "впусти меня" :)
                                //  или
                                //  танец с бубном :|   по чусовой в сторону заката или прыжками против чусовой в сторону восхода :|
                                //#define VENDOR_READ_REQUEST         1
                                //#define VENDOR_READ_REQUEST_TYPE    0xc0
                                #define VENDOR_WRITE_REQUEST        1
                                #define VENDOR_WRITE_REQUEST_TYPE   0x40
                                //#define pl2303_vendor_read(Value)
                                //    usb_control_msg (dev, INDEX_EP_CTRL, VENDOR_READ_REQUEST, VENDOR_READ_REQUEST_TYPE, Value, 0, &(dev->bmIncomingQueue), 1, INFINITE);
                                #define pl2303_vendor_write(Value,Index) \
                                    usb_control_msg (dev, usb_sndctrlpipe(dev,0), VENDOR_WRITE_REQUEST, VENDOR_WRITE_REQUEST_TYPE, Value, Index, NULL, 0, INFINITE, Callback_Ctrl);
                                //pl2303_vendor_read  (0x8484);
                                //pl2303_vendor_write (0x0404, 0);
                                //pl2303_vendor_read  (0x8484);
                                //pl2303_vendor_read  (0x8383);
                                //pl2303_vendor_read  (0x8484);
                                //pl2303_vendor_write (0x0404, 1);
                                //pl2303_vendor_read  (0x8484);
                                //pl2303_vendor_read  (0x8383);
                                //pl2303_vendor_write (0, 1);
                                //pl2303_vendor_write (1, 0);
                                // if (....)    //  ... после какого-то "шайтан-дери" с полем  bDeviceClass
                                //   pl2303_vendor_write (2, 0x24);     <- пробовал, не работает 2015.04.12
                                //  else
                                // этого quirk-а достаточно  :| 2015.04.11
                                pl2303_vendor_write (2, 0x44);
                            }
                            dbg (_WARN, "start reader of StatusLine  std.CDC/ACM\n");
                            //  pipe    - known of characteristic destination ENDPOINT !!!    ;)
                            usb_bulk_msg (dev, usb_rcvbulkpipe(dev,dev->known->bIntrPoint), dev->bmStatusLine, sizeof(dev->bmStatusLine), NO_WAIT, Callback_Intr);
                        }
                        //  starting receiver
                        dbg (_WARN, "START RECEIVERING !\n");
                        coreMutexInit (&dev->MutexIQ);
                        usb_bulk_msg (dev, usb_rcvbulkpipe(dev,dev->known->bInpPoint), dev->bmReceiver, sizeof(dev->bmReceiver), NO_WAIT, Callback_BulkInp);
    }
    if (dev) {
        dbg (_WARN, "OpenDevice  OK\n");//, dev->lDescriptor);
        return dev->lDescriptor;
    } else {
        dbg (_WARN, "ERROR  ProcCmd_OPEN_UART\n");
        return 0; // set ERROR
    }
}

//--
static int __stdcall ProcCmd_CTRL_RAW DeF (ioctl_t *request) {
//          sample_request_ioctl   (_ControlRequest *, uint32 Handle, void *bmData, int timeout_HW__endpoint_LW)
//                                       input           inp_size           output          out_size
    int iResult = 0;    //  set ERROR
    DWORD lDesc = request->inp_size;
    _DeviceUsbUart *dev = SeekToDescriptor (lDesc);

    //dbg (_WARN, "ProcCmd_CTRL_RAW ...\n");
    if (dev
    && (dev->iFlags & BUSY_BIT1)
    && (dev->iFlags & PRESENCE_BIT0)
    && (dev->iFlags & UART_BIT2)==0
    ) {
        _ControlRequest *ctrl_msg = request->input;
        if (ctrl_msg->wLength)
            memcpy (dev->bmReceiver, request->output, ctrl_msg->wLength);
        //dbg (_WARN,"   usb_control_msg (%i reqtype=%Xh, req=%Xh, value=%Xh,\n"
        //    , usb_sndctrlpipe(dev,request->out_size && 0xFFFF)
        //    , ctrl_msg->bmRequestType, ctrl_msg->bRequest, ctrl_msg->wValue);
        //dbg (_WARN,"    index=%Xh, bytes=%u)\n"
        //    , ctrl_msg->wIndex, ctrl_msg->wLength);
        iResult = usb_control_msg (dev,
// 2017.06.22   направление передачи указывается в bmRequestType
            usb_sndctrlpipe(dev,request->out_size && 0xFFFF),
            ctrl_msg->bRequest,
            ctrl_msg->bmRequestType,
            ctrl_msg->wValue,
            ctrl_msg->wIndex,
            dev->bmReceiver,
            ctrl_msg->wLength,
            INFINITE,   //  request->out_size >> 16
            Callback_Ctrl
        );
        if (iResult) {
            _KosPipe *endp = (_KosPipe *)iResult;
            iResult = endp->iCountRW;
            if (ctrl_msg->wLength)
                memcpy (request->output, dev->bmReceiver, ctrl_msg->wLength);
        }
        //dbg (_WARN,"    result= %i\n", iResult);
    }
    //dbg (_WARN, "... ProcCmd_CTRL_RAW\n");
    return iResult;
}

static void __stdcall Callback_Bulk_Msg DeF (void* pipe, int status, void* buffer_not_use, int length, void* calldata) {
    _KosPipe *endp = calldata;

    if (status)
        dbg (_WARN, "Callback_Bulk_Msg\n");

    endp->iSimpleCounter ++;
    endp->iCountRW = length;

    if (endp->pEvent)
        coreRaiseEvent ((void*)endp->pEvent, endp->IdEvent, 0, NULL);
}

static int __stdcall ProcCmd_GET_CONF_DESC DeF (ioctl_t *request) {
//          sample_request_ioctl   (not_use, int Handle, void *buf, int size)
//                                   input    inp_size    output     out_size
    int iResult = 0;    //  set ERROR
    DWORD lDesc = (DWORD) request->inp_size;
    _DeviceUsbUart *dev = SeekToDescriptor (lDesc);
    dbg (_WARN, "coreUSBGetParam  ...\n");
    if (dev
//    && (dev->iFlags & BUSY_BIT1)  откуда тогда lDesc  :)  2017.06.23
    && (dev->iFlags & PRESENCE_BIT0)
    ) {
        _usb_config_desc_std  *config = coreUSBGetParam (dev->pipe0, GET_DESCCONF);
        if (config) {
            iResult = MIN(request->out_size , config->wTotalLength);
            memcpy (request->output, config, iResult);
        } else  //  2017.07.19
            dbg (_WARN, "coreUSBGetParam  ERROR !\n");
    }
    return iResult;
}

static int __stdcall ProcCmd_BULK_RAW DeF (ioctl_t *request) {
//          sample_request_ioctl   (void *bmData, int iSizeData, uint32 Handle, int timeout_HW__endpoint_LW)
//                                       input        inp_size      output            out_size
    int iResult = 0;    //  set ERROR
    DWORD lDesc = (DWORD) request->output;
    _DeviceUsbUart *dev = SeekToDescriptor (lDesc);
    if (dev
    && (dev->iFlags & BUSY_BIT1)
    && (dev->iFlags & PRESENCE_BIT0)
    && (dev->iFlags & UART_BIT2)==0
    ) {
        if ((request->out_size & USB_DIR_IN) ==0)
            memcpy (dev->bmReceiver, request->input, request->inp_size);
        iResult = usb_bulk_msg (dev,
            request->out_size & 0xFFFF,
            dev->bmReceiver,
            request->inp_size,
            INFINITE,   //  request->out_size >> 16
            Callback_Bulk_Msg
        );
        if (request->out_size & USB_DIR_IN)
            memcpy (request->input, dev->bmReceiver, request->inp_size);
        if (iResult) {
            _KosPipe *endp = (_KosPipe *) iResult;
            iResult = endp->iCountRW;
        }
    }
    return iResult;
}

static int __stdcall ProcCmd_OPEN_RAW (ioctl_t *request) {
    _DeviceUsbUart *dev = SetBusy (request);
    dbg (_WARN, "openDEVICE_raw ...\n");
    if (dev) {
// //////////////   2017.06.22    ////////////////////////////////////////////////////////////////////
/* such there is in  ftdi.ftdi_usb_open_dev().ftdi_usb_reset()  2017.06.23
static int I = 0;
if (FTDI_VID == dev->known->wVID && I>0) {
    //  первый раз пропускаем  ^_^
    dbg (_WARN, "   reset FTDI\n");
    usb_control_msg (dev, usb_sndctrlpipe(dev,0),
        FTDI_SIO_RESET_REQUEST, FTDI_SIO_SET_DATA_REQUEST_TYPE,
        FTDI_SIO_RESET_SIO, dev->known->bInterface, NULL, 0, INFINITE, Callback_Ctrl);
}
I++;*/
// ///////////////////////////////////////////////////////////////////////////////////////////////////
        //dbg (_WARN, "   -OK result= %Xh\n", dev->lDescriptor);
        dev->iFlags &= ~ UART_BIT2;
        return dev->lDescriptor;
    } else {
        dbg (_ERR, "   -ERROR\n");
        return 0;
    }
}

static _UsbUart_Procs Procs[] = {
    {USBUART_CMD_OPEN_UART              , ProcCmd_OPEN_UART},
    {USBUART_CMD_SETUP_LINE             , ProcCmd_SETUP_LINE},
    {USBUART_CMD_SET_STATUS_LINE        , ProcCmd_SET_STATUS_LINE},
    {USBUART_CMD_GET_BYTE               , ProcCmd_GET_BYTE},
    {USBUART_CMD_GET_PACKET             , ProcCmd_GET_BYTES},
    {USBUART_CMD_GET_MAX_PACKET_LENGTH  , ProcCmd_GET_MAX_PACKET_LENGTH},
    {USBUART_CMD_SEND_PACKET            , ProcCmd_SEND_PACKET},
    {USBUART_CMD_CLOSE_DEVICE           , ProcCmd_CLOSE_DEVICE},
    {USBUART_CMD_GET_STATUS_LINE        , ProcCmd_GET_STATUS_LINE},
    {USBUART_CMD_EVENT_STATUS           , ProcCmd_EVENT_STATUS},

    {USBUART_CMD_GET_CONF_DESC          , ProcCmd_GET_CONF_DESC},
    {USBUART_CMD_OPEN_RAW               , ProcCmd_OPEN_RAW},
    {USBUART_CMD_CTRL_RAW               , ProcCmd_CTRL_RAW},
    {USBUART_CMD_BULK_RAW               , ProcCmd_BULK_RAW},
};

//--
static int __stdcall ServiceProc DeF (ioctl_t *request) {
    int iResult = 0;    //  set ERROR
    if (request==NULL)  //  paranoia?! :))
        return iResult;

    //    dbg ("ServiceProc (handle= %X, io_code= %X)\n", request->handle, request->io_code);
    if ( request->io_code==USBUART_CMD_FIRST_DEVICE_USBUART && request->output && (unsigned)request->out_size >= sizeof(_Request_FindUsbUart) ) {
        if (rootDevice) {
            _Request_FindUsbUart *find = (_Request_FindUsbUart *) request->output;
            _DeviceUsbUart *dev = rootDevice;
            //  goto first
            for (;dev->last;dev = dev->last);
            find->iDevNum = dev->devnum;
            strcpy (find->szDescription, dev->known->szDescription);
            memcpy (&(find->DeviceDescriptor), &(dev->DeviceDescriptor), sizeof(find->DeviceDescriptor));
            find->iFlags = dev->iFlags;
            find->device = dev;
            iResult = -1;   //  set OK
        }
    } else if ( request->io_code==USBUART_CMD_NEXT_DEVICE_USBUART && request->output && (unsigned)request->out_size >= sizeof(_Request_FindUsbUart) ) {
        if (rootDevice) {
            _Request_FindUsbUart *find = (_Request_FindUsbUart *) request->output;
            if (find && find->device) {
                _DeviceUsbUart *dev = SeekUartDevice (find->device);
                if (dev && dev->next) {
                    dev = dev->next;
                    find->iDevNum = dev->devnum;
                    strcpy (find->szDescription, dev->known->szDescription);
                    memcpy (&(find->DeviceDescriptor), &(dev->DeviceDescriptor), sizeof(find->DeviceDescriptor));
                    find->iFlags = dev->iFlags;
                    find->device = dev;
                    iResult = -1;   //  set OK
                }
            }
        }
    } else if ( request->io_code==USBUART_CMD_GET_EVENT_CHANGE_LIST) {
        iResult = booChangeList;
        if (booChangeList)
            booChangeList = FALSE;
    } else if ( request->io_code==tst_GET_DEVICE_STRUC && request->out_size == sizeof(_DeviceUsbUart)) {
        DWORD lDesc = request->inp_size;
        _DeviceUsbUart *dev = SeekToDescriptor (lDesc);
        if (dev) {
            memcpy ((void *) request->output, dev, sizeof(_DeviceUsbUart));
            iResult = (int) dev;    //  OK :)
        } else
            dbg (_ERR, "-no device with  Desc= %u !\n", lDesc);
    } else {
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        int i;                                              //////////////////////////////////////////////////////////////////
        for (i=0;i<sizeof(Procs)/sizeof(Procs[0]);i++) {    //////////////////////////////////////////////////////////////////
            if (Procs[i].proc && Procs[i].iCmd==request->io_code) {
                iResult = Procs[i].proc (request);          //////////////////////////////////////////////////////////////////
                break;                                      //////////////////////////////////////////////////////////////////
            }                                               //////////////////////////////////////////////////////////////////
        }                                                   //////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }
    return iResult;
}

//--
static void *  __stdcall Additive DeF (void* pipe0, void* configdescr, void* interfacedescr) {
    _DeviceUsbUart *newdev = NULL;  //  result ERROR !
    _usb_interface_desc_std *intrf = (_usb_interface_desc_std *) interfacedescr;
    _usb_device_desc_std *descDev;

    dbg (_WARN, "AddDevice  Intf=%X   Class=%X SubClass=%X Protocol=%X\n"
        , intrf->bInterfaceNumber
        , intrf->bInterfaceClass, intrf->bInterfaceSubClass, intrf->bInterfaceProtocol);
    descDev = coreUSBGetParam (pipe0, GET_DESCDEV);
    if (descDev) {
        int iKnown1, i;
        dbg (_WARN, "this device with VID:PID  %04.4X:%04.4X  bcd=%X\n", descDev->wVID, descDev->wPID, descDev->bcdDevice);

    {
    _usb_config_desc_std  *config = coreUSBGetParam (pipe0, GET_DESCCONF);
        if (config) {
            dbg (_ERR, "  SizeConf=  %u  std=%u\n", config->wTotalLength, sizeof(*config));
        } else
            dbg (_ERR, "-error Config is NULL :(\n");
    }
        // сверяемся со списком ЗНАКОМЫX НАМ! устройств
        iKnown1 = 0;
        for (i=0;(unsigned)i<sizeof(Known_Device) / sizeof(Known_Device[0]);i++) {
            if (descDev->wVID==knownDevice[i].wVID
            && descDev->wPID==knownDevice[i].wPID
            && intrf->bInterfaceNumber==knownDevice[i].bInterface) {
                iKnown1 = i+1;
                break;
            }
        }
        if (iKnown1) {
        // устройство опознано  - его индекс+1 в  iKnown1
        //  создаём/добавляем ячейку
            newdev = coreKmalloc (sizeof(_DeviceUsbUart));
            if (newdev) {
                _KnownDevice *known = & knownDevice [iKnown1 -1];
                memset (newdev, 0, sizeof(*newdev));
                memcpy (&(newdev->DeviceDescriptor), descDev, sizeof(newdev->DeviceDescriptor));
                //  creating resources of read/write/get_status/settings
                newdev->known = known;
                newdev->pipe0 = pipe0;
                {
                    //  OK  :)  additive to list
                    int Index0device = 0;/////////  for named "usbuart%u"  ////////////////////
//dbg ("sizeof(_DeviceUsbUart)= %u\n", sizeof(_DeviceUsbUart));
                    if (rootDevice) {
                        //  settings of original value for Index0device  (naming of device)
                        _DeviceUsbUart *dev = rootDevice;
                        while (dev) {
                            //  goto first
                            for (;dev->last;dev = dev->last);
                            //  goto ended
                            for (;dev;dev = dev->next) {
                                if (dev->devnum == Index0device) {
                                    Index0device++;
                                    //  bad, new searching for !
                                    break;
                                }
                            }
                        }
                        //  goto ended
                        for (;rootDevice->next;rootDevice = rootDevice->next);
                        rootDevice->next = newdev;
                    }//////////////////////////////////////////////////////////////////
                    //  uartDevice   this ended device !
                    newdev->last = rootDevice;
                    newdev->next = NULL;
                    newdev->devnum = Index0device;
                    newdev->iFlags = PRESENCE_BIT0;
                    newdev->bEventsUART |= EVENT_SEND_IS_FREE;  //  sender is free
dbg (_WARN, "... also:   new device= %i\n", newdev->devnum);
                    rootDevice = newdev;
                    booChangeList = TRUE; //  declare about change to list of devices
                //} else {
                //    coreKfree (newdev);
                //    newdev = NULL; // set ERROR :((
                //    dbg (_ERR, "-ERROR  no openning resources of endpoints !\n");
                }
            } else
                dbg (_ERR, "-error, no memory\n");
        } else
            dbg (_ERR, "-this not known device\n");
    } else
        dbg (_ERR, "-error of read descriptor device :((\n");

    return (void*)newdev;    //  NULL == rejection, error :((
}

//--
static void * __stdcall Disconnected DeF (void* devicedata) {
    _DeviceUsbUart *dev = (_DeviceUsbUart *) devicedata;
    dbg (_WARN, "-disconnect device  \"%i\"\n", dev->devnum);
    //  сбросить ТОЛЬКО флаг активности
    dev->iFlags &= ~ PRESENCE_BIT0;  //  предполагается FFFF.FFFEh
    //  если устройство ещё используется ( BUSY_BIT1 e.t.c)- пусть оно болтается, там видно будет
    if (0== dev->iFlags) {
        _DeviceUsbUart *last = dev->last;
        _DeviceUsbUart *next = dev->next;
        // delete of list
        if (last) {
            last->next = next;
            rootDevice = last;
        }
        if (next) {
            next->last = last;
            rootDevice = next;
        }
        if (next==NULL && last==NULL)
            rootDevice = NULL;   //  ups,  Hasta la vista baby
        coreKfree (dev);
        //dbg ("... OK, closed events resources   :)\n");
    } else
        dbg (_ERR, "   -busy :(\n");
    booChangeList = TRUE; //  declare about change to list of devices
    return NULL;
}

static _USBFUNC UsbFunc = {sizeof(UsbFunc), (DWORD)Additive, (DWORD)Disconnected};

//--
int __stdcall Public_Start DeF (int iReason, char *cmdline) {
    int iResult = 0;    //  -ERROR

    if (iReason==DRV_ENTRY) {
//    #ifdef __GNUC__
//asm (
//    ".intel_syntax noprefix\n"
//    "int 3\n"
//    ".att_syntax\n" //  верните обратно AT&T    :))
//);
//    #endif

// кажется всё 2017.02.27 dbg (_WARN, "TEST%10s:  %Xh  %4.4Xh  %04.4Xh\n", "_test", 0x123, 0x123, 0x123);
        dbg (_WARN, "... cmdline= \"%s\" ...\n", cmdline);
        if (cmdline && !memcmp(cmdline,"debug=",6)) {   //  2015.05.05
            setlevel_dbg (atol (&cmdline[6]));
        }

        rootDevice = NULL;
        iResult = (int) coreRegUSBDriver ("usbother", ServiceProc, &UsbFunc);
        if (iResult) {
           dbg (_WARN, "-OK  iResult= %Xh\n", iResult);
        } else
            dbg (_ERR, "-ERROR :(\n");
    } else {
        //  goto first
        for (;rootDevice && rootDevice->last;rootDevice = rootDevice->last);
        // сшибаем ВСЁ !!!
        while (rootDevice) {
            _DeviceUsbUart *next = (_DeviceUsbUart *) rootDevice->next;
            memset (rootDevice,0,sizeof(*rootDevice));
            coreKfree (rootDevice);
            rootDevice = next;
        }
    }
    return iResult;
}
