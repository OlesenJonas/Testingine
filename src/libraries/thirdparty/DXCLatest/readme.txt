these are prebuilt binaries from https://github.com/microsoft/DirectXShaderCompiler
compiled in VS2022 following https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/BuildingAndTestingDXC.rst#using-visual-studio-s-cmake-integration
(if cmake configuration fails because of TAEF, make sure the WDK is installed https://learn.microsoft.com/en-us/windows-hardware/drivers/other-wdk-downloads#step-2-install-the-wdk.
 DXC guide says installing the windows SDK in visual studio (also tested it in 2019) includes the WDK, but that doesnt seem to be the case)