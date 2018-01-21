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

#ifndef NLM_VAPOURSYNTH_H
#define NLM_VAPOURSYNTH_H

#include "NLMKernel.h"

#include <VapourSynth.h>
#include <VSHelper.h>

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

#endif //__NLM_VAPOURSYNTH_H__