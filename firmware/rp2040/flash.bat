rem @echo off
rem REM Always use COM28
rem set "COMPort=COM28"

@echo off
REM Prompt for COM port
set /p COMPort=Enter COM port (e.g. COM13):

REM Check if the input starts with "COM" (case-insensitive)
echo %COMPort% | findstr /b /i "COM" >nul
if errorlevel 1 (
    set COMPort=COM%COMPort%
)

REM Get the directory of this script
set "scriptDir=%~dp0"
REM Build directory and UF2 file path
set "buildDir=%scriptDir%build"
set "uf2File=%buildDir%\n64_dumper.uf2"

REM Path to uf2conv.py (adjust as needed)
set "uf2convPath=D:\Programs\Arduino\Arduino IDE Portable\portable\packages\rp2040\hardware\rp2040\3.2.1\tools\uf2conv.py"

echo Running: python "%uf2convPath%" --serial "%COMPort%" --family RP2040 --deploy "%uf2File%"
python "%uf2convPath%" --serial "%COMPort%" --family RP2040 --deploy "%uf2File%"

rem pause
