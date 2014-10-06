@echo off
set "XFS_SDK=D:\Projects\banks\XFS SDK3.0\SDK"
cl /LD /EHsc /I"%XFS_SDK%\INCLUDE" PCSCspi.cpp /link /LIBPATH:"%XFS_SDK%\LIB" /MACHINE:X86