## Benchmark ##

| | AviSynth | VapourSynth |
| :------------- | :-------------: | :-------------: |
| YUV420P8  | 72 fps  | 71 fps  |
| YUV420P9  | - | 63 fps |
| YUV420P10  | -  | 63 fps  |
| YUV420P16  | 64 fps  | 64 fps  |
| YUV444P8  | 48 fps  | 49 fps  |
| YUV444P9  | -  | 40 fps  |
| YUV444P10  | -  | 40 fps  |
| YUV444P16  | 39 fps  | 40 fps  |
| RGB24  | -  | 49 fps  |
| RGB27  | -  | 40 fps  |
| RGB30  | -  | 40 fps  |
| RGB48  | -  | 41 fps  |
| RGBA32  | 48 fps  | - |

1080p, KNLMeansCL(d=0, a=2, device_type="GPU").

Tested with the following configuration: Intel Core i5 2500K (4.2GHz), 8GB DDR3-1600 MHz, NVIDIA GeForce GTX 760, Windows 10 64bit.
