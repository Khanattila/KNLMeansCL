#### Syntax ####
```
AviSynth:               KNLMeansCL (clip, int d (0), int a (2), int s (4), 
                                float h (1.2), bool cmode(false),  int wmode (1), 
                                float wref (1.0), string device_type ("auto"),
                                int device_id (0), bool lsb_inout (false), 
                                bool info (false))

VapourSynth:            knlm.KNLMeansCL (clip clip, int d (0), int a (2), int s (4), 
                                float h (1.2), int cmode(False), int wmode (1), 
                                float wref (1.0), string device_type ("auto"),  
                                int device_id (0), int info (False)) 
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
                        denoising the current frame. d=0 uses 1 frame, while d=1 uses 3
                        frames and so on. Usually, larger it the better the result of the
                        denoising. Temporal size = (2 * d + 1).

                        Default: 0.


int a                   Set the radius of the search window. a=0 uses 1 pixel, while a=1 
                        uses 9 pixels and son on. Usually, larger it the better the result
                        of the denoising. Spatial size = (2 * a + 1)^2.
                        Total search window size = temporal size * spatial size.
	
                        Default: 2.


int s                   Set the radius of the similarity neighborhood window. The impact 
                        on performance is low, therefore it depends on the nature of the 
                        noise. Similarity neighborhood size = (2 * s + 1)^2.
	
                        Default: 4.

                        
float h                 Controls the strength of the filtering. Larger values will 
                        remove more noise.
	                
                        Default: 1.2.


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
                        
                       
float wref              Amount of original pixel to contribute to the filter output, 
                        relative to the weight of the most similar pixel found.
	                
                        Default: 1.0.
                        
	
clip rclip              Extra reference clip option to do weighting calculation.
	
                        Default: not set.


string device_type      CPU := An OpenCL device that is the host processor.
                        GPU := An OpenCL device that is a GPU. 
                        ACCELERATOR := Dedicated OpenCL accelerators.
                        AUTO := GPU -> ACCELERATOR -> CPU.
	
                        Default: AUTO.
                        
                        
int device_id           The 'device_id'+1º device of type 'device_type' in the system.
                        Example: [device_type = "GPU", device_id = 1] return the second 
                        GPU in the system.
			
                        Default: 0.
	
	
bool lsb_inout          AviSynth hack. Set 16-bit input and output clip.

                        Default: false.
	
	
bool info               Display info on screen. It requires YUV color space.

                        Default: false.
```

#### Troubleshooting ####
Some NVIDIA users have reported a problem with the library 'OpenCL.dll'. Replace it (C:\Windows\System32\OpenCL.dll and/or C:\Windows\SysWOW64\OpenCL.dll) with the one provided in the .zip or replace it with the one provided by NVIDIA (C:\Program Files\NVIDIA Corporation\OpenCL\OpenCL.dll).

#### Benchmark ####

| | cmode | AviSynth | VapourSynth |
| :--------------- | :---------------: | :---------------: | :---------------: |
| YUV420P8 | False | 37 fps  | 35 fps  |
| YUV420P9  | False | - | 33 fps |
| YUV420P10  | False | -  | 33 fps  |
| YUV420P16  | False | 35 fps  | 34 fps  |
| YUV444P8  | True | 28 fps  | 27 fps  |
| YUV444P9  | True | -  | 22 fps  |
| YUV444P10  | True | -  | 23 fps  |
| YUV444P16  | True | 23 fps  | 23 fps  |
| RGB24  | - | - | 27 fps  |
| RGB27  | - | - | 23 fps  |
| RGB30  | - | - | 23 fps  |
| RGB48  | - | - | 23 fps  |
| RGBA32 | - | 28 fps  | - |

720p, KNLMeansCL(d=1, a=2, s=4, device_type="GPU") - v0.7.0.

Tested with the following configuration: Intel Core i5 2500K (4.2GHz), 8GB DDR3-1600 MHz, NVIDIA GeForce GTX 760, Windows 10 64bit.
