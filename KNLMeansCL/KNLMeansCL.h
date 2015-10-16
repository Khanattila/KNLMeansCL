/*
*	This file is part of KNLMeansCL,
*	Copyright(C) 2015  Edoardo Brunetti.
*
*	KNLMeansCL is free software: you can redistribute it and/or modify
*	it under the terms of the GNU Lesser General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	KNLMeansCL is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*	GNU Lesser General Public License for more details.
*
*	You should have received a copy of the GNU Lesser General Public License
*	along with KNLMeansCL. If not, see <http://www.gnu.org/licenses/>.
*/

#define VERSION "0.7.2"

#ifdef _MSC_VER
    #pragma warning (disable : 4514 4710 4820)
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <clocale>
#include <cmath>
#include <cstring>
#include <fstream>

#ifdef __APPLE__
    #include <OpenCL/cl.h>
#else
    #include <CL/cl.h>
#endif

#include "kernel.h"
#include "shared/oclUtils.h"
#include "shared/startchar.h"

#ifdef _WIN32
   #include <avisynth.h>
#endif

#include <VapourSynth.h>
#include <VSHelper.h>

enum { 
    COLOR_GRAY    = 1 << 0, COLOR_YUV     = 1 << 1, COLOR_RGB     = 1 << 2,
    CLIP_UNORM    = 1 << 3, CLIP_UNSIGNED = 1 << 4, CLIP_STACKED  = 1 << 5,
    EXTRA_NONE    = 1 << 6, EXTRA_CLIP    = 1 << 7
};

#ifdef __AVISYNTH_6_H__
class KNLMeansClass : public GenericVideoFilter {
private:
    const int d, a, s, wmode, ocl_id;
    const double h;
    PClip baby;
    const char* ocl_device;
    const bool cmode, lsb, info;
    unsigned int clip_t;
    cl_int idmn[3];
    cl_uint num_devices;
    cl_platform_id platformID;
    cl_device_id deviceID;
    cl_context context;
    cl_program program;
    cl_kernel kernel[nlmNumberKernels];
    cl_mem mem_in[2], mem_out, mem_U[4], mem_P[3];
    bool equals(VideoInfo *v, VideoInfo *w);
public:
    KNLMeansClass(PClip _child, const int _d, const int _a, const int _s, const bool _cmode, const int _wmode, 
        const double _h, PClip _baby, const char* _ocl_device, const int _ocl_id, const bool _lsb, const bool _info, 
        IScriptEnvironment* env);
    ~KNLMeansClass();
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};
#endif //__AVISYNTH_6_H__

#ifdef VAPOURSYNTH_H
typedef struct {
    VSNodeRef *node, *knot;
    const VSVideoInfo *vi;
    int64_t d, a, s, cmode, wmode, ocl_id, info;
    double h;
    const char* ocl_device;
    unsigned int bit_shift, clip_t;
    cl_int idmn[2];
    cl_uint num_devices;
    cl_platform_id platformID;
    cl_device_id deviceID;
    cl_context context;
    cl_program program;
    cl_kernel kernel[nlmNumberKernels];
    cl_mem mem_in[2], mem_out, mem_U[4], mem_P[3];
    bool equals(const VSVideoInfo *v, const VSVideoInfo *w);
} KNLMeansData;
#endif //__VAPOURSYNTH_H__
