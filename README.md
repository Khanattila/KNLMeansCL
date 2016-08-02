# KNLMeansCL #
 [![Build Status](https://travis-ci.org/Khanattila/KNLMeansCL.svg?branch=master)](https://travis-ci.org/Khanattila/KNLMeansCL) [![GitHub release](https://img.shields.io/github/release/Khanattila/KNLMeansCL.svg)](https://github.com/Khanattila/KNLMeansCL/releases) [![PayPal](https://img.shields.io/badge/donate-beer-orange.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=52QYMVWFRCQQY&lc=GB&item_name=KNLMeansCL&currency_code=EUR&bn=PP%2dDonationsBF%3abtn_donate_74x21%2epng%3aNonHosted)

**KNLMeansCL** is an optimized OpenCL implementation of the Non-local means de-noising algorithm. The NLMeans filter, originally proposed by Buades et al., is a very popular filter for the removal of white Gaussian noise, due to its simplicity and excellent performance. The strength of this algorithm is to exploit the repetitive character of the image in order to de-noise the image unlike conventional de-noising algorithms, which typically operate in a local neighbourhood.

### AviSynth(+) and VapourSynth ###
For end user KNLMeansCL is a plugin for [AviSynth](http://avisynth.nl) / [AviSynth+](http://avs-plus.net/) and for [VapourSynth](http://www.vapoursynth.com). Windows, OS X and Linux are supported. For documentation and small benchmark see the [doc](https://github.com/Khanattila/KNLMeansCL/blob/master/DOC.md).

### Requirements ###
- AMD HD 5800 Series GPU or greater. At least [AMD Catalyst™ software 11.12](http://support.amd.com).
- Intel Graphics 2500/4000 or greater. At least [OpenCL™ Drivers 2013](http://software.intel.com/en-us/articles/opencl-drivers).
- NVIDIA GeForce GT 640 or greater. At least [NVIDIA driver 350.12 WHQL](http://www.nvidia.com/download/find.aspx).
- CPU fallback is still available. Install [AMD APP SDK](http://developer.amd.com/tools-and-sdks/opencl-zone/amd-accelerated-parallel-processing-app-sdk/) or [Intel OpenCL™ Runtime](http://software.intel.com/en-us/articles/opencl-drivers).
- [Visual C++ Redistributable Package for Visual Studio 2015](http://www.microsoft.com/en-us/download/details.aspx?id=48145).

### Legacy - [v0.6.11](https://github.com/Khanattila/KNLMeansCL/releases/tag/v0.6.11)###
- AMD HD 5400 Series GPU or greater.
- Intel Graphics 2500/4000 or greater.
- NVIDIA GeForce 8400 GS or greater.
- CPU fallback is still available. Install [AMD APP SDK](http://developer.amd.com/tools-and-sdks/opencl-zone/amd-accelerated-parallel-processing-app-sdk/) or [Intel OpenCL™ Runtime](http://software.intel.com/en-us/articles/opencl-drivers).
- [Visual C++ Redistributable Package for Visual Studio 2013](http://www.microsoft.com/en-US/download/details.aspx?id=40784).
