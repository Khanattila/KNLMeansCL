/*
*    This file is part of KNLMeansCL,
*    Copyright(C) 2015-2018  Edoardo Brunetti.
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

#ifndef KNLMEANSCL_H
#define KNLMEANSCL_H

#define VERSION           "1.1.0"

#define DFT_d             1
#define DFT_a             2
#define DFT_s             4
#define DFT_h             1.2f
#define DFT_channels      "AUTO"
#define DFT_wmode         0
#define DFT_wref          1.0f
#define DFT_ocl_device    "AUTO"
#define DFT_ocl_id        0
#define DFT_ocl_x         0
#define DFT_ocl_y         0
#define DFT_ocl_r         0
#define DFT_lsb           false
#define DFT_info          false 

#define CL_USE_DEPRECATED_OPENCL_2_0_APIS

#include "NLMKernel.h"

#ifdef _WIN32
#    include <avisynth.h>
#endif

#include <VapourSynth.h>
#include <VSHelper.h>

#ifdef __AVISYNTH_6_H__
struct NLMAvisynth : public GenericVideoFilter {
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
    NLMAvisynth(PClip _child, const int _d, const int _a, const int _s, const double _h, const char* _channels, const int _wmode,
        const double _wref, PClip _baby, const char* _ocl_device, const int _ocl_id, const int _ocl_x, const int _ocl_y, 
        const int _ocl_r, const bool _lsb, const bool _info, IScriptEnvironment *env);
    ~NLMAvisynth();
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env);
};
#endif //__AVISYNTH_6_H__

#ifdef VAPOURSYNTH_H
typedef struct NLMVapoursynth {
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

#endif //__KNLMEANSCL_H__