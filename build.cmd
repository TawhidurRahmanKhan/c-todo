@echo off
setlocal
pushd "%~dp0" || exit /b 1

where cl >nul 2>&1
if not errorlevel 1 goto compile

set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%vswhere%" (
    for /f "usebackq delims=" %%I in (`"%vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find **/vcvars64.bat`) do set "vcvars=%%I"
)
if not defined vcvars (
    echo Microsoft C Build Tools could not be found.
    popd
    exit /b 1
)
call "%vcvars%" || (popd & exit /b 1)

:compile
cl /nologo /std:c17 /W4 /WX /D_CRT_SECURE_NO_WARNINGS main.c /Fe:todo.exe
set "build_result=%errorlevel%"
if not "%build_result%"=="0" goto done
cl /nologo /std:c17 /W4 /WX /D_CRT_SECURE_NO_WARNINGS web.c /Fe:todo-web.exe
set "build_result=%errorlevel%"

:done
popd
exit /b %build_result%
