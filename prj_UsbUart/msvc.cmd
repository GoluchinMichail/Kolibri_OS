@echo off

    rem pushd "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build"
    rem     call vcvars32.bat
    rem popd

set Root=%UserProfile%\Root
if not "%Shell%"=="" set Root=d:

rem Target\цель всегда x86  32-bits
set Host=x64

		rem	основной маршрут к компилятору и компановщику
set Path=%Root%\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.12.25827\bin\Host%Host%\x86;%PATH%
		rem маршрут к шибко статическим  api*.dll
set Path=%Root%\Program Files (x86)\Windows Kits\10\Redist\ucrt\DLLs\%Host%;%Path%
	rem	затем! вспомогательный маршрут для nmake.exe   %/
set Path=%PATH%;%Root%\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.12.25827\bin\Host%Host%\%Host%
	rem
set INCLUDE=%Root%\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.12.25827\include;%INCLUDE%
set INCLUDE=%Root%\Program Files (x86)\Windows Kits\10\include\10.0.16299.0\ucrt;%INCLUDE%
	rem set LIB=%Root%\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.12.25827\lib\x86;%LIB%

%*