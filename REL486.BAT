@echo off
SET SEVENZIPPATH=E:\Program Files\7-zip

cd /D %QDOSDEVBASE%\Quake
del /q qdos.exe
del /q qdosfx.exe

make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile CPU=i486 -j4
REM FS: DO NOT COMPRESS THE BINARIES - CAUSES SUBTLE PROBLEMS!
REM UPX -9 %QDOSDEVBASE%\QUAKE\QDOS.EXE

cd /D %QDOSDEVBASE%\QW
del /q qw.exe
del /q qwdosfx.exe

make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile CPU=i486 -j4
REM FS: DO NOT COMPRESS THE BINARIES - CAUSES SUBTLE PROBLEMS!
REM UPX -9 E:\PROJ\QDOS\QW\QW.EXE

del /q "%QDOSDEVBASE%\QDOS_486_LATEST.7Z"

"%SEVENZIPPATH%\7z.exe" a -mx9 -mmt "%QDOSDEVBASE%\QDOS_486_LATEST.7Z" %QDOSDEVBASE%\QUAKE\QDOS.EXE
"%SEVENZIPPATH%\7z.exe" a -mx9 -mmt "%QDOSDEVBASE%\QDOS_486_LATEST.7Z" %QDOSDEVBASE%\QW\QW.EXE

cd /D %QDOSDEVBASE%\
