@echo off & setlocal

set VS_DIR=c:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\
set TSVN_DIR=c:\Program Files\TortoiseSVN\bin\
set OUT_DIR=%~dp0

if /i "%1" == "x86" set TSVN_FILES=libapr_tsvn32 libaprutil_tsvn32 libsvn_tsvn32
if /i "%1" == "x64" set TSVN_FILES=libapr_tsvn libaprutil_tsvn libsvn_tsvn
if /i "%TSVN_FILES%" == "" goto eof

call "%VS_DIR%vcvarsall.bat" %1

for %%x in (%TSVN_FILES%) do (
  dumpbin.exe /exports "%TSVN_DIR%%%x.dll" > "%OUT_DIR%%%x.def"
)

:eof
endlocal
