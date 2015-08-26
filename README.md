# KNLMeansCL #

**KNLMeansCL** is an optimized OpenCL implementation of the Non-local means denoising algorithm. 
Every pixel is restored by the weighted average of all pixels in its search window. The level of averaging is determined by the filtering parameter h. 

### Supported image format ###
-  AviSynth: RGB32, Y8, YV411, YV12, YV16, YV24.
-  VapourSynth: Gray8, Gray16, GrayH, GrayS, YUV420P8, YUV422P8, YUV444P8, YUV410P8, YUV411P8, YUV440P8, YUV420P9, YUV422P9, YUV444P9, YUV420P10, YUV422P10, YUV444P10, YUV420P16, YUV422P16, YUV444P16, YUV444PH, YUV444PS, RGB24, RGB27, RGB30, RGB48, RGBH, RGBS.

### Syntax ###
```
KNLMeansCL (clip, int D (0), int A (2), int S (4), bool cmode(false), int wmode (1), 
	float h (1.2), string device_type ("default"), bool lsb_inout (false), bool info (false))
            
knlm.KNLMeansCL (clip clip, int d (0), int a (2), int s (4), int cmode(0), int wmode (1),  
        float h (1.2), string device_type ("default"), int info (0)) 
```

### Requirements ###
- OpenCL driver. AMD: [link](http://support.amd.com), NVIDIA: [link](http://www.nvidia.com/download/find.aspx), Intel: [link](https://software.intel.com/en-us/articles/opencl-drivers).
- [Visual C++ Redistributable Package for Visual Studio 2013](http://www.microsoft.com/en-US/download/details.aspx?id=40784), windows build.

### Parameters ###
- **clip / clip clip**
``` 
Video source.
```	
- **int D / d**
```
Set the number of past and future frame that the filter uses for denoising 
the current frame. 
D=0 uses 1 frame, while D=1 uses 3 frames and so on. Usually, larger it the better 
the result of the denoising. 
temporal size = (2 * D + 1).
search window size = temporal size * spatial size.
	
Default: 0.
```

- **int A / a**
```	
Set the radius of the search window. Usually, larger it the better the result of 
the denoising.
spatial size = (2 * A + 1)^2.
search window size = temporal size * spatial size.
	
Default: 2.
```
- **int S / s**
```	
Set the radius of the similarity neighborhood window. The impact on performance is low,
therefore it depends on the nature of the noise.
similarity neighborhood size = (2 * S + 1)^2.
	
Default: 4.
```
- **bool cmode / cmode**
```	
Use distance color instead of gray level. Process luma and chroma at once.
If color space is RGB, cmode is always true.
	
Default: false.
```	
- **int wmode / wmode**
```	
0 := Cauchy weighting function has a very slow decay. It assign larger weights to
     dissimilar blocks than the Leclerc robust function, which will eventually lead
     to oversmoothing.
1 := Leclerc weighting function has a faster decay, but still assigns positive weights
     to dissimilar blocks. Original NLMeans weighting function.
2 := Bisquare weighting function use a soft threshold to compare neighbourhoods (the 
     weight is 0 as soon as a giventhreshold is exceeded)
	
Default: 1.
```	
- **float h / h**
```	
Controls the strength of the filtering. Larger values will remove more noise.
	
Default: 1.2.
```
- **clip rclip / rclip**
```	
Extra reference clip option to do weighting calculation.
	
Default: source clip.
```
- **string device_type / device_type**
```	
CPU := An OpenCL device that is the host processor.
GPU := An OpenCL device that is a GPU. 
ACCELERATOR := Dedicated OpenCL accelerators (for example the IBM CELL Blade).
DEFAULT := The default OpenCL device in the system.
	
Default: DEFAULT.
```	
- **bool lsb_inout / not present**
```		
16-bit input and output clip.

Default: false.
```	
- **bool info / info**
```	
Display info on screen.
VapourSynth requires 8-bits per sample.

Default: false.
```
