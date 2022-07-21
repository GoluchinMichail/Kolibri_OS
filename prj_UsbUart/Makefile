
#
#   Open Watcom C/C++ Version 1.9
#

.before
!   ifdef __LINUX__
del= rm
copy= cp
!   else
#   of shell OSDOS/WINDOWS
del= del
copy= copy /Y
!   endif
KOS=..
COMPILE_FLAGS=-q -we -zp1 -za99 -zev -mf -zld -s -zl -bt=raw -fo=$@  -i=..

usbuart.sys:    usbuart.obj ch340_div.obj ftdi.obj    ddkos.obj dbg.obj serial_a.obj
    @echo File { > files.
    @for %i in ($<) do echo %i >> files.
    @echo } >> files.
# 2017.08.02  without  rdtsc/serail   @echo Lib ../clib_simple.wat.lib >> files.
#       2014.12.12  в 3 раза ширше и понадобились "main_" и  "_debug_out@4"
#       2014.11.28  CRT от RDOS ну его, мало-ли :)
    @echo Lib linux/clib3r.lib >> files.
!ifdef __LINUX__
    @echo Option Start=\'_Public_Start@8\' >> files.
    wlink Sys rdos  Name $@ Option Quiet @files. @../core_import.wat.def
!else
    @echo Option Start='_Public_Start@8' >> files.
    wlink Sys rdos  Name $@ Option Map,Quiet @files. @..\core_import.wat.def
!endif
    $(del) files.
    @echo -OK
!ifdef __LINUX__
    $(copy) $@ ../../!run/uart
!else
    $(copy) $@ ..\..\!run\uart
!endif
	rm *.obj

#   попытка использовать "core_import.def" как GNU/MSVC
#   lib386.exe /nologo /machine:ix86 /def:core.import.def /out:core.import.msvc.lib
#   содержимое lib-файла НЕТО :(( 2015.01.20
#core_import.wat.lib:   core_import.wat.def
#    lib.exe /nologo /machione:IX86 /def:$< /out:$@
#    del $*.exp


dbg.obj : $(KOS)/lib__kernel_api/dbg.c
    wcc386  $(COMPILE_FLAGS)                $<

ddkos.obj : $(KOS)/lib__kernel_api/ddkos.NASM.asm
    nasm -I$(KOS)/lib__kernel_api -f coff  -o $@    $<

.c.obj:
!ifdef __LINUX__
    @echo -i=$(WATCOM)/h\;.. > prm.
!else
    @echo -i=%WATCOM%\h;.. > prm.
!endif
    wcc386  $(COMPILE_FLAGS) @prm. $<
    @$(del) prm.

#   -i=drivers\include;drivers\include\linux
#   -zld          remove file dependency information
#   -s            remove stack overflow checks  ("undefined symbol __CHK")
#   -zl           remove default library information
##   -zz           remove "@size" from __stdcall function names (10.0 compatible)

serial_a.obj : $(KOS)/lib__misc/serial_a.NASM.asm
    nasm -I$(KOS)/lib__misc  -f coff  -o $@  $<
