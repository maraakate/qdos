@echo off
SET SEVENZIPPATH=E:\Program Files\7-zip

cd /D %QDOSDEVBASE%\

del /Q QDOS_486_LATEST.7Z
rd /s /q release

cd /D %QDOSDEVBASE%\QUAKE
del /Q qdos.exe
del /Q qdos.dbg

make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile CPU=i486 -j4
REM FS: DO NOT COMPRESS THE BINARIES - CAUSES SUBTLE PROBLEMS!
REM UPX -9 %QDOSDEVBASE%\QUAKE\QDOS.EXE

objcopy --only-keep-debug qdos.exe qdos.dbg
objcopy --strip-debug --strip-unneeded qdos.exe

cd /D %QDOSDEVBASE%\QW
del /Q qw.exe
del /Q qw.dbg

make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile CPU=i486 -j4
REM FS: DO NOT COMPRESS THE BINARIES - CAUSES SUBTLE PROBLEMS!
REM UPX -9 E:\PROJ\QDOS\QW\QW.EXE

objcopy --only-keep-debug qw.exe qw.dbg
objcopy --strip-debug --strip-unneeded qw.exe

del /q "%QDOSDEVBASE%\QDOS_486_LATEST.7Z"

cd..
md release
echo These i486 binaries are EXPERIMENTAL!  You can submit a bug report, but they may not get fixed! > release\!READ_486.txt
copy cwsdpmi.exe release
copy %QDOSDEVBASE%\QUAKE\QDOS.EXE release
copy %QDOSDEVBASE%\QUAKE\QDOS.DBG release
copy %QDOSDEVBASE%\QW\QW.EXE release
copy %QDOSDEVBASE%\QW\QW.DBG release
copy %QDOSDEVBASE%\QUAKE\README.QD release
copy %QDOSDEVBASE%\QW\README.QWD release
copy %QDOSDEVBASE%\3rdparty\lib_dxe\sndpci.dxe release

"%SEVENZIPPATH%\7z.exe" a -mx9 -mmt "%QDOSDEVBASE%\QDOS_486_LATEST.7Z" "%QDOSDEVBASE%\RELEASE\*"

rd /s /q %QDOSDEVBASE%\RELEASE

cd /D %QDOSDEVBASE%\
