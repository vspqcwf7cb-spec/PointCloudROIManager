@echo off
setlocal EnableExtensions

REM ============================================================
REM  3DJianCeTuJiao - local build script (Windows / MSVC)
REM
REM  Dependencies: PCL 1.13.1 (system) + 3rdparty/json.hpp
REM  (no vcpkg / no OpenCV required)
REM
REM  Usage:
REM     build_operator.bat            -> x64 Release build
REM     build_operator.bat Debug       -> x64 Debug build
REM ============================================================

set "ROOT=%~dp0"
set "BUILD_DIR=%ROOT%build_sbt"
set "INSTALL_DIR=%ROOT%install"
set "BUILD_TYPE=%~1"
if "%BUILD_TYPE%"=="" set "BUILD_TYPE=Release"

echo.
echo [INFO] Root       : %ROOT%
echo [INFO] Build dir  : %BUILD_DIR%
echo [INFO] Build type : %BUILD_TYPE%

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo.
echo [STEP 1/2] cmake configure ...
pushd "%BUILD_DIR%"
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 (
    popd
    echo.
    echo [ERROR] cmake configure failed. See messages above.
    goto :fail
)

echo.
echo [STEP 2/2] cmake build ...
cmake --build . --config %BUILD_TYPE% -- /m
if errorlevel 1 (
    popd
    echo.
    echo [ERROR] build failed.
    goto :fail
)
popd

echo.
echo [INFO] Done. Artifacts in:
echo          %INSTALL_DIR%\sbt_cv_ops.dll
echo          %INSTALL_DIR%\pointCloudROIManagerDemo.exe
echo          %INSTALL_DIR%\test_roi_manager.exe
echo.
echo [INFO] Run demo (from install dir):
echo          cd /d %INSTALL_DIR%
echo          pointCloudROIManagerDemo.exe
echo.
echo [OK] Build succeeded.
endlocal
pause
exit /b 0

:fail
endlocal
echo.
echo [FAIL] Build failed. Check messages above, then close this window.
pause
exit /b 1
