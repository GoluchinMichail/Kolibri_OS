
// ////////////    FTDI     USB Single Port Serial Converter    /////////////////////////////////////////////////////

    #define FTDI_SIO_SET_DATA_REQUEST_TYPE  0x40

    #define FTDI_SIO_RESET_REQUEST          0       // Reset the port
    #define FTDI_SIO_RESET_SIO              0

    #define FTDI_SIO_MODEM_CTRL             1
    #define FTDI_CONTROL_WRITE_DTR          0x0100  //  change of line status DTR
                                                    //      (0 bit  this status)
    #define FTDI_CONTROL_WRITE_RTS          0x0200  //  change of line status RTS
                                                    //      (1 bit  this status)

    #define FTDI_SIO_SET_BAUDRATE_REQUEST   3

    #define FTDI_SIO_SET_DATA_REQUEST       4
