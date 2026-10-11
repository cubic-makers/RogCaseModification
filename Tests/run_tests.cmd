@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist Build\tests mkdir Build\tests
rem DMA addresses are 32-bit on STM32; the host mock does not dereference those addresses.
cl /nologo /W3 /wd4311 /std:c11 /FI"mock_hal.h" /I"Tests/mock" /I"Core/Inc" Tests/test_modules.c /Fo"Build/tests/test_modules.obj" /Fe"Build/tests/test_modules.exe"
if errorlevel 1 exit /b 1
Build\tests\test_modules.exe
exit /b %errorlevel%
