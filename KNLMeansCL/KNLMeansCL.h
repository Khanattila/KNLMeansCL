/*
*    This file is part of KNLMeansCL,
*    Copyright(C) 2015-2017  Edoardo Brunetti.
*
*    KNLMeansCL is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    KNLMeansCL is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with KNLMeansCL. If not, see <http://www.gnu.org/licenses/>.
*/

#define VERSION "1.1.0"
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS

#include <cstdint>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <clocale>
#include <cstring>
#include <fstream>

#ifdef __APPLE__
#    include <OpenCL/cl.h>
#else
#    include <CL/cl.h>
#endif

#include "kernel.h"
#include "shared/oclUtils.h"
#include "shared/startchar.h"

#ifdef _WIN32
#    include <avisynth.h>
#endif

#include <VapourSynth.h>
#include <VSHelper.h>

#ifdef __AVISYNTH_6_H__
struct _NLMAvisynth : public GenericVideoFilter {
private:
    const int d, a, s, wmode, ocl_id, ocl_x, ocl_y, ocl_r;
    const double wref, h;
    PClip baby;
    const char *channels, *ocl_device;
    const bool stacked, info;
    bool pre_processing;
    cl_uint clip_t, channel_num, idmn[2];
    cl_platform_id platformID;
    cl_device_id deviceID;
    cl_context context;
    cl_command_queue command_queue;
    cl_program program;
    cl_kernel kernel[NLM_KERNEL];
    cl_mem mem_U[NLM_MEMORY], mem_P[6];
    size_t hrz_result, vrt_result, dst_block[2], hrz_block[2], vrt_block[2];
    bool equals(VideoInfo *v, VideoInfo *w);
    void oclErrorCheck(const char* function, cl_int errcode, IScriptEnvironment *env);
public:
    _NLMAvisynth(PClip _child, const int _d, const int _a, const int _s, const double _h, const char* _channels, const int _wmode,
        const double _wref, PClip _baby, const char* _ocl_device, const int _ocl_id, const int _ocl_x, const int _ocl_y, 
        const int _ocl_r, const bool _lsb, const bool _info, IScriptEnvironment *env);
    ~_NLMAvisynth();
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env);
};
#endif //__AVISYNTH_6_H__

#ifdef VAPOURSYNTH_H
typedef struct _NLMVapoursynth {
public:
    VSNodeRef *node, *knot;
    const VSVideoInfo *vi;
    int64_t d, a, s, wmode, ocl_id, ocl_x, ocl_y, ocl_r, info;
    double wref, h;
    const char *channels, *ocl_device;
    bool pre_processing;
    cl_uint clip_t, channel_num, idmn[2];
    cl_platform_id platformID;
    cl_device_id deviceID;
    cl_context context;
    cl_command_queue command_queue;
    cl_program program;
    cl_kernel kernel[NLM_KERNEL];
    cl_mem mem_U[NLM_MEMORY], mem_P[3];
    size_t hrz_result, vrt_result, dst_block[2], hrz_block[2], vrt_block[2];
    bool equals(const VSVideoInfo *v, const VSVideoInfo *w);
    void oclErrorCheck(const char* function, cl_int errcode, VSMap *out, const VSAPI *vsapi);
} NLMVapoursynth;
#endif //__VAPOURSYNTH_H__
