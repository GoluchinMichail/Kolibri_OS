
.386
_FTDI_ASM_ segment word public 'CODE'

public ftdi_set_baudrate, ConfPacket

TYPE_2232H=4

SIO_SET_BAUD_RATE =3  ;Set baud rate
SIO_SET_BAUDRATE_REQUEST     =SIO_SET_BAUD_RATE
LIBUSB_REQUEST_TYPE_VENDOR = (02 shl 5)
LIBUSB_RECIPIENT_DEVICE = 00
LIBUSB_ENDPOINT_OUT = 00
FTDI_DEVICE_OUT_REQTYPE = (LIBUSB_REQUEST_TYPE_VENDOR or LIBUSB_RECIPIENT_DEVICE or LIBUSB_ENDPOINT_OUT)

ftdi_context    struc
    chipType                db      ?
;    baudrate                dd      ?
;    bitbangEnabled          db      ?
;    readBufChunkSize        dd      ?
;    writeBufChunkSize       dd      ?
;    readBufPtr              dd      ?
;    writeBufPtr             dd      ?
;    readBufSize             dd      ?
;    writeBufSize            dd      ?
;    maxPacketSize           dd      ?
;    interface               dd      ?
    index                   dd      ?
;    inEP                    dd      ?
;    outEP                   dd      ?
;    nullP                   dd      ?
;    lockPID                 dd      ?
;    next_context            dd      ?
ftdi_context    ends

_H_CLK = 120000000
_C_CLK = 48000000

ConfPacket  db  10 dup(?)

  ftdi_set_baudrate:
        ;DEBUGF 2, 'K :ftdi: FTDI Set baudrate to %d PID: %d Dev handle: 0x%x\n',\
        ;           [edi+8], [edi], [edi+4]
        mov     ebx, [edi+4]
        cmp     [ebx + ftdi_context.chipType], TYPE_2232H
        jl      c_clk
        imul    eax, dword ptr [edi+8], 10
        cmp     eax, _H_CLK / 3FFFh
        jle     c_clk
  h_clk:
        cmp     dword ptr [edi+8], _H_CLK/10
        jl      h_nextbaud1
        xor     edx, edx
        mov     ecx, _H_CLK/10
        jmp     calcend

  c_clk:
        cmp     dword ptr [edi+8], _C_CLK/16
        jl      c_nextbaud1
        xor     edx, edx
        mov     ecx, _C_CLK/16
        jmp     calcend

  h_nextbaud1:
        cmp     dword ptr [edi+8], _H_CLK/(10 + 10/2)
        jl      h_nextbaud2
        mov     edx, 1
        mov     ecx, _H_CLK/(10 + 10/2)
        jmp     calcend

  c_nextbaud1:
        cmp     dword ptr [edi+8], _C_CLK/(16 + 16/2)
        jl      c_nextbaud2
        mov     edx, 1
        mov     ecx, _C_CLK/(16 + 16/2)
        jmp     calcend

  h_nextbaud2:
        cmp     dword ptr [edi+8], _H_CLK/(2*10)
        jl      h_nextbaud3
        mov     edx, 2
        mov     ecx, _H_CLK/(2*10)
        jmp     calcend

  c_nextbaud2:
        cmp     dword ptr [edi+8], _C_CLK/(2*16)
        jl      c_nextbaud3
        mov     edx, 2
        mov     ecx, _C_CLK/(2*16)
        jmp     calcend

  h_nextbaud3:
        mov     eax, _H_CLK*16/10  ; eax - best_divisor
        xor     edx, edx
        div     dword ptr [edi+8]      ; [edi+8] - baudrate
        push    eax
        and     eax, 1
        pop     eax
        shr     eax, 1
        jz      h_rounddowndiv     ; jump by result of and eax, 1
        inc     eax
  h_rounddowndiv:
        cmp     eax, 20000h
        jle     h_best_divok
        mov     eax, 1FFFFh
  h_best_divok:
        mov     ecx, eax
        mov     eax, _H_CLK*16/10
        xor     edx, edx
        div     ecx
        xchg    ecx, eax            ; ecx - best_baud
        push    ecx
        and     ecx, 1
        pop     ecx
        shr     ecx, 1
        jz      rounddownbaud
        inc     ecx
        jmp     rounddownbaud

  c_nextbaud3:
        mov     eax, _C_CLK        ; eax - best_divisor
        xor     edx, edx
        div     dword ptr [edi+8]      ; [edi+8] - baudrate
        push    eax
        and     eax, 1
        pop     eax
        shr     eax, 1
        jnz     c_rounddowndiv     ; jump by result of and eax, 1
        inc     eax
  c_rounddowndiv:
        cmp     eax, 20000h
        jle     c_best_divok
        mov     eax, 1FFFFh
  c_best_divok:
        mov     ecx, eax
        mov     eax, _C_CLK
        xor     edx, edx
        div     ecx
        xchg    ecx, eax            ; ecx - best_baud
        push    ecx
        and     ecx, 1
        pop     ecx
        shr     ecx, 1
        jnz     rounddownbaud
        inc     ecx

  rounddownbaud:
        mov     edx, eax            ; edx - encoded_divisor
        shr     edx, 3
        and     eax, 7
        ;push    7 6 5 1 4 2 3 0
        pushd 7
         pushd 6
          pushd 5
           pushd 1
            pushd 4
             pushd 2
              pushd 3
               pushd 0
        mov     eax, [esp+eax*4]
        shl     eax, 14
        or      edx, eax
        add     esp, 32

  calcend:
        mov     eax, edx        ; eax - *value
        mov     ecx, edx        ; ecx - *index
        and     eax, 0FFFFh
        cmp     [ebx + ftdi_context.chipType], TYPE_2232H
        jge     foxyindex
        shr     ecx, 16
        jmp     preparepacket
  foxyindex:
        shr     ecx, 8
        and     ecx, 0FF00h
        or      ecx, [ebx + ftdi_context.index]

  preparepacket:
        mov     word ptr [ConfPacket], (FTDI_DEVICE_OUT_REQTYPE) \
                                + (SIO_SET_BAUDRATE_REQUEST shl 8)
        mov     word ptr [ConfPacket+2], ax
        mov     word ptr [ConfPacket+4], cx
        mov     word ptr [ConfPacket+6], 0
;        jmp     own_index

_FTDI_ASM_ ends
end
