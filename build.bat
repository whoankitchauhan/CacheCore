@echo off
:: build.bat — convenience build script for CacheCore on Windows
:: Requires:  CMake (installed at C:\Program Files\CMake\bin)
::            w64devkit (extracted at %USERPROFILE%\Desktop\w64devkit)
::
:: Usage:
::   build.bat          -> Release build
::   build.bat debug    -> Debug build
::   build.bat clean    -> Remove build directory
::   build.bat test     -> Build + run tests
::   build.bat bench    -> Build + run benchmark

setlocal

set "CMAKE=C:\Program Files\CMake\bin\cmake.exe"
set "W64=%USERPROFILE%\Desktop\w64devkit\bin"
set "PATH=%W64%;%PATH%"
set "BUILD_TYPE=Release"
set "BUILD_DIR=build"

if "%1"=="clean" (
    echo Removing build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    echo Done.
    goto :EOF
)

if "%1"=="debug" set "BUILD_TYPE=Debug"

:: Configure if build dir doesn't exist
if not exist "%BUILD_DIR%\Makefile" (
    echo Configuring CMake [%BUILD_TYPE%]...
    "%CMAKE%" -S . -B "%BUILD_DIR%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
    if errorlevel 1 ( echo [FAIL] CMake configure failed & exit /b 1 )
)

:: Build
echo Building...
"%CMAKE%" --build "%BUILD_DIR%" --config %BUILD_TYPE%
if errorlevel 1 ( echo [FAIL] Build failed & exit /b 1 )
echo [OK] Build succeeded.

if "%1"=="test" (
    echo.
    echo Running tests...
    "%BUILD_DIR%\cachecore_tests.exe"
    goto :EOF
)

if "%1"=="bench" (
    echo.
    echo Running benchmark...
    "%BUILD_DIR%\cachecore_bench.exe"
    goto :EOF
)

echo.
echo Targets built:
echo   build\cachecore_cli.exe    - CLI (SET/GET/DELETE/STATS/EXIT)
echo   build\cachecore_tests.exe  - Test suite
echo   build\cachecore_bench.exe  - Benchmark
echo.
echo Run:  build.bat test
echo Run:  build.bat bench
echo Run:  build\cachecore_cli.exe [capacity]
