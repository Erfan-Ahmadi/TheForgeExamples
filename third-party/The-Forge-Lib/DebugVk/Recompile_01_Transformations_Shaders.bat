setlocal
set errorlevel=dummy
set errorlevel=
echo Compiling ..\src\01_Transformations\Shaders\Vulkan\basic.frag to spirv
copy %0 "D:\__projects__\The-Forge\Examples_3\Unit_Tests\PC Visual Studio 2017\x64\DebugVk\Recompile_01_Transformations_Shaders.bat"
"C:\VulkanSDK\1.1.106.0\bin\glslangValidator.exe" -V "D:\__projects__\The-Forge\Examples_3\Unit_Tests\src\01_Transformations\Shaders\Vulkan\basic.frag" -o "D:\__projects__\The-Forge\Examples_3\Unit_Tests\src\01_Transformations\Shaders\Vulkan\basic.frag\..\Binary\basic.frag.bin"
echo Compiling ..\src\01_Transformations\Shaders\Vulkan\basic.vert to spirv
copy %0 "D:\__projects__\The-Forge\Examples_3\Unit_Tests\PC Visual Studio 2017\x64\DebugVk\Recompile_01_Transformations_Shaders.bat"
"C:\VulkanSDK\1.1.106.0\bin\glslangValidator.exe" -V "D:\__projects__\The-Forge\Examples_3\Unit_Tests\src\01_Transformations\Shaders\Vulkan\basic.vert" -o "D:\__projects__\The-Forge\Examples_3\Unit_Tests\src\01_Transformations\Shaders\Vulkan\basic.vert\..\Binary\basic.vert.bin"
echo Compiling ..\src\01_Transformations\Shaders\Vulkan\skybox.frag to spirv
copy %0 "D:\__projects__\The-Forge\Examples_3\Unit_Tests\PC Visual Studio 2017\x64\DebugVk\Recompile_01_Transformations_Shaders.bat"
"C:\VulkanSDK\1.1.106.0\bin\glslangValidator.exe" -V "D:\__projects__\The-Forge\Examples_3\Unit_Tests\src\01_Transformations\Shaders\Vulkan\skybox.frag" -o "D:\__projects__\The-Forge\Examples_3\Unit_Tests\src\01_Transformations\Shaders\Vulkan\skybox.frag\..\Binary\skybox.frag.bin"
echo Compiling ..\src\01_Transformations\Shaders\Vulkan\skybox.vert to spirv
copy %0 "D:\__projects__\The-Forge\Examples_3\Unit_Tests\PC Visual Studio 2017\x64\DebugVk\Recompile_01_Transformations_Shaders.bat"
"C:\VulkanSDK\1.1.106.0\bin\glslangValidator.exe" -V "D:\__projects__\The-Forge\Examples_3\Unit_Tests\src\01_Transformations\Shaders\Vulkan\skybox.vert" -o "D:\__projects__\The-Forge\Examples_3\Unit_Tests\src\01_Transformations\Shaders\Vulkan\skybox.vert\..\Binary\skybox.vert.bin"

:VCEnd
exit %errorlevel%
