/*
*    This file is part of KNLMeansCL,
*    Copyright(C) 2015-2020  Edoardo Brunetti.
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

#ifndef NLM_AVISYNTH_H
#define NLM_AVISYNTH_H

#include "NLMKernel.h"
#include "shared/ocl_utils.h"

#ifdef _WIN32
#    include <avisynth.h>
#else
#    include <avisynth/avisynth.h>
#endif

#ifdef __AVISYNTH_8_H__
struct NLMAvisynth : public GenericVideoFilter {

private:
    bool has_at_least_v8; // frame property support
    const int d, a, s, wmode, ocl_id, ocl_x, ocl_y, ocl_r;
    const double wref, h;
    const int mode_9_to_15bits; // 0: UNORM in/out; 1:UNSIGNED in/out; 2:UNORM in UNSIGNED out
    PClip baby;
    const char *channels, *ocl_device;
    const bool stacked, info;
    bool pre_processing;
    bool use_mem_P_out;
    cl_uint clip_t, channel_num, idmn[2];
    cl_platform_id platformID;
    cl_device_id deviceID;
    cl_context context;
    cl_command_queue command_queue;
    cl_program program;
    cl_kernel kernel[NLM_KERNEL];
    cl_mem mem_U[NLM_MEMORY], mem_P[6], mem_P_out[3];
    size_t hrz_result, vrt_result, dst_block[2], hrz_block[2], vrt_block[2];
    bool equals(VideoInfo *v, VideoInfo *w);
    void oclErrorCheck(const char* function, cl_int errcode, IScriptEnvironment *env);

public:
    NLMAvisynth(PClip _child, const int _d, const int _a, const int _s, const double _h,
        const char* _channels, const int _wmode, const double _wref, PClip _baby,
        const char* _ocl_device, const int _ocl_id, const int _ocl_x, const int _ocl_y,
        const int _ocl_r, const bool _lsb, const bool _info, const int _mode_9_to_15bits, IScriptEnvironment *env);
    ~NLMAvisynth();
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env);
    int __stdcall SetCacheHints(int cachehints, int frame_range)
    {
      return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
    }

};
#endif //__AVISYNTH_8_H__

#endif //__NLM_AVISYNTH_H__
