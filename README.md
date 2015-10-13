## KNLMeansCL ##
[![Build Status](https://travis-ci.org/Khanattila/KNLMeansCL.svg?branch=master)](https://travis-ci.org/Khanattila/KNLMeansCL) [![GitHub release](https://img.shields.io/github/release/Khanattila/KNLMeansCL.svg)](https://github.com/Khanattila/KNLMeansCL/releases) [![GitHub download](https://img.shields.io/github/downloads/Khanattila/KNLMeansCL/latest/total.svg)](https://github.com/Khanattila/KNLMeansCL/releases) [![GitHub issues](https://img.shields.io/github/issues/Khanattila/KNLMeansCL.svg)](https://github.com/Khanattila/KNLMeansCL/issues) [![GitHub license](https://img.shields.io/github/license/Khanattila/KNLMeansCL.svg)](https://github.com/Khanattila/KNLMeansCL/blob/master/LICENSE) 

[![PayPal](https://www.paypalobjects.com/webstatic/en_US/btn/btn_donate_74x21.png)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=52QYMVWFRCQQY&lc=GB&item_name=KNLMeansCL&currency_code=EUR&bn=PP%2dDonationsBF%3abtn_donate_74x21%2epng%3aNonHosted) 

**KNLMeansCL** is an optimized OpenCL implementation of the Non-local means de-noising algorithm. Every pixel is restored by the weighted average of all pixels in its search window. The level of averaging is determined by the filtering parameter h. 

### AviSynth(+) and VapourSynth plugin ###
For user, KNLMeansCL is the implementation of both an [AviSynth](http://avisynth.nl)/[AviSynth+](http://avs-plus.net/) and a [VapourSynth](http://www.vapoursynth.com) source plugin. For documentation and small benchmark see the [DOCUMENTATION.md](https://github.com/Khanattila/KNLMeansCL/blob/master/DOCUMENTATION.md).

### Requirements ###
- OpenCL driver. AMD: [link](http://support.amd.com), NVIDIA: [link](http://www.nvidia.com/download/find.aspx), Intel: [link](https://software.intel.com/en-us/articles/opencl-drivers).
- [Intel Supported Graphics APIs and Features](http://www.intel.com/support/graphics/sb/CS-033757.htm).
- [Visual C++ Redistributable Package for Visual Studio 2013](http://www.microsoft.com/en-US/download/details.aspx?id=40784).
