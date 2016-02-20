# KNLMeansCL #
[![PayPal](https://www.paypalobjects.com/webstatic/en_US/btn/btn_donate_74x21.png)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=52QYMVWFRCQQY&lc=GB&item_name=KNLMeansCL&currency_code=EUR&bn=PP%2dDonationsBF%3abtn_donate_74x21%2epng%3aNonHosted) [![Build Status](https://travis-ci.org/Khanattila/KNLMeansCL.svg?branch=master)](https://travis-ci.org/Khanattila/KNLMeansCL) [![GitHub release](https://img.shields.io/github/release/Khanattila/KNLMeansCL.svg)](https://github.com/Khanattila/KNLMeansCL/releases) [![GitHub download](https://img.shields.io/github/downloads/Khanattila/KNLMeansCL/latest/total.svg)](https://github.com/Khanattila/KNLMeansCL/releases) 

**KNLMeansCL** is an optimized OpenCL implementation of the Non-local means de-noising algorithm. Every pixel is restored by the weighted average of all pixels in its search window. The level of averaging is determined by the filtering parameter h. 

For maximum performance the plugin should run on the GPU. The library is written in C/C++ and the source is available under the [GNU General Public License v3.0](https://github.com/Khanattila/KNLMeansCL/blob/master/LICENSE).

### AviSynth(+) and VapourSynth ###
For end user KNLMeansCL is a plugin for [AviSynth](http://avisynth.nl) / [AviSynth+](http://avs-plus.net/) and for [VapourSynth](http://www.vapoursynth.com). Windows, OS X and Linux are supported. For documentation and small benchmark see the [doc](https://github.com/Khanattila/KNLMeansCL/blob/master/DOC.md).

### Requirements ###
- AMD HD 5800 Series GPU or greater. At least [AMD Catalyst™ software 11.12](http://support.amd.com).
- Intel Graphics 2500/4000 or greater. At least [OpenCL™ Drivers 2013](http://software.intel.com/en-us/articles/opencl-drivers).
- NVIDIA GeForce GT 640 or greater. At least [NVIDIA driver 350.12 WHQL](http://www.nvidia.com/download/find.aspx).
- If you have an older device, you can use this [version](http://github.com/Khanattila/KNLMeansCL/releases/tag/v0.6.11).
- [Visual C++ Redistributable Package for Visual Studio 2015](http://www.microsoft.com/en-us/download/details.aspx?id=48145).
