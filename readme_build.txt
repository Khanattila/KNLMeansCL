Building KNLMeansCL under Visual Studio 2019
============================================
howto by pinterf

KNLMeansCL needs resources from the following external projects:
- AviSynth+
- VapourSynth
- Khronos Group OpenCL
- (Intel/NVidia/.. OpenCL SDK)

You can clone/download the necessary subprojects to their default place
or copy the necessary header files into a project subfolder.

Either way you have to provide the necessary paths for the compiler and linker. 

1.) OpenCL headers
  Download OpenCL headers from

  https://github.com/KhronosGroup/OpenCL-Headers

  download zip, extract its internal folder to
  
  <path>\KNLMeansCL\KNLMeansCL\openCL\

  so as header files appear in 

  <path>\Github\KNLMeansCL\KNLMeansCL\openCL\CL

2.) Avisynth headers
  https://github.com/AviSynth/AviSynthPlus

  (Note: as of 2020.04.29 appropriate headers with v8 interface are in the "neo" branch
   and not in master)

  Copy avisynth.h header and its avs folder from
  
  <path>\AviSynthPlus\avs_core\include\

  to KNLMeans project's avisynth folder (create if there is none)
  c:\Github\KNLMeansCL\KNLMeansCL\avisynth\

3.) VapourSynth headers

  http://www.vapoursynth.com/doc/apireference.html#public-headers
  copy the following files
    VapourSynth.h
    VSHelper.h
  to
    c:\Github\KNLMeansCL\KNLMeansCL\vapoursynth\

4.) Set path to include directories:
    
    Project Properties|C++|General|Additional Include Directories
  
    Set to: vapoursynth;avisynth;openCL

5.) Getting OpenCL.lib
  Info: This lib is an abstract one and will use the appropriate AMD/NVIDIA/Intel OpenCL.dll runtime
  installed along with other graphics drivers on your system.
  
  You don't even have to have and SDK, you can compile this lib for yourself from Khronos sources.
  Or grab it ready-made from any downloaded SDK.
    
  Download and install an OpenCL SDK

  5.1) Intel versions
  https://software.intel.com/en-us/opencl-sdk
  
  Find
  c:\Program Files (x86)\IntelSWTools\system_studio_2020\OpenCL\sdk\lib\Win32\
  c:\Program Files (x86)\IntelSWTools\system_studio_2020\OpenCL\sdk\lib\x64\
  
  5.2) NVIDIA versions
  Download Cuda ToolKit from:
  https://developer.nvidia.com/cuda-downloads
  
  c:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v10.2\lib\Win32\
  c:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v10.2\lib\x64\

6.) Set library path 
    If you include them directly, different ones exist for Win32 and x64 
    
    Project Properties|Linker|Input|Additional Dependencies

    Add the following entries
    for x64 platform: .\lib\x64\openCL.lib;
    for Win32 platform: .\lib\x86\openCL.lib;
