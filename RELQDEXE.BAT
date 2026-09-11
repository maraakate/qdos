@echo off
SET SEVENZIPPATH=E:\Program Files\7-zip

cd /D %QDOSDEVBASE%\

del /Q QDOS_EXE_LATEST.7Z
rd /s /q release

cd /D %QDOSDEVBASE%\QUAKE
del /Q qdos.exe
del /Q qdos.dbg
del /Q qdosfx.exe
del /Q qdosfx.dbg

make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile -j4
REM FS: DO NOT COMPRESS THE BINARIES - CAUSES SUBTLE PROBLEMS!
REM UPX -9 %QDOSDEVBASE%\QUAKE\QDOS.EXE

objcopy --only-keep-debug qdos.exe qdos.dbg
objcopy --strip-debug --strip-unneeded qdos.exe

make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile GLQUAKE=1 -j4
REM FS: DO NOT COMPRESS THE BINARIES - CAUSES SUBTLE PROBLEMS!
REM UPX -9 %QDOSDEVBASE%\QUAKE\QDOSFX.EXE

objcopy --only-keep-debug qdosfx.exe qdosfx.dbg
objcopy --strip-debug --strip-unneeded qdosfx.exe

cd..
md release
copy cwsdpmi.exe release
copy readme.fx release
copy %QDOSDEVBASE%\QUAKE\QDOS.EXE release
copy %QDOSDEVBASE%\QUAKE\QDOS.DBG release
copy %QDOSDEVBASE%\QUAKE\QDOSFX.EXE release
copy %QDOSDEVBASE%\QUAKE\QDOSFX.DBG release
copy %QDOSDEVBASE%\QUAKE\README.QD release
xcopy 3rdparty\lib_dxe release /E

"%SEVENZIPPATH%\7z.exe" a -mx9 -mmt "%QDOSDEVBASE%\QDOS_EXE_LATEST.7Z" "%QDOSDEVBASE%\RELEASE\*"

rd /s /q %QDOSDEVBASE%\RELEASE

cd /D %QDOSDEVBASE%\
