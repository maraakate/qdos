@echo off
SET SEVENZIPPATH=E:\Program Files\7-zip

cd /D %QDOSDEVBASE%\

del /Q QWDOS_EXE_LATEST.7Z
rd /s /q release

cd /D %QDOSDEVBASE%\QW
make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile -j4
REM FS: DO NOT COMPRESS THE BINARIES - CAUSES SUBTLE PROBLEMS!
REM UPX -9 %QDOSDEVBASE%\QW\QW.EXE

make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile GLQUAKE=1 -j4
REM FS: DO NOT COMPRESS THE BINARIES - CAUSES SUBTLE PROBLEMS!
REM UPX -9 %QDOSDEVBASE%\QW\QWDOSFX.EXE

cd..
md release
copy cwsdpmi.exe release
copy readme.fx release
copy %QDOSDEVBASE%\QW\qw.exe release
copy %QDOSDEVBASE%\QW\qwdosfx.exe release
copy %QDOSDEVBASE%\QW\readme.qwd release
xcopy 3rdparty\lib_dxe release /E

"%SEVENZIPPATH%\7z.exe" a -mx9 -mmt "%QDOSDEVBASE%\QWDOS_EXE_LATEST.7Z" "%QDOSDEVBASE%\RELEASE\*"

rd /s /q %QDOSDEVBASE%\RELEASE

cd /D cd /D %QDOSDEVBASE%\
