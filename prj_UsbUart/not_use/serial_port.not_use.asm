
;format COFF                                      ; FASM
;section '.text' code ;readable executable        ; FASM

;.386                                              ; WASM / JWASM / ML(masm) / TASM32v5.4
;mycomdebug SEGMENT para PUBLIC use32 'CODE'       ; WASM / JWASM / ML(masm) / TASM32v5.4
;    assume cs:mycomdebug    ;   "tasm" writing "Near jump or call to different CS"    ^_^

public _ser_outp_b@8
;public _ser_simple_outp_b@8

;   void __stdcall ser_outp_b (int port, char Char)
_ser_outp_b@8:
    push ebp
    mov ebp, esp

    push ax
    push dx
    push ecx
    pushf

    ;////////////////////////////////////////
    ;//  ожидание доступности передачи
    ;   148  - 2014.06.13 :(    медленный com-порт :|
    mov ecx, 148 ; если порт НЕ пуст! - первый байт болтается в передатчике (зацикливая) до подключения напарника
    mov dx, [ebp+8 +0]
    add dx, 5
    Q:
        dec ecx
        jcxz Q_out  ;   :(
            in al,dx
            and al, 11100000b
            ;//      \ тайм-аут (устройство не связано с компьютером)
    jz Q
    Q_out:
    ;////////////////////////////////////////
    mov dx, [ebp+8 +0]
    mov al, [ebp+8 +4]
    out dx, al

    popf
    pop ecx
    pop dx
    pop ax

    pop ebp
    ret 4*2

;   void __stdcall ser_simple_outp_b (int port, char Char)
;_ser_simple_outp_b@8:
    push ebp
    mov ebp, esp

    push ax
    push dx

    mov dx, [ebp+8 +0]
    mov al, [ebp+8 +4]
    out dx, al

    pop dx
    pop ax

    pop ebp
    ret 4*2

;mycomdebug ends                                         ; WASM / JWASM / ML(masm)
;    end                                                 ; WASM / JWASM / ML(masm)
