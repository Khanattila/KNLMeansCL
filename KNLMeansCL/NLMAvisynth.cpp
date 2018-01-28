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

#include "NLMAvisynth.h"
#include "NLMDefault.h"
#include "shared/common.h"
#include "shared/startchar.h"
#include "shared/ocl_utils.h"

#include <cinttypes>
#include <clocale>
#include <cstdio>

#ifdef _MSC_VER
#    define strcasecmp _stricmp
#endif

#ifdef __AVISYNTH_6_H__

//////////////////////////////////////////
// AviSynthFunctions
inline bool NLMAvisynth::equals(VideoInfo *v, VideoInfo *w) {
    return 
        v->width == w->width && 
        v->height == w->height && 
        v->fps_numerator == w->fps_numerator &&
        v->fps_denominator == w->fps_denominator && 
        v->num_frames == w->num_frames &&
        v->pixel_type == w->pixel_type &&
        v->sample_type == w->sample_type;
}

inline void NLMAvisynth::oclErrorCheck(const char* function, cl_int errcode, IScriptEnvironment *env) {
    switch (errcode) {
        case CL_SUCCESS:
            break;
        case OCL_UTILS_NO_DEVICE_AVAILABLE:
            env->ThrowError("KNLMeansCL: no compatible opencl platforms available!");
            break;
        case CL_IMAGE_FORMAT_NOT_SUPPORTED:
            env->ThrowError("KNLMeansCL: the opencl device does not support this video format!");
            break;
        default:
            char buffer[2048];
            snprintf(buffer, 2048, "KNLMeansCL: fatal error!\n (%s: %s)", function, oclUtilsErrorToString(errcode));
            env->ThrowError(buffer);
            break;
    }
}

//////////////////////////////////////////
// AviSynthInit
NLMAvisynth::NLMAvisynth(PClip _child, const int _d, const int _a, const int _s, const double _h, const char* _channels,
    const int _wmode, const double _wref, PClip _baby, const char* _ocl_device, const int _ocl_id, const int _ocl_x,
    const int _ocl_y, const int _ocl_r, const bool _stacked, const bool _info, IScriptEnvironment *env) : 
    GenericVideoFilter(_child), d(_d), a(_a), s(_s), h(_h), channels(_channels), wmode(_wmode), wref(_wref), baby(_baby), 
    ocl_device(_ocl_device), ocl_id(_ocl_id), ocl_x(_ocl_x), ocl_y(_ocl_y), ocl_r(_ocl_r), stacked(_stacked), info(_info) {

    // Check AviSynth Version
    env->CheckVersion(5);
    child->SetCacheHints(CACHE_WINDOW, d);

    // Check source clip
    if (vi.width > 8192 || vi.height > 8192)
        env->ThrowError("KNLMeansCL: 8192x8192 is the highest resolution supported!");

    // Check rclip
    if (baby) {
        VideoInfo rvi = baby->GetVideoInfo();
        if (!equals(&vi, &rvi)) env->ThrowError("KNLMeansCL: 'rclip' does not match the source clip!");
        baby->SetCacheHints(CACHE_WINDOW, d);
        clip_t = NLM_CLIP_EXTRA_TRUE;
    } else {
        clip_t = NLM_CLIP_EXTRA_FALSE;
    }

    // Checks user value
    if (d < 0)
        env->ThrowError("KNLMeansCL: 'd' must be greater than or equal to 0!");
    if (a < 1)
        env->ThrowError("KNLMeansCL: 'a' must be greater than or equal to 1!");
    if (s < 0 || s > 8)
        env->ThrowError("KNLMeansCL: 's' must be in range [0, 8]!");
    if (h <= 0.0f)
        env->ThrowError("KNLMeansCL: 'h' must be greater than 0!");
    if (vi.IsY8() && strcasecmp(channels, "Y") && strcasecmp(channels, "auto"))
        env->ThrowError("KNLMeansCL: 'channels' must be 'Y' with Y8 pixel format!");
    else if (vi.IsPlanar() && vi.IsYUV() && strcasecmp(channels, "YUV") && strcasecmp(channels, "Y") &&
        strcasecmp(channels, "UV") && strcasecmp(channels, "auto"))
        env->ThrowError("KNLMeansCL: 'channels' must be 'YUV', 'Y' or 'UV' with YUV color space!");
    else if (!vi.Is444() && !strcasecmp(channels, "YUV"))
        env->ThrowError("KNLMeansCL: 'channels = YUV' require a YV24 pixel format!");
    else if (vi.IsRGB() && strcasecmp(channels, "RGB") && strcasecmp(channels, "auto"))
        env->ThrowError("KNLMeansCL: 'channels' must be 'RGB' with RGB color space!");
    if (wmode < 0 || wmode > 3)
        env->ThrowError("KNLMeansCL: 'wmode' must be in range [0, 3]!");
    if (wref < 0.0f)
        env->ThrowError("KNLMeansCL: 'wref' must be greater than or equal to 0!");
    cl_uint ocl_device_type = 0;
    if (!strcasecmp(ocl_device, "CPU"))
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_CPU;
    else if (!strcasecmp(ocl_device, "GPU"))
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_GPU;
    else if (!strcasecmp(ocl_device, "ACCELERATOR"))
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_ACCELERATOR;
    else if (!strcasecmp(ocl_device, "AUTO"))
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_AUTO;
    else
        env->ThrowError("KNLMeansCL: 'device_type' must be 'cpu', 'gpu', 'accelerator' or 'auto'!");
    if (ocl_id < 0)
        env->ThrowError("KNLMeansCL: 'device_id' must be greater than or equal to 0!");
    if (ocl_x < 0 || ocl_y < 0 || ocl_r < 0) 
        env->ThrowError("KNLMeansCL: 'ocl_x', 'ocl_y' and 'ocl_r' must be greater than 0!");
    else if (!(ocl_x == 0 && ocl_y == 0 && ocl_r == 0) && !(ocl_x > 0 && ocl_y > 0 && ocl_r > 0)) 
        env->ThrowError("KNLMeansCL: 'ocl_x', 'ocl_y' and 'ocl_r' must be set!");   
    if ((info && vi.IsRGB()) || (info && vi.BitsPerComponent() != 8))
        env->ThrowError("KNLMeansCL: 'info' requires Gray8 or YUVP8 color space!");

    // Set image dimensions
    if (!strcasecmp(channels, "UV")) {
        int subSamplingW = vi.IsYV24() ? 0 : 1;
        int subSamplingH = stacked ? (vi.IsYV12() ? 2 : 1) : (vi.IsYV12() ? 1 : 0);
        idmn[0] = (cl_uint) vi.width >> subSamplingW;
        idmn[1] = (cl_uint) vi.height >> subSamplingH;
    } else {
        idmn[0] = (cl_uint) vi.width;
        idmn[1] = (cl_uint) (stacked ? (vi.height >> 1) : vi.height);
    }

    // Set pre_processing, clip_t, channel_order and channel_num
    pre_processing = false;
    cl_channel_order channel_order;
    if (!strcasecmp(channels, "YUV")) {
        pre_processing = true;
        clip_t |= NLM_CLIP_REF_YUV;
        channel_order = CL_RGBA;
        channel_num = 4; // 3 + buffer
    } else if (!strcasecmp(channels, "Y")) {
        clip_t |= NLM_CLIP_REF_LUMA;
        channel_order = CL_R;
        channel_num = 2; // 1 + buffer
    } else if (!strcasecmp(channels, "UV")) {
        pre_processing = true;
        clip_t |= NLM_CLIP_REF_CHROMA;
        channel_order = CL_RG;
        channel_num = 3; // 2 + buffer
    } else if (!strcasecmp(channels, "RGB")) {
        clip_t |= NLM_CLIP_REF_RGB;
        channel_order = CL_RGBA;
        channel_num = 4; // 3 + buffer
    } else {
        if (vi.IsPlanar()) {
            clip_t |= NLM_CLIP_REF_LUMA;
            channel_order = CL_R;
            channel_num = 2; // 1 + buffer
        } else {
            clip_t |= NLM_CLIP_REF_RGB;
            channel_order = CL_RGBA;
            channel_num = 4; // 3 + buffer
        }
    }

    // Set channel_type
    cl_channel_type channel_type_u = NULL, channel_type_p = NULL;
    if (vi.IsPlanar() && vi.IsYUV() || vi.IsRGB32() || vi.IsRGB64()) {
        if (vi.BitsPerComponent() == 8) {
            if (stacked) {
                pre_processing = true;
                clip_t |= NLM_CLIP_TYPE_STACKED;
                channel_type_u = CL_UNORM_INT16;
                channel_type_p = CL_UNSIGNED_INT8;
            } else {
                clip_t |= NLM_CLIP_TYPE_UNORM;
                channel_type_u = channel_type_p = CL_UNORM_INT8;
            }
        } else if (vi.BitsPerComponent() == 10) {
            if (stacked) {
                env->ThrowError("KNLMeansCL: INT20 are not supported!");
            } else if (strcasecmp(channels, "YUV")) {
                env->ThrowError("KNLMeansCL: INT10 require 'channels = YUV'!");
            } else if (vi.Is444()) {
                clip_t |= NLM_CLIP_TYPE_UNSIGNED;
                channel_order = CL_RGB;
                channel_type_u = CL_UNORM_INT_101010;
                channel_type_p = CL_UNSIGNED_INT16;
            } else {
                env->ThrowError("KNLMeansCL: INT10 require no chroma subsampling!");
            }
        } else if (vi.BitsPerComponent() == 16) {
            if (stacked) {
                env->ThrowError("KNLMeansCL: INT32 are not supported!");
            } else {
                clip_t |= NLM_CLIP_TYPE_UNORM;
                channel_type_u = channel_type_p = CL_UNORM_INT16;
            }
        } else if (vi.BitsPerComponent() == 32) {
            if (stacked) {
                env->ThrowError("KNLMeansCL: DOUBLE are not supported!");
            } else {
                clip_t |= NLM_CLIP_TYPE_UNORM;
                channel_type_u = channel_type_p = CL_FLOAT;
            }
        } else {
            env->ThrowError("KNLMeansCL: P8, P10, P16 and Single are supported!");
        }
    } else {
        env->ThrowError("KNLMeansCL: planar YUV, RGB32 and RGB64 are supported!");
    }

    // Get platformID and deviceID
    cl_int ret = oclUtilsGetPlaformDeviceIDs(ocl_device_type, (cl_uint) ocl_id, &platformID, &deviceID);
    oclErrorCheck("oclUtilsGetPlaformDeviceIDs", ret, env);

    // Get maximum number of work-items
    if (ocl_x > 0 || ocl_y > 0 || ocl_r > 0) {
        hrz_block[0] = vrt_block[0] = (size_t) ocl_x;
        hrz_block[1] = vrt_block[1] = (size_t) ocl_y;
        hrz_result = vrt_result = (size_t) ocl_r;
    } else {
        size_t max_work_group_size;
        ret = clGetDeviceInfo(deviceID, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &max_work_group_size, NULL);
        oclErrorCheck("clGetDeviceInfo", ret, env);
        switch (max_work_group_size) {
            case 256: // AMD
                hrz_block[0] = vrt_block[0] = 32;
                hrz_block[1] = vrt_block[1] = 8;
                hrz_result = vrt_result = 1;
                break;
            case 512: // INTEL GPU
                hrz_block[0] = vrt_block[0] = 32;
                hrz_block[1] = vrt_block[1] = 8;
                hrz_result = vrt_result = 3;
                break;
            case 1024: // NVIDIA
                hrz_block[0] = vrt_block[0] = 16;
                hrz_block[1] = vrt_block[1] = 8;
                hrz_result = vrt_result = 3;
                break;
            case 8192: // INTEL CPU
                hrz_block[0] = vrt_block[0] = 64;
                hrz_block[1] = vrt_block[1] = 8;
                hrz_result = vrt_result = 5;
                break;
            default:
                hrz_block[0] = vrt_block[0] = 8;
                hrz_block[1] = vrt_block[1] = 8;
                hrz_result = vrt_result = 1;
                break;
        }
    }

    // Create an OpenCL context
    context = clCreateContext(NULL, 1, &deviceID, NULL, NULL, &ret);
    oclErrorCheck("clCreateContext", ret, env);

    // Create a command queue
    command_queue = clCreateCommandQueue(context, deviceID, 0, &ret);
    oclErrorCheck("clCreateCommandQueue", ret, env);

    // Create mem_U[]
    size_t size_u2 = sizeof(cl_float) * idmn[0] * idmn[1] * channel_num;
    size_t size_u5 = sizeof(cl_float) * idmn[0] * idmn[1];
    cl_mem_flags flags_u1ab, flags_u1z;
    if (!(clip_t & NLM_CLIP_REF_CHROMA) && !(clip_t & NLM_CLIP_REF_YUV) && !stacked) {
        flags_u1ab = CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY;
        flags_u1z = CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY;
    } else {
        flags_u1ab = flags_u1z = CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS;
    }
    const cl_image_format format_u1 = { channel_order, channel_type_u };
    const cl_image_format format_u4 = { CL_R, CL_FLOAT };
    size_t array_size = (size_t) (2 * d + 1);
    const cl_image_desc desc_u = { CL_MEM_OBJECT_IMAGE2D_ARRAY, idmn[0], idmn[1], 1, array_size, 0, 0, 0, 0, NULL };
    const cl_image_desc desc_u1z = { CL_MEM_OBJECT_IMAGE2D, idmn[0], idmn[1], 1, 1, 0, 0, 0, 0, NULL };
    mem_U[memU1a] = clCreateImage(context, flags_u1ab, &format_u1, &desc_u, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_U[NLM_MEM_U1a])", ret, env);
    if (baby) {
        mem_U[memU1b] = clCreateImage(context, flags_u1ab, &format_u1, &desc_u, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_U[NLM_MEM_U1b])", ret, env);
    }
    mem_U[memU1z] = clCreateImage(context, flags_u1z, &format_u1, &desc_u1z, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_U[NLM_MEM_U1z])", ret, env);
    mem_U[memU2] = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u2, NULL, &ret);
    oclErrorCheck("clCreateBuffer(mem_U[NLM_MEM_U2])", ret, env);
    mem_U[memU4a] = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &format_u4, &desc_u, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_U[NLM_MEM_U4a])", ret, env);
    mem_U[memU4b] = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &format_u4, &desc_u, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_U[NLM_MEM_U4b])", ret, env);
    mem_U[memU5] = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u5, NULL, &ret);
    oclErrorCheck("clCreateBuffer(mem_U[NLM_MEM_U0a])", ret, env);

    // Create mem_P[]
    if (pre_processing) {
        const cl_image_format format_p = { CL_R, channel_type_p };
        const cl_image_desc desc_p = { CL_MEM_OBJECT_IMAGE2D, idmn[0], idmn[1], 1, 1, 0, 0, 0, 0, NULL };
        mem_P[0] = clCreateImage(context, CL_MEM_READ_WRITE, &format_p, &desc_p, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_P[0])", ret, env);
        mem_P[1] = clCreateImage(context, CL_MEM_READ_WRITE, &format_p, &desc_p, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_P[1])", ret, env);
        mem_P[2] = clCreateImage(context, CL_MEM_READ_WRITE, &format_p, &desc_p, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_P[2])", ret, env);
        mem_P[3] = clCreateImage(context, CL_MEM_READ_WRITE, &format_p, &desc_p, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_P[3])", ret, env);
        mem_P[4] = clCreateImage(context, CL_MEM_READ_WRITE, &format_p, &desc_p, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_P[4])", ret, env);
        mem_P[5] = clCreateImage(context, CL_MEM_READ_WRITE, &format_p, &desc_p, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_P[5])", ret, env);
    }

    // Creates and Build a program executable from the program source
    program = clCreateProgramWithSource(context, 1, &kernel_source_code, NULL, &ret);
    oclErrorCheck("clCreateProgramWithSource", ret, env);
    char options[2048];
    setlocale(LC_ALL, "C");
    snprintf(options, 2048, "-cl-single-precision-constant -cl-denorms-are-zero -cl-fast-relaxed-math -Werror \
        -D %s -D %s -D %s -D VI_DIM_X=%u -D VI_DIM_Y=%u -D HRZ_RESULT=%zu -D VRT_RESULT=%zu \
        -D HRZ_BLOCK_X=%zu -D HRZ_BLOCK_Y=%zu -D VRT_BLOCK_X=%zu -D VRT_BLOCK_Y=%zu \
        -D NLM_D=%i -D NLM_S=%i -D NLM_H=%f -D NLM_WREF=%f",
        nlmClipTypeToString(clip_t), nlmClipRefToString(clip_t), nlmWmodeToString(wmode),
        idmn[0], idmn[1], hrz_result, vrt_result,
        hrz_block[0], hrz_block[1], vrt_block[0], vrt_block[1],
        d, s, h, wref);
    ret = clBuildProgram(program, 1, &deviceID, options, NULL, NULL);
    if (ret != CL_SUCCESS) {
        oclUtilsDebugInfo(platformID, deviceID, program, ret);
        env->ThrowError("KNLMeansCL: build program error!\n Please report Log-KNLMeansCL.txt.");
    }
    setlocale(LC_ALL, "");

    // Create kernel objects
    kernel[nlmDistance] = clCreateKernel(program, "nlmDistance", &ret);
    oclErrorCheck("clCreateKernel(nlmDistance)", ret, env);
    kernel[nlmHorizontal] = clCreateKernel(program, "nlmHorizontal", &ret);
    oclErrorCheck("clCreateKernel(nlmHorizontal)", ret, env);
    kernel[nlmVertical] = clCreateKernel(program, "nlmVertical", &ret);
    oclErrorCheck("clCreateKernel(nlmVertical)", ret, env);
    kernel[nlmAccumulation] = clCreateKernel(program, "nlmAccumulation", &ret);
    oclErrorCheck("clCreateKernel(nlmAccumulation)", ret, env);
    kernel[nlmFinish] = clCreateKernel(program, "nlmFinish", &ret);
    oclErrorCheck("clCreateKernel(nlmFinish)", ret, env);
    kernel[nlmPack] = clCreateKernel(program, "nlmPack", &ret);
    oclErrorCheck("clCreateKernel(nlmPack)", ret, env);
    kernel[nlmUnpack] = clCreateKernel(program, "nlmUnpack", &ret);
    oclErrorCheck("clCreateKernel(nlmUnpack)", ret, env);

    // Get some kernel info
    size_t dst_work_group;
    ret = clGetKernelWorkGroupInfo(kernel[nlmDistance], deviceID,
        CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &dst_work_group, NULL);
    oclErrorCheck("clGetKernelWorkGroupInfo(nlmDistance)", ret, env);
    if (dst_work_group >= 1024) 
        dst_block[0] = dst_block[1] = 32;
    else if (dst_work_group >= 256) 
        dst_block[0] = dst_block[1] = 16;
    else if (dst_work_group >= 64) 
        dst_block[0] = dst_block[1] = 8;
    else 
        dst_block[0] = dst_block[1] = 1;

    // Set kernel arguments - nlmDistance
    int index_u1 = (baby) ? memU1b : memU1a;
    ret = clSetKernelArg(kernel[nlmDistance], 0, sizeof(cl_mem), &mem_U[index_u1]);
    oclErrorCheck("clSetKernelArg(nlmDistance[0])", ret, env);
    ret = clSetKernelArg(kernel[nlmDistance], 1, sizeof(cl_mem), &mem_U[memU4a]);
    oclErrorCheck("clSetKernelArg(nlmDistance[1])", ret, env);
    // kernel[nlmDistance] -> 2 is set by AviSynthPluginGetFrame
    // kernel[nlmDistance] -> 3 is set by AviSynthPluginGetFrame

    // nlmHorizontal
    ret = clSetKernelArg(kernel[nlmHorizontal], 0, sizeof(cl_mem), &mem_U[memU4a]);
    oclErrorCheck("clSetKernelArg(nlmHorizontal[0])", ret, env);
    ret = clSetKernelArg(kernel[nlmHorizontal], 1, sizeof(cl_mem), &mem_U[memU4b]);
    oclErrorCheck("clSetKernelArg(nlmHorizontal[1])", ret, env);
    // kernel[nlmHorizontal] -> 2 is set by AviSynthPluginGetFrame

    // nlmVertical
    ret = clSetKernelArg(kernel[nlmVertical], 0, sizeof(cl_mem), &mem_U[memU4b]);
    oclErrorCheck("clSetKernelArg(nlmVertical[0])", ret, env);
    ret = clSetKernelArg(kernel[nlmVertical], 1, sizeof(cl_mem), &mem_U[memU4a]);
    oclErrorCheck("clSetKernelArg(nlmVertical[1])", ret, env);
    // kernel[nlmVertical] -> 2 is set by AviSynthPluginGetFrame

    // nlmAccumulation
    ret = clSetKernelArg(kernel[nlmAccumulation], 0, sizeof(cl_mem), &mem_U[memU1a]);
    oclErrorCheck("clSetKernelArg(nlmAccumulation[0])", ret, env);
    ret = clSetKernelArg(kernel[nlmAccumulation], 1, sizeof(cl_mem), &mem_U[memU2]);
    oclErrorCheck("clSetKernelArg(nlmAccumulation[1])", ret, env);
    ret = clSetKernelArg(kernel[nlmAccumulation], 2, sizeof(cl_mem), &mem_U[memU4a]);
    oclErrorCheck("clSetKernelArg(nlmAccumulation[2])", ret, env);
    ret = clSetKernelArg(kernel[nlmAccumulation], 3, sizeof(cl_mem), &mem_U[memU5]);
    oclErrorCheck("clSetKernelArg(nlmAccumulation[3])", ret, env);
    // kernel[nlmAccumulation] -> 4 is set by AviSynthPluginGetFrame

    // nlmFinish
    ret = clSetKernelArg(kernel[nlmFinish], 0, sizeof(cl_mem), &mem_U[memU1a]);
    oclErrorCheck("clSetKernelArg(nlmFinish[0])", ret, env);
    ret = clSetKernelArg(kernel[nlmFinish], 1, sizeof(cl_mem), &mem_U[memU1z]);
    oclErrorCheck("clSetKernelArg(nlmFinish[1])", ret, env);
    ret = clSetKernelArg(kernel[nlmFinish], 2, sizeof(cl_mem), &mem_U[memU2]);
    oclErrorCheck("clSetKernelArg(nlmFinish[2])", ret, env);
    ret = clSetKernelArg(kernel[nlmFinish], 3, sizeof(cl_mem), &mem_U[memU5]);
    oclErrorCheck("clSetKernelArg(nlmFinish[3])", ret, env);

    // nlmPack
    if (pre_processing) {
        ret = clSetKernelArg(kernel[nlmPack], 0, sizeof(cl_mem), &mem_P[0]);
        oclErrorCheck("clSetKernelArg(nlmPack[0])", ret, env);
        ret = clSetKernelArg(kernel[nlmPack], 1, sizeof(cl_mem), &mem_P[1]);
        oclErrorCheck("clSetKernelArg(nlmPack[1])", ret, env);
        ret = clSetKernelArg(kernel[nlmPack], 2, sizeof(cl_mem), &mem_P[2]);
        oclErrorCheck("clSetKernelArg(nlmPack[2])", ret, env);
        ret = clSetKernelArg(kernel[nlmPack], 3, sizeof(cl_mem), &mem_P[3]);
        oclErrorCheck("clSetKernelArg(nlmPack[3])", ret, env);
        ret = clSetKernelArg(kernel[nlmPack], 4, sizeof(cl_mem), &mem_P[4]);
        oclErrorCheck("clSetKernelArg(nlmPack[4])", ret, env);
        ret = clSetKernelArg(kernel[nlmPack], 5, sizeof(cl_mem), &mem_P[5]);
        oclErrorCheck("clSetKernelArg(nlmPack[5])", ret, env);
        // kernel[nlmPack] -> 6 is set by AviSynthPluginGetFrame 
        // kernel[nlmPack] -> 7 is set by AviSynthPluginGetFrame 
    }

    // nlmUnpack
    if (pre_processing) {
        ret = clSetKernelArg(kernel[nlmUnpack], 0, sizeof(cl_mem), &mem_P[0]);
        oclErrorCheck("clSetKernelArg(nlmUnpack[0])", ret, env);
        ret = clSetKernelArg(kernel[nlmUnpack], 1, sizeof(cl_mem), &mem_P[1]);
        oclErrorCheck("clSetKernelArg(nlmUnpack[1])", ret, env);
        ret = clSetKernelArg(kernel[nlmUnpack], 2, sizeof(cl_mem), &mem_P[2]);
        oclErrorCheck("clSetKernelArg(nlmUnpack[2])", ret, env);
        ret = clSetKernelArg(kernel[nlmUnpack], 3, sizeof(cl_mem), &mem_P[3]);
        oclErrorCheck("clSetKernelArg(nlmUnpack[3])", ret, env);
        ret = clSetKernelArg(kernel[nlmUnpack], 4, sizeof(cl_mem), &mem_P[4]);
        oclErrorCheck("clSetKernelArg(nlmUnpack[4])", ret, env);
        ret = clSetKernelArg(kernel[nlmUnpack], 5, sizeof(cl_mem), &mem_P[5]);
        oclErrorCheck("clSetKernelArg(nlmUnpack[5])", ret, env);
        ret = clSetKernelArg(kernel[nlmUnpack], 6, sizeof(cl_mem), &mem_U[memU1z]);
        oclErrorCheck("clSetKernelArg(nlmUnpack[6])", ret, env);
    }

}

//////////////////////////////////////////
// AviSynthGetFrame
PVideoFrame __stdcall NLMAvisynth::GetFrame(int n, IScriptEnvironment* env) {
    // Variables
    PVideoFrame src, ref;
    PVideoFrame dst = env->NewVideoFrame(vi);
    int k_start = -min(d, n);
    int k_end = min(d, vi.num_frames - 1 - n);
    int spt_side = 2 * a + 1;
    int spt_area = spt_side * spt_side;
    size_t size_u2 = sizeof(cl_float) * idmn[0] * idmn[1] * channel_num;
    size_t size_u5 = sizeof(cl_float) * idmn[0] * idmn[1];
    const cl_int t = d;
    const cl_float pattern_u2 = 0.0f;
    const cl_float pattern_u5 = CL_FLT_EPSILON;
    const size_t origin[3] = { 0, 0, 0 };
    const size_t region[3] = { idmn[0], idmn[1], 1 };
    const size_t global_work[2] = { mrounds(idmn[0], 16), mrounds(idmn[1], 8) };
    const size_t global_work_dst[2] = {
        mrounds(idmn[0], dst_block[0]),
        mrounds(idmn[1], dst_block[1])
    };
    const size_t global_work_hrz[2] = {
        mrounds(idmn[0], hrz_result * hrz_block[0]) / hrz_result,
        mrounds(idmn[1], hrz_block[1])
    };
    const size_t global_work_vrt[2] = {
        mrounds(idmn[0], vrt_block[0]),
        mrounds(idmn[1], vrt_result * vrt_block[1]) / vrt_result
    };

    // Set-up buffers   
    cl_int ret = CL_SUCCESS;
    ret |= clEnqueueFillBuffer(command_queue, mem_U[memU2], &pattern_u2, sizeof(cl_float), 0, size_u2, 0, NULL, NULL);
    ret |= clEnqueueFillBuffer(command_queue, mem_U[memU5], &pattern_u5, sizeof(cl_float), 0, size_u5, 0, NULL, NULL);

    // Write image
    for (int k = k_start; k <= k_end; k++) {
        src = child->GetFrame(n + k, env);
        ref = (baby) ? baby->GetFrame(n + k, env) : nullptr;
        const cl_int t_pk = t + k;
        const size_t origin_in[3] = { 0, 0, (size_t) t_pk };
        switch (clip_t) {
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                ret |= clEnqueueWriteImage(command_queue, mem_U[memU1a], CL_FALSE, origin_in, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                ret |= clEnqueueWriteImage(command_queue, mem_U[memU1a], CL_FALSE, origin_in, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_U[memU1b], CL_FALSE, origin_in, region,
                    (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_LUMA):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_Y),
                    0, src->GetReadPtr(PLANAR_Y) + src->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_U[memU1a]);
                ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_LUMA):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_Y),
                    0, src->GetReadPtr(PLANAR_Y) + src->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_U[memU1a]);
                ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) ref->GetPitch(PLANAR_Y),
                    0, ref->GetReadPtr(PLANAR_Y) + ref->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_U[memU1b]);
                ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_U[memU1a]);
                ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_U[memU1a]);
                ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_U), 0, ref->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_V), 0, ref->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_U[memU1b]);
                ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_CHROMA):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_U),
                    0, src->GetReadPtr(PLANAR_U) + src->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[4], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_V),
                    0, src->GetReadPtr(PLANAR_V) + src->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_U[memU1a]);
                ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_CHROMA):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_U),
                    0, src->GetReadPtr(PLANAR_U) + src->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[4], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_V),
                    0, src->GetReadPtr(PLANAR_V) + src->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_U[memU1a]);
                ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_U), 0, ref->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_V), 0, ref->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) ref->GetPitch(PLANAR_U),
                    0, ref->GetReadPtr(PLANAR_U) + ref->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[4], CL_FALSE, origin, region, (size_t) ref->GetPitch(PLANAR_V),
                    0, ref->GetReadPtr(PLANAR_V) + ref->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_U[memU1b]);
                ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_U[memU1a]);
                ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_U[memU1a]);
                ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_U), 0, ref->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_FALSE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_V), 0, ref->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_U[memU1b]);
                ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_YUV):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_Y),
                    0, src->GetReadPtr(PLANAR_Y) + src->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[4], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_U),
                    0, src->GetReadPtr(PLANAR_U) + src->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[5], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_V),
                    0, src->GetReadPtr(PLANAR_V) + src->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_U[memU1a]);
                ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_YUV):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_FALSE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_Y),
                    0, src->GetReadPtr(PLANAR_Y) + src->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[4], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_U),
                    0, src->GetReadPtr(PLANAR_U) + src->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[5], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_V),
                    0, src->GetReadPtr(PLANAR_V) + src->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_U[memU1a]);
                ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_U), 0, ref->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_FALSE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_V), 0, ref->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) ref->GetPitch(PLANAR_Y),
                    0, ref->GetReadPtr(PLANAR_Y) + ref->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[4], CL_FALSE, origin, region, (size_t) ref->GetPitch(PLANAR_U),
                    0, ref->GetReadPtr(PLANAR_U) + ref->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[5], CL_FALSE, origin, region, (size_t) ref->GetPitch(PLANAR_V),
                    0, ref->GetReadPtr(PLANAR_V) + ref->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_U[memU1b]);
                ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                ret |= clEnqueueWriteImage(command_queue, mem_U[memU1a], CL_FALSE, origin_in, region,
                    (size_t) src->GetPitch(), 0, src->GetReadPtr(), 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                ret |= clEnqueueWriteImage(command_queue, mem_U[memU1a], CL_FALSE, origin_in, region,
                    (size_t) src->GetPitch(), 0, src->GetReadPtr(), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_U[memU1b], CL_FALSE, origin_in, region,
                    (size_t) ref->GetPitch(), 0, ref->GetReadPtr(), 0, NULL, NULL);
                break;
            default:
                env->ThrowError("KNLMeansCL: clip_t error!\n (AviSynthGetFrame)");
                break;
        }
    }

    // Spatio-temporal processing
    for (int k = k_start; k <= 0; k++) {
        for (int j = -a; j <= a; j++) {
            for (int i = -a; i <= a; i++) {
                if (k * spt_area + j * spt_side + i < 0) {
                    const cl_int q[4] = { i, j, k, 0 };
                    ret |= clSetKernelArg(kernel[nlmDistance], 2, sizeof(cl_int), &t);
                    ret |= clSetKernelArg(kernel[nlmDistance], 3, 4 * sizeof(cl_int), &q);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmDistance],
                        2, NULL, global_work_dst, dst_block, 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmHorizontal], 2, sizeof(cl_int), &t);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmHorizontal],
                        2, NULL, global_work_hrz, hrz_block, 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmVertical], 2, sizeof(cl_int), &t);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmVertical],
                        2, NULL, global_work_vrt, vrt_block, 0, NULL, NULL);
                    if (k) {
                        const cl_int t_mq = t - k;
                        ret |= clSetKernelArg(kernel[nlmDistance], 2, sizeof(cl_int), &t_mq);
                        ret |= clSetKernelArg(kernel[nlmDistance], 3, 4 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmDistance],
                            2, NULL, global_work_dst, dst_block, 0, NULL, NULL);
                        ret |= clSetKernelArg(kernel[nlmHorizontal], 2, sizeof(cl_int), &t_mq);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmHorizontal],
                            2, NULL, global_work_hrz, hrz_block, 0, NULL, NULL);
                        ret |= clSetKernelArg(kernel[nlmVertical], 2, sizeof(cl_int), &t_mq);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmVertical],
                            2, NULL, global_work_vrt, vrt_block, 0, NULL, NULL);
                    }
                    ret |= clSetKernelArg(kernel[nlmAccumulation], 4, 4 * sizeof(cl_int), &q);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmAccumulation],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                }
            }
        }
    }
    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmFinish], 2, NULL, global_work, NULL, 0, NULL, NULL);

    // Read image
    switch (clip_t) {
        case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
        case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
            ret |= clEnqueueReadImage(command_queue, mem_U[memU1z], CL_FALSE, origin, region,
                (size_t) dst->GetPitch(PLANAR_Y), 0, dst->GetWritePtr(PLANAR_Y), 0, NULL, NULL);
            break;
        case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_LUMA):
        case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_LUMA):
            ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmUnpack],
                2, NULL, global_work, NULL, 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                (size_t) dst->GetPitch(PLANAR_Y), 0, dst->GetWritePtr(PLANAR_Y), 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) dst->GetPitch(PLANAR_Y),
                0, dst->GetWritePtr(PLANAR_Y) + dst->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
            break;
        case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA):
        case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA):
            ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmUnpack],
                2, NULL, global_work, NULL, 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                (size_t) dst->GetPitch(PLANAR_U), 0, dst->GetWritePtr(PLANAR_U), 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                (size_t) dst->GetPitch(PLANAR_V), 0, dst->GetWritePtr(PLANAR_V), 0, NULL, NULL);
            break;
        case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_CHROMA):
        case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_CHROMA):
            ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmUnpack],
                2, NULL, global_work, NULL, 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                (size_t) dst->GetPitch(PLANAR_U), 0, dst->GetWritePtr(PLANAR_U), 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                (size_t) dst->GetPitch(PLANAR_V), 0, dst->GetWritePtr(PLANAR_V), 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) dst->GetPitch(PLANAR_U),
                0, dst->GetWritePtr(PLANAR_U) + dst->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[4], CL_FALSE, origin, region, (size_t) dst->GetPitch(PLANAR_V),
                0, dst->GetWritePtr(PLANAR_V) + dst->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
            break;
        case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
        case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
        case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
        case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
            ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmUnpack],
                2, NULL, global_work, NULL, 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                (size_t) dst->GetPitch(PLANAR_Y), 0, dst->GetWritePtr(PLANAR_Y), 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                (size_t) dst->GetPitch(PLANAR_U), 0, dst->GetWritePtr(PLANAR_U), 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[2], CL_FALSE, origin, region,
                (size_t) dst->GetPitch(PLANAR_V), 0, dst->GetWritePtr(PLANAR_V), 0, NULL, NULL);
            break;
        case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_YUV):
        case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_YUV):
            ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmUnpack],
                2, NULL, global_work, NULL, 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                (size_t) dst->GetPitch(PLANAR_Y), 0, dst->GetWritePtr(PLANAR_Y), 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                (size_t) dst->GetPitch(PLANAR_U), 0, dst->GetWritePtr(PLANAR_U), 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[2], CL_FALSE, origin, region,
                (size_t) dst->GetPitch(PLANAR_V), 0, dst->GetWritePtr(PLANAR_V), 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) dst->GetPitch(PLANAR_Y),
                0, dst->GetWritePtr(PLANAR_Y) + dst->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[4], CL_FALSE, origin, region, (size_t) dst->GetPitch(PLANAR_U),
                0, dst->GetWritePtr(PLANAR_U) + dst->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[5], CL_FALSE, origin, region, (size_t) dst->GetPitch(PLANAR_V),
                0, dst->GetWritePtr(PLANAR_V) + dst->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
            break;
        case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
        case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
            ret |= clEnqueueReadImage(command_queue, mem_U[memU1z], CL_FALSE, origin, region,
                (size_t) dst->GetPitch(), 0, dst->GetWritePtr(), 0, NULL, NULL);
            break;
        default:
            env->ThrowError("KNLMeansCL: clip_t error!\n (AviSynthGetFrame)");
            break;
    }

    // Issues all queued OpenCL commands
    ret |= clFlush(command_queue);

    // Copy other data
    src = child->GetFrame(n, env);
    if (!vi.IsY8() && (clip_t & NLM_CLIP_REF_LUMA)) {
        env->BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), src->GetReadPtr(PLANAR_U),
            src->GetPitch(PLANAR_U), src->GetRowSize(PLANAR_U), src->GetHeight(PLANAR_U));
        env->BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), src->GetReadPtr(PLANAR_V),
            src->GetPitch(PLANAR_V), src->GetRowSize(PLANAR_V), src->GetHeight(PLANAR_V));
    } else if (clip_t & NLM_CLIP_REF_CHROMA) {
        env->BitBlt(dst->GetWritePtr(PLANAR_Y), dst->GetPitch(PLANAR_Y), src->GetReadPtr(PLANAR_Y),
            src->GetPitch(PLANAR_Y), src->GetRowSize(PLANAR_Y), src->GetHeight(PLANAR_Y));
    } else if (clip_t & NLM_CLIP_REF_RGB) {
        const uint8_t* srcp = src->GetReadPtr();
        uint8_t* dstp = dst->GetWritePtr();
        for (int y = 0; y < src->GetHeight(); y++) {
            for (int x = 0; x < src->GetRowSize(); x += 4) {
                *(dstp + x + 3) = *(srcp + x + 3);
            }
        }
        srcp += src->GetPitch();
        dstp += dst->GetPitch();
    }

    // Blocks until all queued OpenCL commands have completed
    ret |= clFinish(command_queue);
    if (ret != CL_SUCCESS) env->ThrowError("KNLMeansCL: fatal error!\n (AviSynthGetFrame)");

    // Info
    if (info) {
        uint8_t y = 0, *frm = dst->GetWritePtr(PLANAR_Y);
        int pitch = dst->GetPitch(PLANAR_Y);
        char buffer[2048], str[2048], str1[2048];
        DrawString(frm, pitch, 0, y++, "KNLMeansCL");
        DrawString(frm, pitch, 0, y++, " Version " VERSION);
        DrawString(frm, pitch, 0, y++, " Copyright(C) Khanattila");
        snprintf(buffer, 2048, " Bits per sample: %i", stacked ? 16 : vi.BitsPerComponent());
        DrawString(frm, pitch, 0, y++, buffer);
        snprintf(buffer, 2048, " Search window: %ix%ix%i", 2 * a + 1, 2 * a + 1, 2 * d + 1);
        DrawString(frm, pitch, 0, y++, buffer);
        snprintf(buffer, 2048, " Similarity neighborhood: %ix%i", 2 * s + 1, 2 * s + 1);
        DrawString(frm, pitch, 0, y++, buffer);
        snprintf(buffer, 2048, " Num of ref pixels: %i", (2 * a + 1)*(2 * a + 1)*(2 * d + 1) - 1);
        DrawString(frm, pitch, 0, y++, buffer);
        snprintf(buffer, 2048, " Local work size: %zux%zu - %zux%zux%zu",
            dst_block[0], dst_block[1], hrz_block[0], hrz_block[1], hrz_result);
        DrawString(frm, pitch, 0, y++, buffer);
        DrawString(frm, pitch, 0, y++, "Platform info");
        ret |= clGetPlatformInfo(platformID, CL_PLATFORM_NAME, sizeof(char) * 2048, str, NULL);
        snprintf(buffer, 2048, " Name: %s", str);
        DrawString(frm, pitch, 0, y++, buffer);
        ret |= clGetPlatformInfo(platformID, CL_PLATFORM_VENDOR, sizeof(char) * 2048, str, NULL);
        snprintf(buffer, 2048, " Vendor: %s", str);
        DrawString(frm, pitch, 0, y++, buffer);
        ret |= clGetPlatformInfo(platformID, CL_PLATFORM_VERSION, sizeof(char) * 2048, str, NULL);
        snprintf(buffer, 2048, " Version: %s", str);
        DrawString(frm, pitch, 0, y++, buffer);
        DrawString(frm, pitch, 0, y++, "Device info");
        ret |= clGetDeviceInfo(deviceID, CL_DEVICE_NAME, sizeof(char) * 2048, str, NULL);
        snprintf(buffer, 2048, " Name: %s", str);
        DrawString(frm, pitch, 0, y++, buffer);
        ret |= clGetDeviceInfo(deviceID, CL_DEVICE_VENDOR, sizeof(char) * 2048, str, NULL);
        snprintf(buffer, 2048, " Vendor: %s", str);
        DrawString(frm, pitch, 0, y++, buffer);
        ret |= clGetDeviceInfo(deviceID, CL_DEVICE_VERSION, sizeof(char) * 2048, str, NULL);
        ret |= clGetDeviceInfo(deviceID, CL_DRIVER_VERSION, sizeof(char) * 2048, str1, NULL);
        snprintf(buffer, 2048, " Version: %s %s", str, str1);
        DrawString(frm, pitch, 0, y++, buffer);
        if (ret != CL_SUCCESS) env->ThrowError("KNLMeansCL: fatal error!\n (AviSynthInfo)");
    }
    return dst;
}

//////////////////////////////////////////
// AviSynthFree
NLMAvisynth::~NLMAvisynth() {
    clReleaseCommandQueue(command_queue);
    if (pre_processing) {
        clReleaseMemObject(mem_P[5]);
        clReleaseMemObject(mem_P[4]);
        clReleaseMemObject(mem_P[3]);
        clReleaseMemObject(mem_P[2]);
        clReleaseMemObject(mem_P[1]);
        clReleaseMemObject(mem_P[0]);
    }
    clReleaseMemObject(mem_U[memU5]);
    clReleaseMemObject(mem_U[memU4b]);
    clReleaseMemObject(mem_U[memU4a]);
    clReleaseMemObject(mem_U[memU2]);
    clReleaseMemObject(mem_U[memU1z]);
    if (baby) {
        clReleaseMemObject(mem_U[memU1b]);
    }
    clReleaseMemObject(mem_U[memU1a]);
    clReleaseKernel(kernel[nlmUnpack]);
    clReleaseKernel(kernel[nlmPack]);
    clReleaseKernel(kernel[nlmFinish]);
    clReleaseKernel(kernel[nlmAccumulation]);
    clReleaseKernel(kernel[nlmVertical]);
    clReleaseKernel(kernel[nlmHorizontal]);
    clReleaseKernel(kernel[nlmDistance]);
    clReleaseProgram(program);
    clReleaseContext(context);
}

//////////////////////////////////////////
// AviSynthCreate
AVSValue __cdecl AviSynthPluginCreate(AVSValue args, void* user_data, IScriptEnvironment* env) {
    return new NLMAvisynth(
        args[0].AsClip(), 
        args[1].AsInt(DFT_d), 
        args[2].AsInt(DFT_a), 
        args[3].AsInt(DFT_s),
        args[4].AsFloat(DFT_h), 
        args[5].AsString(DFT_channels), 
        args[6].AsInt(DFT_wmode), 
        args[7].AsFloat(DFT_wref),
        args[8].Defined() ? args[8].AsClip() : nullptr, 
        args[9].AsString(DFT_ocl_device), 
        args[10].AsInt(DFT_ocl_id),
        args[11].AsInt(DFT_ocl_x), 
        args[12].AsInt(DFT_ocl_y), 
        args[13].AsInt(DFT_ocl_r), 
        args[14].AsBool(DFT_lsb),
        args[15].AsBool(DFT_info), env);
}

//////////////////////////////////////////
// AviSynthPluginInit
const AVS_Linkage *AVS_linkage = 0;
extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env,
    const AVS_Linkage * const vectors) {

    AVS_linkage = vectors;
    env->AddFunction("KNLMeansCL", "c[d]i[a]i[s]i[h]f[channels]s[wmode]i[wref]f[rclip]c[device_type]s[device_id]i[ocl_x]i[ocl_y]i\
[ocl_r]i[stacked]b[info]b", AviSynthPluginCreate, 0);
    if (env->FunctionExists("SetFilterMTMode")) {
        static_cast<IScriptEnvironment2*>(env)->SetFilterMTMode("KNLMeansCL", MT_MULTI_INSTANCE, true);
    }
    return "KNLMeansCL for AviSynth";
}

#endif //__AVISYNTH_6_H__