
@echo off

  rem set path=D:\gcc-4.8-win32\bin;%path%
  rem set path=D:\mingw32\bin;%path%
set path=C:\Qt\Qt5.4.0\Tools\mingw491_32\bin;%path%
  rem 2015.01.14  работают все три варианта, в "gcc-4.8-win32" bin-арник меньше :)

if "%1"=="" mingw32-make.exe  -f GNUmakefile  -B
%1 %2 %3 %4 %5 %6 %7 %8 %9
