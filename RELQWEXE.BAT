@echo off
SET SEVENZIPPATH=E:\Program Files\7-zip

cd /D %QDOSDEVBASE%\

del /Q QWDOS_EXE_LATEST.7Z
rd /s /q release

cd /D %QDOSDEVBASE%\QW
del /Q qw.exe
del /Q qw.dbg
del /Q qwdosfx.exe
del /Q qwdosfx.dbg

make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile -j4
REM FS: DO NOT COMPRESS THE BINARIES - CAUSES SUBTLE PROBLEMS!
REM UPX -9 %QDOSDEVBASE%\QW\QW.EXE

objcopy --only-keep-debug qw.exe qw.dbg
objcopy --strip-debug --strip-unneeded qw.exe

make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile GLQUAKE=1 -j4
REM FS: DO NOT COMPRESS THE BINARIES - CAUSES SUBTLE PROBLEMS!
REM UPX -9 %QDOSDEVBASE%\QW\QWDOSFX.EXE

objcopy --only-keep-debug qwdosfx.exe qwdosfx.dbg
objcopy --strip-debug --strip-unneeded qwdosfx.exe

cd..
md release
copy cwsdpmi.exe release
copy readme.fx release
copy %QDOSDEVBASE%\QW\QW.EXE release
copy %QDOSDEVBASE%\QW\QW.DBG release
copy %QDOSDEVBASE%\QW\QWDOSFX.EXE release
copy %QDOSDEVBASE%\QW\QWDOSFX.DBG release
copy %QDOSDEVBASE%\QW\README.QWD release
xcopy 3rdparty\lib_dxe release /E

"%SEVENZIPPATH%\7z.exe" a -mx9 -mmt "%QDOSDEVBASE%\QWDOS_EXE_LATEST.7Z" "%QDOSDEVBASE%\RELEASE\*"

rd /s /q %QDOSDEVBASE%\RELEASE

cd /D %QDOSDEVBASE%\
