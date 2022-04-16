@ECHO OFF

set A_CDSTART=%cd%

@REM CMake Check
cmake --version
set A_ERRORLEVEL=%ERRORLEVEL%
if %A_ERRORLEVEL% neq 0 goto missingCMake

@REM DearImGui
cd thirdparty
xcopy /y /f imconfig_but_in_a_different_folder_so_i_dont_have_another_repo.h imgui\imconfig.h

@REM GLFW
cd glfw
cmake . -B "build" -A Win32 -D CMAKE_BUILD_TYPE=Release -D GLFW_BUILD_EXAMPLES=OFF -D GLFW_BUILD_TESTS=OFF -D GLFW_BUILD_DOCS=OFF -D CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded 
cmake --build build --config Release

copy build\src\Release\glfw3.lib glfw3.lib

goto done

:missingCMake
echo =============================
echo Failed to install thirdparty!
echo Please download CMake!
echo =============================
cd %A_CDSTART%
exit /b 1

:done
echo =============================
echo Compile complete!
echo =============================
cd %A_CDSTART%
