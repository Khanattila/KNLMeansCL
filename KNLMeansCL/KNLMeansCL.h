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

#define VERSION "0.5.9"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#define snprintf sprintf_s
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <fstream>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

#include "kernel.h"
#include "startchar.h"

#ifdef _WIN32
#include "avisynth.h"
#endif

#include <VapourSynth.h>
#include <VSHelper.h>

enum color_t { Gray, YUV, RGB, YUV30, RGB30 };

#ifdef __AVISYNTH_6_H__
class KNLMeansClass : public GenericVideoFilter {
private:
    const int D, A, S, wmode;
    const double h;
    PClip baby;
    const char* ocl_device;
    const bool lsb, info;
    void* hostBuffer;
    color_t color;
    cl_uint idmn[2];
    cl_platform_id platformID;
    cl_device_id deviceID;
    cl_context context;
    cl_program program;
    cl_kernel kernel[6];
    cl_mem mem_in[4], mem_out, mem_U[4];
    bool avs_equals(VideoInfo *v, VideoInfo *w);
    cl_uint readBufferImage(PVideoFrame &frm, cl_command_queue command_queue,
        cl_mem image, const size_t origin[3], const size_t region[3]);
    cl_uint writeBufferImage(PVideoFrame &frm, cl_command_queue command_queue,
        cl_mem image, const size_t origin[3], const size_t region[3]);
public:
    KNLMeansClass(PClip _child, const int _D, const int _A, const int _S, const int _wmode, const double _h,
        PClip _baby, const char* _ocl_device, const bool _lsb, const bool _info, IScriptEnvironment* env);
    ~KNLMeansClass();
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};
#endif //__AVISYNTH_6_H__

#ifdef VAPOURSYNTH_H
typedef struct {
    VSNodeRef *node, *knot;
    const VSVideoInfo *vi;
    int64_t d, a, s, wmode, info;
    double h;
    const char* ocl_device;
    void* hostBuffer;
    color_t color;
    cl_uint idmn[2];
    cl_platform_id platformID;
    cl_device_id deviceID;
    cl_context context;
    cl_program program;
    cl_kernel kernel[6];
    cl_mem mem_in[4], mem_out, mem_U[4];
} KNLMeansData;
#endif //__VAPOURSYNTH_H__

inline size_t mrounds(const size_t num, const size_t mul) {
    return (size_t) ceil((double) num / (double) mul) * mul;
}

template <typename T>T fastmax(const T& left, const T& right) {
    return left > right ? left : right;
}

template <typename T>T fastmin(const T& left, const T& right) {
    return left < right ? left : right;
}

template <typename T>T clamp(const T& value, const T& low, const T& high) {
    return value < low ? low : (value > high ? high : value);
}