@echo off
echo [BUILD] Initializing MSVC Environment...
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if %errorlevel% neq 0 (
    echo [ERROR] Failed to initialize MSVC environment.
    exit /b %errorlevel%
)

echo [BUILD] Cleaning previous build outputs...
if exist dist rmdir /s /q dist
if exist mb_resource_checker.zip del /f /q mb_resource_checker.zip
if exist loader.res del /f /q loader.res
if exist "Motherboard Resource Checker.exe" del /f /q "Motherboard Resource Checker.exe"

echo [BUILD] Running QMake...
"C:\Qt\5.15.2\msvc2019_64\bin\qmake.exe" mb_resource_checker.pro
if %errorlevel% neq 0 (
    echo [ERROR] QMake failed.
    exit /b %errorlevel%
)

echo [BUILD] Running NMake...
nmake clean
nmake release
if %errorlevel% neq 0 (
    echo [ERROR] NMake build failed.
    exit /b %errorlevel%
)

echo [BUILD] Preparing clean distribution directory...
mkdir dist
copy release\mb_resource_checker.exe dist\
if %errorlevel% neq 0 (
    echo [ERROR] Failed to copy binary to dist folder.
    exit /b %errorlevel%
)

echo [BUILD] Deploying dependencies with windeployqt...
call "C:\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe" dist\mb_resource_checker.exe
if %errorlevel% neq 0 (
    echo [ERROR] windeployqt failed.
    exit /b %errorlevel%
)

echo [BUILD] Packaging dist folder into ZIP...
powershell -Command "Compress-Archive -Path dist\* -DestinationPath mb_resource_checker.zip -Force"
if %errorlevel% neq 0 (
    echo [ERROR] ZIP packaging failed.
    exit /b %errorlevel%
)

echo [BUILD] Compiling loader resource script...
rc.exe /fo loader.res src\loader.rc
if %errorlevel% neq 0 (
    echo [ERROR] rc.exe failed.
    exit /b %errorlevel%
)

echo [BUILD] Compiling final portable single-executable...
cl.exe /O2 /MT /EHsc src\loader.cpp src\miniz.c loader.res /Fe"Motherboard Resource Checker.exe" /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib
if %errorlevel% neq 0 (
    echo [ERROR] cl.exe loader build failed.
    exit /b %errorlevel%
)

echo [BUILD] Cleaning up temporary files...
if exist dist rmdir /s /q dist
if exist loader.res del /f /q loader.res

echo [BUILD] Build completed successfully! Final binary is: "Motherboard Resource Checker.exe"
