@echo off
rem Build-3ds.bat - build ctr\pokedaisy.3dsx natively with devkitPro, no Docker.
rem The same steps `make 3ds` runs in the devkitpro/devkitarm image (see Makefile).
rem
rem   Build-3ds          configure + build, copy the .3dsx to ctr\
rem   Build-3ds clean    delete ctr\build first and configure from scratch
rem
rem Needs devkitPro (devkitARM, libctru, citro2d, citro3d, 3dsxtool, smdhtool:
rem `dkp-pacman -S 3ds-dev`), CMake, make and git on PATH, and DEVKITPRO set.
setlocal EnableExtensions

set "HERE=%~dp0"
set "CTR=%HERE%ctr"
set "MGBA=%HERE%..\third_party\mgba"
set "FAILED="

echo == Checking prerequisites

if not defined DEVKITPRO (
    echo   [missing] DEVKITPRO is not set. Install devkitPro from https://devkitpro.org/wiki/Getting_Started, then open a new terminal.
    goto :fail
)

rem DEVKITPRO is an MSYS path like /opt/devkitpro; find the Windows folder behind it.
set "DKP=%DEVKITPRO%"
where cygpath >nul 2>nul && for /f "delims=" %%P in ('cygpath -w "%DEVKITPRO%"') do set "DKP=%%P"
if not exist "%DKP%\devkitARM" if exist "C:\devkitPro\devkitARM" set "DKP=C:\devkitPro"
echo   [ok] DEVKITPRO = %DEVKITPRO%  -^> %DKP%

call :need_tool cmake "CMake - install it with: pacman -S cmake, or from https://cmake.org"
call :need_tool make "make - it comes with devkitPro's MSYS2; run this from a shell that has it on PATH"
call :need_tool git "git - install it from https://git-scm.com"
call :need_file "%DKP%\devkitARM\bin\arm-none-eabi-gcc.exe" "devkitARM - run: dkp-pacman -S 3ds-dev"
call :need_file "%DKP%\libctru\lib\libctru.a" "libctru - run: dkp-pacman -S 3ds-dev"
call :need_file "%DKP%\libctru\lib\libcitro2d.a" "citro2d - run: dkp-pacman -S 3ds-dev"
call :need_file "%DKP%\libctru\lib\libcitro3d.a" "citro3d - run: dkp-pacman -S 3ds-dev"
call :need_file "%DKP%\tools\bin\3dsxtool.exe" "3dsxtool - run: dkp-pacman -S 3ds-dev"
call :need_file "%DKP%\tools\bin\smdhtool.exe" "smdhtool - run: dkp-pacman -S 3ds-dev"
if defined FAILED goto :fail

if not exist "%MGBA%\CMakeLists.txt" (
    echo   [missing] the mGBA submodule - fetching it
    git -C "%HERE%.." submodule update --init third_party/mgba
    if errorlevel 1 goto :fail
)
echo   [ok] mGBA submodule

if /i "%~1"=="clean" if exist "%CTR%\build" (
    echo == Removing ctr\build
    rmdir /s /q "%CTR%\build"
)

rem CMAKE_POLICY_VERSION_MINIMUM: CMake 4 refuses mGBA 0.10.5's cmake_minimum_required(3.1)
rem without it; older CMake ignores it.
rem CMAKE_DEPENDS_USE_COMPILER=OFF: devkitARM writes C:/... paths into its .d files, and make
rem reads that colon as a rule ("multiple target patterns") on every build after the first.
echo == Configuring
pushd "%CTR%"
cmake -S . -B build -G "Unix Makefiles" -Wno-dev ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
    -DCMAKE_DEPENDS_USE_COMPILER=OFF ^
    -DCMAKE_TOOLCHAIN_FILE=../../third_party/mgba/src/platform/3ds/CMakeToolchain.txt
if errorlevel 1 goto :fail_popd

echo == Building
cmake --build build -j %NUMBER_OF_PROCESSORS%
if errorlevel 1 goto :fail_popd

copy /y build\pokedaisy.3dsx . >nul
if errorlevel 1 goto :fail_popd
popd

echo.
for %%F in ("%CTR%\pokedaisy.3dsx") do echo Built %%~fF, %%~zF bytes
exit /b 0

rem No ( ) blocks below: a path with parentheses in it would end one early.
:need_tool
where %1 >nul 2>nul
if not errorlevel 1 echo   [ok] %1& exit /b 0
echo   [missing] %~2
set "FAILED=1"
exit /b 0

:need_file
if exist "%~1" echo   [ok] %~nx1& exit /b 0
echo   [missing] %~2
echo             not found: %~1
set "FAILED=1"
exit /b 0

:fail_popd
popd
:fail
echo.
echo Build failed.
exit /b 1
