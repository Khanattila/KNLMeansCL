## Change Log ##
**-v 0.7.6**
  - Fixed wrong version displaying.
  - VapourSynth: fixed wrong API usage.

**-v 0.7.5**
  - New 'wref' (default 1.0) parameter.
  - Improved automatic selection of OpenCL device.
  - Changed parameters order. 
  - VapourSynth: fixed missing 's' value check. 

**- v0.7.4**
  - Workaround for GeForce 900 Series.
   
**- v0.7.3**
  - Removed 'default' and 'all' from 'device_type'.
  - Improved OpenCL platform control.
  - VapourSynth: fixed frame properties.

**- v0.7.2**
  - VapourSynth: fixed memory leak.
  
**- v0.7.1**
  - Fixed 'clSetKernelArg' error in some configuration.
  
**- v0.7.0**
  - Performance increase: temporal up to 35% faster.
  - New 'auto' device_type.
  - Changed default 'd' value from '0' to '1'.
  - Changed min 'a' value from '0' to '1'.
  - OpenCL 1.2 support now required.
  - AviSynth 2.6.0a1-a5 backward compatibility.
  - Fixed Mac OS X version.
  
**- v0.6.11**
  - Performance increase: up to 50% faster.
  - Fixed bad processing borders.
  - AviSynth: OpenMP no more needed.
  - VapourSynth: fixed bad support of P9/P10 rclip.

**- v0.6.4**
  - VapourSynth: OpenMP no more needed.

**- v0.6.3**
  - Enhanced OpenCL device selection (device_id).
  - VapourSynth linux: fixed wrong locate.
  - Minor changes and bug fixes.

**- v0.6.2**
  - Enhanced error log.
  - Fixed warning use of logical '||' with constant operand.

**- v0.6.1**
  - Fixed 'h' strength in some situations.
  - Minor changes.

**- v0.6.0**
  - Added color distance (cmode).
  - VapourSynth: added support for missing color format.
  - Changed weighted RGB distance.
  - No more SSE2 / SSE3 required. 
  - Minor changes and bug fixes.
  
**- v0.5.9**
  - Fixed rare issues with OpenCL compiler, again. 

**- v0.5.8**
  - Fixed rare issues with OpenCL compiler.
  - Fixed rare issues with clGetPlatformIDs.
  - AviSynth: clip does not need to be aligned.

**- v0.5.7**
  - Added extra reference clip (rclip).
  - Weighted RGB distance.

**- v0.5.6**
  - VapourSynth: fixed temporal support.
  - VapourSynth: fixed arg device_type ignored.
  - Fixed possible opencl device not available. 

**- v0.5.5**
  - AviSynth: added RGB32 support.

**- v0.5.4**
  - AviSynth: fixed temporal support with 16-bits clip.  

**- v0.5.3**
  - VapourSynth: better accuracy with 32-bits.
  - VapourSynth: fixed arguments.

**- v0.5.2**
  - Added AviSynth+ support.
  - VapourSynth: fixed memory leak.

**- v0.5.1**
  - Added VapourSynth support.
