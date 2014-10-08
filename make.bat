@echo off
set "XFS_SDK=..\XFS SDK3.0\SDK"
cl /LD /EHsc /I"%XFS_SDK%\INCLUDE" PCSCspi.cpp /link /LIBPATH:"%XFS_SDK%\LIB" /MACHINE:X86