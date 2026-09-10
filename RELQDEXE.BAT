@echo off
SET SEVENZIPPATH=E:\Program Files\7-zip

cd /D %QDOSDEVBASE%\

del /Q QDOS_EXE_LATEST.7Z
rd /s /q release

cd /D %QDOSDEVBASE%\QUAKE
make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile -j4
UPX -9 %QDOSDEVBASE%\QUAKE\QDOS.EXE
make -f makefile clean
make -f makefile GLQUAKE=1 clean
make -f makefile GLQUAKE=1 -j4
UPX -9 %QDOSDEVBASE%\QUAKE\QDOSFX.EXE

cd..
md release
copy cwsdpmi.exe release
copy readme.fx release
copy %QDOSDEVBASE%\quake\qdos.exe release
copy %QDOSDEVBASE%\quake\qdosfx.exe release
copy %QDOSDEVBASE%\quake\readme.qd release
xcopy 3rdparty\lib_dxe release /E

"%SEVENZIPPATH%\7z.exe" a -mx9 -mmt "%QDOSDEVBASE%\QDOS_EXE_LATEST.7Z" "%QDOSDEVBASE%\RELEASE\*"

rd /s /q %QDOSDEVBASE%\RELEASE

cd /D %QDOSDEVBASE%\
