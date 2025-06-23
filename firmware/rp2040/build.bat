@echo off
REM Check if the "build" folder exists; if not, create it.
if not exist "build" (
    mkdir build
)

REM Change to the build folder.
cd build

echo Running cmake...
cmake -G "NMake Makefiles" ..

echo Running nmake...
nmake

rem #### VERBOSE OUTPUT
@echo off
rem setlocal

rem echo Deleting old build directory for a clean start...
rem if exist "build" (
rem     rmdir /s /q build
rem )
rem mkdir build

rem REM Change to the build folder.
rem cd build

rem echo Running cmake to configure with NMake...
rem cmake -G "NMake Makefiles" ..

rem REM Check if cmake configuration succeeded
rem if %errorlevel% neq 0 (
rem     echo CMake configuration failed. Halting script.
rem     goto :eof
rem )

rem echo Running nmake with verbose output...
rem REM We use "cmake --build ." which calls nmake for us.
rem REM The --verbose flag will show every single command being run by nmake.
rem cmake --build . --verbose

rem endlocal