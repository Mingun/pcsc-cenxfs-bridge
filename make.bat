@echo off
set "XFS_SDK=..\XFS SDK3.0\SDK"

:clear
del /F *.obj *.dll *.lib *.exp

:build
cl /LD /EHsc /FePCSCspi ^
	/Wp64 ^
	-D_WIN32_WINNT=0x0501 -DDEBUG ^
	/I"%XFS_SDK%\INCLUDE" ^
	/I"%BOOST_ROOT%" ^
	*.cpp ^
	/link ^
	/LIBPATH:"%XFS_SDK%\LIB" ^
	/LIBPATH:"%BOOST_ROOT%/stage/lib" ^
	/MACHINE:X86