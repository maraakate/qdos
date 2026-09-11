@echo off
SET SEVENZIPPATH=E:\Program Files\7-zip

cd /D %QDOSDEVBASE%\

del /Q QDOS_SSE_LATEST.7Z
rd /s /q release

cd /D %QDOSDEVBASE%\QUAKE
del /Q qdos.exe
del /Q qdos.dbg
del /Q qdosfx.exe
del /Q qdosfx.dbg

make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile USE_SSE=1 -j4
REM FS: DO NOT COMPRESS THE BINARIES - CAUSES SUBTLE PROBLEMS!
REM UPX -9 %QDOSDEVBASE%\QUAKE\QDOS.EXE

objcopy --only-keep-debug qdos.exe qdos.dbg
objcopy --strip-debug --strip-unneeded qdos.exe

make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile USE_SSE=1 GLQUAKE=1 -j4
REM FS: DO NOT COMPRESS THE BINARIES - CAUSES SUBTLE PROBLEMS!
REM UPX -9 %QDOSDEVBASE%\QUAKE\QDOSFX.EXE

objcopy --only-keep-debug qdosfx.exe qdosfx.dbg
objcopy --strip-debug --strip-unneeded qdosfx.exe

cd /D %QDOSDEVBASE%\QW
del /Q qw.exe
del /Q qw.dbg
del /Q qwdosfx.exe
del /Q qwdosfx.dbg

make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile USE_SSE=1 -j4
REM FS: DO NOT COMPRESS THE BINARIES - CAUSES SUBTLE PROBLEMS!
REM UPX -9 E:\PROJ\QDOS\QW\QW.EXE

objcopy --only-keep-debug qw.exe qw.dbg
objcopy --strip-debug --strip-unneeded qw.exe

make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile USE_SSE=1 GLQUAKE=1 -j4
REM FS: DO NOT COMPRESS THE BINARIES - CAUSES SUBTLE PROBLEMS!
REM UPX -9 E:\PROJ\QDOS\QW\QWDOSFX.EXE

objcopy --only-keep-debug qwdosfx.exe qwdosfx.dbg
objcopy --strip-debug --strip-unneeded qwdosfx.exe

del /q "%QDOSDEVBASE%\QDOS_SSE_LATEST.7Z"

cd..
md release
echo SSE is for Pentium 3 or later processors ONLY.  It takes advantage of additional CPU extensions that can speed up graphics processing.  These SSE builds are entirely UNSUPPORTED and provided only as a convenience!  Please do not send us bug reports from them, Thanks! > release\!READ_SSE.txt
copy cwsdpmi.exe release
copy %QDOSDEVBASE%\QUAKE\QDOS.EXE release
copy %QDOSDEVBASE%\QUAKE\QDOS.DBG release
copy %QDOSDEVBASE%\QUAKE\QDOSFX.EXE release
copy %QDOSDEVBASE%\QUAKE\QDOSFX.DBG release
copy %QDOSDEVBASE%\QW\QW.EXE release
copy %QDOSDEVBASE%\QW\QW.DBG release
copy %QDOSDEVBASE%\QW\QWDOSFX.EXE release
copy %QDOSDEVBASE%\QW\QWDOSFX.DBG release

"%SEVENZIPPATH%\7z.exe" a -mx9 -mmt "%QDOSDEVBASE%\QDOS_SSE_LATEST.7Z" "%QDOSDEVBASE%\RELEASE\*"

rd /s /q %QDOSDEVBASE%\RELEASE

cd /D %QDOSDEVBASE%\
