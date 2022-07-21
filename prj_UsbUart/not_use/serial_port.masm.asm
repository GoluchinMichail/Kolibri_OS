
.386                                              ; WASM / JWASM / ML(masm) / TASM32v5.4
mycomdebug SEGMENT para PUBLIC use32 'CODE'       ; WASM / JWASM / ML(masm) / TASM32v5.4
    assume cs:mycomdebug    ;   tasm32 "Near jump or call to different CS"      ^_^

include serial_port.asm

mycomdebug ends                                   ; WASM / JWASM / ML(masm)
    end                                           ; WASM / JWASM / ML(masm)
