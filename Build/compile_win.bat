@echo off

set BUILD_DIR=build_GrowTrace
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

cmake ../..
cmake --build .

echo.
pause