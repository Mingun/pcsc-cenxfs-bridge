@echo off
set "XFS_SDK=..\XFS SDK3.0\SDK"

:clear
del /F *.obj *.dll *.lib *.exp

:build
cl /LD /EHsc /FePCSCspi ^
	-D_WIN32_WINNT=0x0501 -DWIN32_LEAN_AND_MEAN -DDEBUG ^
	/I"%XFS_SDK%\INCLUDE" ^
	/I"%BOOST_ROOT%" ^
	*.cpp ^
	/link ^
	/LIBPATH:"%XFS_SDK%\LIB" ^
	/LIBPATH:"%BOOST_ROOT%/stage/lib" ^
	/MACHINE:X86