/*
*	This file is part of KNLMeansCL,
*	Copyright(C) 2014-2015  Khanattila.
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

#define VERSION "0.6.4"

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
    #include <OpenCL/opencl.h>
#else
    #include <CL/cl.h>
#endif

#include "kernel.h"
#include "shared/startchar.h"

#ifdef _WIN32
    #define _stdcall __stdcall
    #include <avisynth.h>
#endif

#include <VapourSynth.h>
#include <VSHelper.h>

enum color_t { Gray, YUV, RGB };

typedef struct _device_list {
    cl_platform_id platform;
    cl_device_id device;
} device_list;

#ifdef __AVISYNTH_6_H__
class KNLMeansClass : public GenericVideoFilter {
private:
    const int d, a, s, wmode, ocl_id;
    const double h;
    PClip baby;
    const char* ocl_device;
    const bool cmode, lsb, info;
    void* hostBuffer;
    color_t color;
    cl_uint idmn[2], sum_devices;
    cl_platform_id platformID;
    cl_device_id deviceID;
    cl_context context;
    cl_program program;
    cl_kernel kernel[6];
    cl_mem mem_in[4], mem_out, mem_U[4];
    bool avs_equals(VideoInfo *v, VideoInfo *w);
    cl_int readBufferImage(PVideoFrame &frm, cl_command_queue command_queue,
        cl_mem image, const size_t origin[3], const size_t region[3]);
    cl_int writeBufferImage(PVideoFrame &frm, cl_command_queue command_queue,
        cl_mem image, const size_t origin[3], const size_t region[3]);
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
    int64_t d, a, s, cmode, wmode, info, ocl_id;
    double h;
    const char* ocl_device;
    color_t color;
    cl_uint idmn[2], sum_devices;
    cl_platform_id platformID;
    cl_device_id deviceID;
    cl_context context;
    cl_program program;
    cl_kernel kernel[8];
    cl_mem mem_in[4], mem_out, mem_U[4], mem_P[3];
} KNLMeansData;
#endif //__VAPOURSYNTH_H__

inline size_t mrounds(const size_t num, const size_t mul) {
    return (size_t) ceil((double) num / (double) mul) * mul;
}

template <typename T>T inline fastmax(const T& left, const T& right) {
    return left > right ? left : right;
}

template <typename T>T inline clamp(const T& value, const T& low, const T& high) {
    return value < low ? low : (value > high ? high : value);
}