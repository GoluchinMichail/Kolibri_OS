
#   win:    gcc.win.cmd  mingw32-make.exe  -f GNUmakefile  -B
#
#   Host Linux: i686-w64-mingw32-gcc-4.8.0-linux64_rubenvb.tar.xz
#   Host Win:   mingw491_32 (within Qt 5.4.0)    2017.02.16

ifdef OS
	# host Windows PREFIX=""
else
	PREFIX=i686-w64-mingw32-
endif

CC= $(PREFIX)gcc
DLLTOOL= $(PREFIX)dlltool
LD= $(PREFIX)ld
STRIP= $(PREFIX)strip
LINK_COMMAND= $(LD) -s -o $@ @ld_cmd_lst  $^
C_COMPILE_FLAG=-I..

SOURCES_OBJ_FILES_KOS=ddkos.obj dbg.obj serial_a.obj EAX_EBX_ECX_EDX_ret_EAX.obj
SOURCES_OBJ_FILES_CLIB=atol.obj prtf.obj memcpy.obj memset.obj memcmp.obj strlen.obj strcpy.obj
SOURCES_OBJ_FILES=$(SOURCES_OBJ_FILES_KOS) $(SOURCES_OBJ_FILES_CLIB)
KOS_PRJ=..

#  ../menuetlibc/lib/libc.a
#       нету strcpy, memcpy,snprintf
#       проблема с префиксами "_"
#       кроме того компиляторы-то разные ;)
#   изуверы :((  ^-^    важна последовательность перечисления !!            2015.01.13
usbuart.mingw.sys:   usbuart.o  ftdi.o   $(SOURCES_OBJ_FILES) core_import.gnu.a
	@echo   --entry=_Public_Start@8                 >  ld_cmd_lst
#   @echo   --enable-stdcall-fixup      >>  ld_cmd.lst
#               Link _sym to _sym@nn without warnings   (for "_start")
##  @echo   --print-output-format       >>  ld_cmd.lst
##          curiosities for the sake of     :)
#
#   OVERHEAD! :
#   target ->  exe-file with Base Relocation Directory [part .reloc]    2014.11.26  ^_^
#           creating file  "usbuart_gnu.sys.base"
	$(LINK_COMMAND)     --base-file $@.base
#           files "core_import.gnu.a" and "usbuart_gnu.sys.base" is created
#   
#           creating file  "usbuart_gnu.sys.reloc"
	$(DLLTOOL) --base-file $@.base --output-exp $@.reloc
#           file "usbuart_gnu.sys.base" anymore needs
	@$(RM) $@.base
#   
#           creating binary (obj * libs * core_import.gnu.a * usbuart_gnu.sys.reloc)
	$(LINK_COMMAND) $@.reloc   
#           deleting temporary files,   ufff  :)
	@$(RM) $@.reloc
	@$(RM) core_import.gnu.a
	@$(RM) ld_cmd_lst
	echo
	cp $@ ../../!run/$@
	rm *.obj
	@echo -OK

.c.o:
	$(CC) -I.. -c  $<
# оптимизация (-Ox) если и влияет, то не значительно    2015.01.28

core_import.gnu.a: ../core_import.def
	$(DLLTOOL)  --input-def $<  --output-lib $@
# --no-delete	temporary file

ddkos.obj : ../lib__kernel_api/ddkos.NASM.asm
	nasm -I$(<D) -f coff  -o $@    $<

dbg.obj : ../lib__kernel_api/dbg.c
	$(CC) -c $(C_COMPILE_FLAG)  -o $@  $<

serial_a.obj : ../lib__misc/serial_a.NASM.asm
	nasm -I$(<D) -f coff  -o $@    $<

EAX_EBX_ECX_EDX_ret_EAX.obj : ../lib__kos_api/EAX_EBX_ECX_EDX_ret_EAX.asm
	nasm -I$(<D) -f coff  -o $@    $<

atol.obj : $(KOS_PRJ)/lib__clib_simple/atol.c
	$(CC) -c $(C_COMPILE_FLAG)  -o $@  $<

memcpy.obj : $(KOS_PRJ)/lib__clib_simple/memcpy.c
	$(CC) -c $(C_COMPILE_FLAG)  -o $@  $<

prtf.obj : $(KOS_PRJ)/lib__clib_simple/prtf.c
	$(CC) -c $(C_COMPILE_FLAG)  -o $@  $<

memcmp.obj : $(KOS_PRJ)/lib__clib_simple/memcmp.c
	$(CC) -c $(C_COMPILE_FLAG)  -o $@  $<

memset.obj : $(KOS_PRJ)/lib__clib_simple/memset.c
	$(CC) -c $(C_COMPILE_FLAG)  -o $@  $<

strlen.obj : $(KOS_PRJ)/lib__clib_simple/strlen.c
	$(CC) -c $(C_COMPILE_FLAG)  -o $@  $<

strcat.obj : $(KOS_PRJ)/lib__clib_simple/strcat.c
	$(CC) -c $(C_COMPILE_FLAG)  -o $@  $<

strstr.obj : $(KOS_PRJ)/lib__clib_simple/strstr.c
	$(CC) -c $(C_COMPILE_FLAG)  -o $@  $<

strcpy.obj : $(KOS_PRJ)/lib__clib_simple/strcpy.c
	$(CC) -c $(C_COMPILE_FLAG)  -o $@  $<
