@echo off
setlocal enabledelayedexpansion

REM Build ScorpionSatin Engine

if not exist "build" mkdir build
cd build

echo [*] Cleaning previous build...
if exist CMakeCache.txt del CMakeCache.txt
if exist CMakeFiles rmdir /s /q CMakeFiles

echo [*] Configuring CMake...
cmake .. -G "Visual Studio 17 2022" -A x64

if errorlevel 1 (
    echo [!] CMake configuration failed!
    pause
    exit /b 1
)

echo [*] Building Release...
cmake --build . --config Release

if errorlevel 1 (
    echo [!] Build failed!
    pause
    exit /b 1
)

echo [*] Build complete!
cd ..
pause
