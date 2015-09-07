## KNLMeansCL ##

**KNLMeansCL** is an optimized OpenCL implementation of the Non-local means denoising algorithm. 
Every pixel is restored by the weighted average of all pixels in its search window. The level of averaging is determined by the filtering parameter h. 

#### Requirements ####
- OpenCL driver. AMD: [link](http://support.amd.com), NVIDIA: [link](http://www.nvidia.com/download/find.aspx), Intel: [link](https://software.intel.com/en-us/articles/opencl-drivers).
- [Visual C++ Redistributable Package for Visual Studio 2013](http://www.microsoft.com/en-US/download/details.aspx?id=40784).

#### Syntax ####
```
AviSynth:               KNLMeansCL (clip, int D (0), int A (2), int S (4), 
                                bool cmode(false),  int wmode (1), float h (1.2), 
                                string device_type ("default"), int device_id (0),
                                bool lsb_inout (false), bool info (false))

VapourSynth:            knlm.KNLMeansCL (clip clip, int d (0), int a (2), int s (4), 
                                int cmode(0), int wmode (1), float h (1.2), 
                                string device_type ("default"), int device_id (0), 
                                int info (0)) 
```

#### Supported image format ####
```
AviSynth:               RGB32, Y8, YV411, YV12, YV16, YV24.

VapourSynth: 	        All.
```


#### Parameters ####
``` 
clip clip               Video source.
	

int d                   Set the number of past and future frame that the filter uses for 
                        denoising the current frame. D=0 uses 1 frame, while D=1 uses 3
                        frames and so on. Usually, larger it the better the result of the
                        denoising. Temporal size = (2 * D + 1).

                        Default: 0.


int a                   Set the radius of the search window. A=0 uses 1 pixel, while D=1 
                        use 9 pixels and son on. Usually, larger it the better the result
                        of the denoising. Spatial size = (2 * A + 1)^2.
                        Total search window size = temporal size * spatial size.
	
                        Default: 2.


int s                   Set the radius of the similarity neighborhood window. The impact 
                        on performance is low, therefore it depends on the nature of the 
                        noise. Similarity neighborhood size = (2 * S + 1)^2.
	
                        Default: 4.


bool cmode              Use color distance instead of gray intensities. Normally 
                        KNLMeansCL processes only Luma and copy Chroma if present. If 
                        cmode is true KNLMeansCL processes Luma and Chroma together. 
                        If color space is RGB, cmode is always true.
	
                        Default: false.
	

int wmode               0 := Cauchy weighting function has a very slow decay. It assigns 
                        larger weights to dissimilar blocks than the Leclerc robust 
                        function, which will eventually lead to oversmoothing.
                        
                        1 := Leclerc weighting function has a faster decay, but still
                        assigns positive weights to dissimilar blocks. Original NLMeans 
                        weighting function.
                        
                        2 := Bisquare weighting function use a soft threshold to compare 
                        neighbourhoods (the weight is 0 as soon as a giventhreshold is 
                        exceeded).
	
                        Default: 1.
	
	
float h                 Controls the strength of the filtering. Larger values will 
                        remove more noise.
	                
                        Default: 1.2.


clip rclip              Extra reference clip option to do weighting calculation.
	
                        Default: not set.


string device_type      CPU := An OpenCL device that is the host processor.
                        GPU := An OpenCL device that is a GPU. 
                        ACCELERATOR := Dedicated OpenCL accelerators.
                        DEFAULT := The default OpenCL device in the system.
                        ALL := All OpenCL devices available in the system.
	
                        Default: DEFAULT.
                        

int device_id		The 'device_id'º device of type 'device_type' in the system.
                        Example: [device_type = "GPU", device_id = 1] return the second 
                        GPU in the system.
			
                        Default: 0.
	
	
bool lsb_inout          AviSynth hack. Set 16-bit input and output clip.

                        Default: false.
	
	
bool info               Display info on screen. It requires YUV color space.

                        Default: false.
```
