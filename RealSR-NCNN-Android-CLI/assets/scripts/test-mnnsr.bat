@echo off

:: Test script for mnnsr-ncnn.exe
:: This script calls test-all.bat to test mnnsr-ncnn.exe

:: Call the main test script with mnnsr-ncnn.exe as parameter
call "%~dp0test-all.bat" mnnsr-ncnn.exe

pause
