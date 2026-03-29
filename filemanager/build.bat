@echo off
REM AfterOS File Manager 编译脚本
REM 需要安装 MinGW-w64 工具链

echo Compiling AfterOS File Manager...

REM 使用 MinGW-w64 交叉编译到 Linux ELF
x86_64-w64-mingw32-g++ -static ^
    FileManager.cpp ^
    -o FileManager.elf ^
    -std=c++11 ^
    -O2 ^
    -Wall

if %errorlevel% == 0 (
    echo.
    echo ========================================
    echo Compilation successful!
    echo Output: FileManager.elf
    echo ========================================
    
    REM 显示文件信息
    dir FileManager.elf
) else (
    echo.
    echo ========================================
    echo Compilation failed!
    echo ========================================
)

pause
