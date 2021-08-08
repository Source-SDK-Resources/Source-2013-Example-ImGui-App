@ECHO OFF
echo =====================================================================
echo If this batch does not work, MAKE SURE YOU RUN IT FROM VS2013 DEV CMD
echo =====================================================================

cd thirdparty
xcopy /y /f imconfig_but_in_a_different_folder_so_i_dont_have_another_repo.h imgui\imconfig.h

cd glfw
cmake . -B "build" -G "Visual Studio 12 2013" -A Win32 -D GLFW_BUILD_EXAMPLES=OFF -D GLFW_BUILD_TESTS=OFF -D GLFW_BUILD_DOCS=OFF -D CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded 
msbuild build\glfw.sln /p:Configuration=Release /m:8
xcopy /y /f build\src\Release\glfw3.lib glfw3.lib