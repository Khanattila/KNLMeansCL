# KNLMeansCL #
[![PayPal](https://www.paypalobjects.com/webstatic/en_US/btn/btn_donate_74x21.png)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=52QYMVWFRCQQY&lc=GB&item_name=KNLMeansCL&currency_code=EUR&bn=PP%2dDonationsBF%3abtn_donate_74x21%2epng%3aNonHosted) [![Build Status](https://travis-ci.org/Khanattila/KNLMeansCL.svg?branch=master)](https://travis-ci.org/Khanattila/KNLMeansCL) [![GitHub release](https://img.shields.io/github/release/Khanattila/KNLMeansCL.svg)](https://github.com/Khanattila/KNLMeansCL/releases) [![GitHub download](https://img.shields.io/github/downloads/Khanattila/KNLMeansCL/latest/total.svg)](https://github.com/Khanattila/KNLMeansCL/releases) 

**KNLMeansCL** is an optimized OpenCL implementation of the Non-local means de-noising algorithm. Every pixel is restored by the weighted average of all pixels in its search window. The level of averaging is determined by the filtering parameter h. 

For maximum performance the plugin should run on the GPU. The library is written in C/C++ and the source is available under the [GNU General Public License v3.0](https://github.com/Khanattila/KNLMeansCL/blob/master/LICENSE).

### AviSynth(+) and VapourSynth ###
For end user KNLMeansCL is a plugin for [AviSynth](http://avisynth.nl) / [AviSynth+](http://avs-plus.net/) and for [VapourSynth](http://www.vapoursynth.com). Windows, OS X and Linux are supported. For documentation and small benchmark see the [doc](https://github.com/Khanattila/KNLMeansCL/blob/master/DOCUMENTATION.md).

### Requirements ###
- OpenCL driver: [AMD](http://support.amd.com), [Intel](https://software.intel.com/en-us/articles/opencl-drivers), [NVIDIA](http://www.nvidia.com/download/find.aspx).
- AMD Radeon HD 7700 Series GPU or greater.
- Intel Graphics 2500/4000 or greater.
- NVIDIA GeForce GT 640 or greater.
- [Visual C++ Redistributable Package for Visual Studio 2013](http://www.microsoft.com/en-US/download/details.aspx?id=40784).
