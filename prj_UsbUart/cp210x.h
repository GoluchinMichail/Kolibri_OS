
// ///////////  устройства  CP210x (Silicon Labs Inc)  //////////////////////////////////////////////////////////////

#define CP210X_UART_ENABLE              1
#define CP210X_UART_DISABLE             0
#define CP210X_IFC_ENABLE               0
#define CP210X_TRANS_TO_DEVICE          0x41
#define CP210X_SET_BAUDRATE             0x1E
#define CP210X_SET_LINE_CTL             3       //  record two byte of deskside of line
#define CP210X_SET_MHS                  7
#define CP210X_CONTROL_WRITE_DTR        0x0100  //  change of line status DTR
#define CP210X_CONTROL_WRITE_RTS        0x0200  //  change of line status RTS

#define CP210X_REQTYPE_TO_HOST          0xC1    //  analogue  CDC_ACM_TRANS_TO_HOST  a1h   :((
#define		REQTYPE_INTERFACE_TO_HOST	CP210X_REQTYPE_TO_HOST
#define CP210X_REQUEST_GET_MDMSTS       8
//  STATUS LINE     cp210x.c    linux-3-18  2015.03.10
//  bits: 7.  6.   5.  4.    3.2.1.0
//       CD  RI  DSR  CTS    ? ? ? ?

#define CP210X_GET_FLOW		0x14
#define CP210X_GET_BAUDRATE	0x1D
#define CP210X_GET_LINE_CTL	0x04
