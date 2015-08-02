# KNLMeansCL #

**KNLMeansCL** is an optimized pixelwise OpenCL implementation of the Non-local means denoising algorithm. 
Every pixel is restored by the weighted average of all pixels in its search window. The level of averaging is determined by the filtering parameter h. 

Don't process chroma channels, copy it from the source clip if present.

### Supported color space ###
-  AviSynth: RGB32, Y8, YV411, YV12, YV16, YV24.
-  VapourSynth: GrayG, YUV420PG, YUV422PG, YUV444PG, YUV410PG, YUV411PG, YUV440PG.

*The generic type name G is used to indicate 8, 16, H and S. Progressive only*.

### Syntax ###
```
KNLMeansCL (clip, int D (0), int A (2), int S (4), int wmode (1), float h (1.2), 
            string device_type ("default"), bool lsb_inout (false), bool info (false))
            
knlm.KNLMeansCL (clip clip, int d (0), int a (2), int s (4), int wmode (1),  
                 float h (1.2), string device_type ("default"), int info (0)) 
```

### Requirements ###
- SSE2 / SSE3 instruction set.
- OpenCL driver. AMD: [link](http://support.amd.com), NVIDIA: [link](http://www.nvidia.com/download/find.aspx), Intel: [link](https://software.intel.com/en-us/articles/opencl-drivers).
- [Visual C++ Redistributable Package for Visual Studio 2013](http://www.microsoft.com/en-US/download/details.aspx?id=40784), windows build.

### Parameters ###
- **clip (clip)**
``` 
Video source.
```	
- **int D (d)**
```
Set the number of past and future frame that the filter uses for denoising 
the current frame. 
D=0 uses 1 frame, while D=1 uses 3 frames and so on. Usually, larger it the better 
the result of the denoising. 
temporal size = (2 * D + 1).
search window size = temporal size * spatial size.
	
Default: 0.
```

- **int A (a)**
```	
Set the radius of the search window. Usually, larger it the better the result of 
the denoising.
spatial size = (2 * A + 1)^2.
search window size = temporal size * spatial size.
	
Default: 2.
```
- **int S (s)**
```	
Set the radius of the similarity neighborhood window. The impact on performance is low,
therefore it depends on the nature of the noise.
similarity neighborhood size = (2 * S + 1)^2.
	
Default: 4.
```
- **int wmode (wmode)**
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
- **float h (h)**
```	
Controls the strength of the filtering. Larger values will remove more noise.
	
Default: 1.2.
```
- **clip rclip (rclip)**
```	
Extra reference clip option to do weighting calculation.
	
Default: source clip.
```
- **string device_type (device_type)**
```	
CPU := An OpenCL device that is the host processor.
GPU := An OpenCL device that is a GPU. 
ACCELERATOR := Dedicated OpenCL accelerators (for example the IBM CELL Blade).
DEFAULT := The default OpenCL device in the system.
	
Default: DEFAULT.
```	
- **bool lsb_inout (not present)**
```		
16-bit input and output clip.

Default: false.
```	
- **bool info (info)**
```	
Display info on screen.
VapourSynth requires 8-bits per sample.

Default: false.
```
