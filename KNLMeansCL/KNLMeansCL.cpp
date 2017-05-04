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

#ifdef _MSC_VER
#    define strcasecmp _stricmp
#    pragma warning (disable : 4514 4710 6031)
#endif

#include "KNLMeansCL.h"

//////////////////////////////////////////
// AviSynthFunctions
#ifdef __AVISYNTH_6_H__
inline bool _NLMAvisynth::equals(VideoInfo *v, VideoInfo *w) {
    return v->width == w->width && v->height == w->height && v->fps_numerator == w->fps_numerator &&
        v->fps_denominator == w->fps_denominator && v->num_frames == w->num_frames;
}

inline void _NLMAvisynth::oclErrorCheck(const char* function, cl_int errcode, IScriptEnvironment *env) {
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
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthFunctions
#ifdef VAPOURSYNTH_H
inline bool _NLMVapoursynth::equals(const VSVideoInfo *v, const VSVideoInfo *w) {   
    return v->width == w->width && v->height == w->height && v->fpsNum == w->fpsNum &&
        v->fpsDen == w->fpsDen && v->numFrames == w->numFrames && v->format == w->format;       
}

inline void _NLMVapoursynth::oclErrorCheck(const char* function, cl_int errcode, VSMap *out, const VSAPI *vsapi) {
    switch (errcode) {
        case OCL_UTILS_NO_DEVICE_AVAILABLE:
            vsapi->setError(out, "knlm.KNLMeansCL: no compatible opencl platforms available!");
            break;
        case CL_IMAGE_FORMAT_NOT_SUPPORTED:
            vsapi->setError(out, "knlm.KNLMeansCL: the opencl device does not support this video format!");
            break;
        default:
            char buffer[2048];
            snprintf(buffer, 2048, "knlm.KNLMeansCL: fatal error!\n (%s: %s)", function, oclUtilsErrorToString(errcode));
            vsapi->setError(out, buffer);
            break;
    }
    vsapi->freeNode(node);
    vsapi->freeNode(knot);
}
#endif //__VAPOURSYNTH_H__

//////////////////////////////////////////
// AviSynthInit
#ifdef __AVISYNTH_6_H__
_NLMAvisynth::_NLMAvisynth(PClip _child, const int _d, const int _a, const int _s, const double _h, const char* _channels,
    const int _wmode, const double _wref, PClip _baby, const char* _ocl_device, const int _ocl_id, const int _ocl_x,
    const int _ocl_y, const int _ocl_r, const bool _stacked, const bool _info, IScriptEnvironment *env) : GenericVideoFilter(_child),
    d(_d), a(_a), s(_s), h(_h), channels(_channels), wmode(_wmode), wref(_wref), baby(_baby), ocl_device(_ocl_device),
    ocl_id(_ocl_id), ocl_x(_ocl_x), ocl_y(_ocl_y), ocl_r(_ocl_r), stacked(_stacked), info(_info) {

    // Check AviSynth Version
    env->CheckVersion(5);
    child->SetCacheHints(CACHE_WINDOW, d);

    // Check source clip and rclip
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
    else if (!vi.IsYV24() && !strcasecmp(channels, "YUV"))
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
        channel_num = 4; /* 3 + buffer */
    } else if (!strcasecmp(channels, "Y")) {
        clip_t |= NLM_CLIP_REF_LUMA;
        channel_order = CL_R;
        channel_num = 2; /* 1 + buffer */
    } else if (!strcasecmp(channels, "UV")) {
        pre_processing = true;
        clip_t |= NLM_CLIP_REF_CHROMA;
        channel_order = CL_RG;
        channel_num = 3; /* 2 + buffer */
    } else if (!strcasecmp(channels, "RGB")) {
        clip_t |= NLM_CLIP_REF_RGB;
        channel_order = CL_RGBA;
        channel_num = 4; /* 3 + buffer */
    } else {
        if (vi.IsPlanar()) {
            clip_t |= NLM_CLIP_REF_LUMA;
            channel_order = CL_R;
            channel_num = 2; /* 1 + buffer */
        } else {
            clip_t |= NLM_CLIP_REF_RGB;
            channel_order = CL_RGBA;
            channel_num = 4; /* 3 + buffer */
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
                env->ThrowError("KNLMeansCL: P8, P10, P16 and Single are supported!");
            } else if (vi.IsYV24()) {
                clip_t |= NLM_CLIP_TYPE_UNSIGNED;
                channel_order = CL_RGB;
                channel_type_u = CL_UNORM_INT_101010;
                channel_type_p = CL_UNORM_INT16;
            } else {
                env->ThrowError("KNLMeansCL: only YUV444P10 is supported!");
            }
        } else if (vi.BitsPerComponent() == 16) {
            if (stacked) {
                env->ThrowError("KNLMeansCL: P8, P10, P16 and Single are supported!");
            } else {
                clip_t |= NLM_CLIP_TYPE_UNORM;
                channel_type_u = channel_type_p = CL_UNORM_INT16;
            }
        } else if (vi.BitsPerComponent() == 32) {
            if (stacked) {
                env->ThrowError("KNLMeansCL: P8, P10, P16 and Single are supported!");
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
        oclUtilsNlmClipTypeToString(clip_t), oclUtilsNlmClipRefToString(clip_t),
        oclUtilsNlmWmodeToString(wmode),
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
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthInit
#ifdef VAPOURSYNTH_H
static void VS_CC VapourSynthPluginViInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core,
    const VSAPI *vsapi) {

    NLMVapoursynth *d = (NLMVapoursynth*) * instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
}
#endif //__VAPOURSYNTH_H__

//////////////////////////////////////////
// AviSynthGetFrame
#ifdef __AVISYNTH_6_H__
PVideoFrame __stdcall _NLMAvisynth::GetFrame(int n, IScriptEnvironment* env) {
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
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthGetFrame
#ifdef VAPOURSYNTH_H
static const VSFrameRef *VS_CC VapourSynthPluginGetFrame(int n, int activationReason, void **instanceData,
    void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {

    NLMVapoursynth *d = (NLMVapoursynth*) * instanceData;
    int k_start = -min(int64ToIntS(d->d), n);
    int k_end = min(int64ToIntS(d->d), d->vi->numFrames - 1 - n);
    if (activationReason == arInitial) {
        for (int k = k_start; k <= k_end; k++) {
            vsapi->requestFrameFilter(n + k, d->node, frameCtx);
            if (d->knot) vsapi->requestFrameFilter(n + k, d->knot, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        // Variables
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx), *ref;
        const VSFormat *fi = d->vi->format;
        VSFrameRef *dst;
        int spt_side = 2 * int64ToIntS(d->a) + 1;
        int spt_area = spt_side * spt_side;
        size_t size_u2 = sizeof(cl_float) * d->idmn[0] * d->idmn[1] * d->channel_num;
        size_t size_u5 = sizeof(cl_float) * d->idmn[0] * d->idmn[1];
        const cl_int t = int64ToIntS(d->d);
        const cl_float pattern_u2 = 0.0f;
        const cl_float pattern_u5 = CL_FLT_EPSILON;
        const size_t origin[3] = { 0, 0, 0 };
        const size_t region[3] = { d->idmn[0], d->idmn[1], 1 };
        const size_t global_work[2] = { mrounds(d->idmn[0], 16), mrounds(d->idmn[1], 8) };
        const size_t global_work_dst[2] = {
            mrounds(d->idmn[0], d->dst_block[0]),
            mrounds(d->idmn[1], d->dst_block[1])
        };
        const size_t global_work_hrz[2] = {
            mrounds(d->idmn[0], d->hrz_result * d->hrz_block[0]) / d->hrz_result,
            mrounds(d->idmn[1], d->hrz_block[1])
        };
        const size_t global_work_vrt[2] = {
            mrounds(d->idmn[0], d->vrt_block[0]),
            mrounds(d->idmn[1], d->vrt_result * d->vrt_block[1]) / d->vrt_result
        };

        // Copy other 
        if (fi->colorFamily != cmGray && (d->clip_t & NLM_CLIP_REF_LUMA)) {
            const VSFrameRef * planeSrc[] = { NULL, src, src };
            const int planes[] = { 0, 1, 2 };
            dst = vsapi->newVideoFrame2(fi, (int) d->idmn[0], (int) d->idmn[1], planeSrc, planes, src, core);
        } else if (d->clip_t & NLM_CLIP_REF_CHROMA) {
            const VSFrameRef * planeSrc[] = { src, NULL, NULL };
            const int planes[] = { 0, 1, 2 };
            dst = vsapi->newVideoFrame2(fi, (int) d->idmn[0] << d->vi->format->subSamplingW, 
                (int) d->idmn[1] << d->vi->format->subSamplingH, planeSrc, planes, src, core);
        } else {
            dst = vsapi->newVideoFrame(fi, (int) d->idmn[0], (int) d->idmn[1], src, core);
        }
        vsapi->freeFrame(src);

        // Set-up buffers
        cl_int ret = CL_SUCCESS;
        ret |= clEnqueueFillBuffer(d->command_queue, d->mem_U[memU2], &pattern_u2, sizeof(cl_float), 0, size_u2, 0, NULL, NULL);
        ret |= clEnqueueFillBuffer(d->command_queue, d->mem_U[memU5], &pattern_u5, sizeof(cl_float), 0, size_u5, 0, NULL, NULL);

        // Read image
        for (int k = k_start; k <= k_end; k++) {
            src = vsapi->getFrameFilter(n + k, d->node, frameCtx);
            ref = (d->knot) ? vsapi->getFrameFilter(n + k, d->knot, frameCtx) : nullptr;
            const cl_int t_pk = t + k;
            const size_t origin_in[3] = { 0, 0, (size_t) t_pk };
            switch (d->clip_t) {
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_U[memU1a], CL_FALSE, origin_in, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_U[memU1a], CL_FALSE, origin_in, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_U[memU1b], CL_FALSE, origin_in, region,
                        (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA):
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_P[0], CL_FALSE, origin, region,
                        (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_P[1], CL_FALSE, origin, region,
                        (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmPack], 6, sizeof(cl_mem), &d->mem_U[memU1a]);
                    ret |= clSetKernelArg(d->kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA):
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_P[0], CL_FALSE, origin, region,
                        (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_P[1], CL_FALSE, origin, region,
                        (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmPack], 6, sizeof(cl_mem), &d->mem_U[memU1a]);
                    ret |= clSetKernelArg(d->kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_P[0], CL_FALSE, origin, region,
                        (size_t) vsapi->getStride(ref, 1), 0, vsapi->getReadPtr(ref, 1), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_P[1], CL_FALSE, origin, region,
                        (size_t) vsapi->getStride(ref, 2), 0, vsapi->getReadPtr(ref, 2), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmPack], 6, sizeof(cl_mem), &d->mem_U[memU1b]);
                    ret |= clSetKernelArg(d->kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB):
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_P[0], CL_FALSE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_P[1], CL_FALSE, origin, region,
                        (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_P[2], CL_FALSE, origin, region,
                        (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmPack], 6, sizeof(cl_mem), &d->mem_U[memU1a]);
                    ret |= clSetKernelArg(d->kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB):
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_P[0], CL_FALSE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_P[1], CL_FALSE, origin, region,
                        (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_P[2], CL_FALSE, origin, region,
                        (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmPack], 6, sizeof(cl_mem), &d->mem_U[memU1a]);
                    ret |= clSetKernelArg(d->kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_P[0], CL_FALSE, origin, region,
                        (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_P[1], CL_FALSE, origin, region,
                        (size_t) vsapi->getStride(ref, 1), 0, vsapi->getReadPtr(ref, 1), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(d->command_queue, d->mem_P[2], CL_FALSE, origin, region,
                        (size_t) vsapi->getStride(ref, 2), 0, vsapi->getReadPtr(ref, 2), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmPack], 6, sizeof(cl_mem), &d->mem_U[memU1b]);
                    ret |= clSetKernelArg(d->kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                default:
                    vsapi->setFilterError("knlm.KNLMeansCL: clip_t error!\n (VapourSynthGetFrame)", frameCtx);
                    vsapi->freeFrame(dst);
                    return 0;
            }
            vsapi->freeFrame(src);
            vsapi->freeFrame(ref);
        }

        // Spatio-temporal processing
        for (int k = k_start; k <= 0; k++) {
            for (int j = int64ToIntS(-d->a); j <= d->a; j++) {
                for (int i = int64ToIntS(-d->a); i <= d->a; i++) {
                    if (k * spt_area + j * spt_side + i < 0) {
                        const cl_int q[4] = { i, j, k, 0 };
                        ret |= clSetKernelArg(d->kernel[nlmDistance], 2, sizeof(cl_int), &t);
                        ret |= clSetKernelArg(d->kernel[nlmDistance], 3, 4 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmDistance],
                            2, NULL, global_work_dst, d->dst_block, 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[nlmHorizontal], 2, sizeof(cl_int), &t);
                        ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmHorizontal],
                            2, NULL, global_work_hrz, d->hrz_block, 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[nlmVertical], 2, sizeof(cl_int), &t);
                        ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmVertical],
                            2, NULL, global_work_vrt, d->vrt_block, 0, NULL, NULL);
                        if (k) {
                            const cl_int t_mq = t - k;
                            ret |= clSetKernelArg(d->kernel[nlmDistance], 2, sizeof(cl_int), &t_mq);
                            ret |= clSetKernelArg(d->kernel[nlmDistance], 3, 4 * sizeof(cl_int), &q);
                            ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmDistance],
                                2, NULL, global_work_dst, d->dst_block, 0, NULL, NULL);
                            ret |= clSetKernelArg(d->kernel[nlmHorizontal], 2, sizeof(cl_int), &t_mq);
                            ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmHorizontal],
                                2, NULL, global_work_hrz, d->hrz_block, 0, NULL, NULL);
                            ret |= clSetKernelArg(d->kernel[nlmVertical], 2, sizeof(cl_int), &t_mq);
                            ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmVertical],
                                2, NULL, global_work_vrt, d->vrt_block, 0, NULL, NULL);
                        }
                        ret |= clSetKernelArg(d->kernel[nlmAccumulation], 4, 4 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmAccumulation],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                    }
                }
            }
        }
        ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmFinish], 2, NULL, global_work, NULL, 0, NULL, NULL);

        // Write image
        switch (d->clip_t) {
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                ret |= clEnqueueReadImage(d->command_queue, d->mem_U[memU1z], CL_FALSE, origin, region,
                    (size_t) vsapi->getStride(dst, 0), 0, vsapi->getWritePtr(dst, 0), 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA):
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA):
                ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmUnpack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueReadImage(d->command_queue, d->mem_P[0], CL_FALSE, origin, region,
                    (size_t) vsapi->getStride(dst, 1), 0, vsapi->getWritePtr(dst, 1), 0, NULL, NULL);
                ret |= clEnqueueReadImage(d->command_queue, d->mem_P[1], CL_FALSE, origin, region,
                    (size_t) vsapi->getStride(dst, 2), 0, vsapi->getWritePtr(dst, 2), 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB):
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB):
                ret |= clEnqueueNDRangeKernel(d->command_queue, d->kernel[nlmUnpack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueReadImage(d->command_queue, d->mem_P[0], CL_FALSE, origin, region,
                    (size_t) vsapi->getStride(dst, 0), 0, vsapi->getWritePtr(dst, 0), 0, NULL, NULL);
                ret |= clEnqueueReadImage(d->command_queue, d->mem_P[1], CL_FALSE, origin, region,
                    (size_t) vsapi->getStride(dst, 1), 0, vsapi->getWritePtr(dst, 1), 0, NULL, NULL);
                ret |= clEnqueueReadImage(d->command_queue, d->mem_P[2], CL_FALSE, origin, region,
                    (size_t) vsapi->getStride(dst, 2), 0, vsapi->getWritePtr(dst, 2), 0, NULL, NULL);
                break;
            default:
                vsapi->setFilterError("knlm.KNLMeansCL: clip_t error!\n (VapourSynthGetFrame)", frameCtx);
                vsapi->freeFrame(dst);
                return 0;
        }

        // Blocks until commands have completed
        ret |= clFinish(d->command_queue);
        if (ret != CL_SUCCESS) {
            vsapi->setFilterError("knlm.KNLMeansCL: fatal error!\n (VapourSynthGetFrame)", frameCtx);
            vsapi->freeFrame(dst);
            return 0;
        }

        // Info
        if (d->info) {
            uint8_t y = 0, *frm = vsapi->getWritePtr(dst, 0);
            int pitch = vsapi->getStride(dst, 0);
            char buffer[2048], str[2048], str1[2048];
            DrawString(frm, pitch, 0, y++, "KNLMeansCL");
            DrawString(frm, pitch, 0, y++, " Version " VERSION);
            DrawString(frm, pitch, 0, y++, " Copyright(C) Khanattila");
            snprintf(buffer, 2048, " Bits per sample: %i", d->vi->format->bitsPerSample);
            DrawString(frm, pitch, 0, y++, buffer);
            snprintf(buffer, 2048, " Search window: %" PRId64 "x%" PRId64 "x%" PRId64,
                2 * d->a + 1, 2 * d->a + 1, 2 * d->d + 1);
            DrawString(frm, pitch, 0, y++, buffer);
            snprintf(buffer, 2048, " Similarity neighborhood: %" PRId64 "x%" PRId64, 2 * d->s + 1, 2 * d->s + 1);
            DrawString(frm, pitch, 0, y++, buffer);
            snprintf(buffer, 2048, " Num of ref pixels: %" PRId64, (2 * d->a + 1)*(2 * d->a + 1)*(2 * d->d + 1) - 1);
            DrawString(frm, pitch, 0, y++, buffer);
            snprintf(buffer, 2048, " Local work size: %zux%zu - %zux%zux%zu",
                d->dst_block[0], d->dst_block[1], d->hrz_block[0], d->hrz_block[1], d->hrz_result);
            DrawString(frm, pitch, 0, y++, buffer);
            DrawString(frm, pitch, 0, y++, "Platform info");
            ret |= clGetPlatformInfo(d->platformID, CL_PLATFORM_NAME, sizeof(char) * 2048, str, NULL);
            snprintf(buffer, 2048, " Name: %s", str);
            DrawString(frm, pitch, 0, y++, buffer);
            ret |= clGetPlatformInfo(d->platformID, CL_PLATFORM_VENDOR, sizeof(char) * 2048, str, NULL);
            snprintf(buffer, 2048, " Vendor: %s", str);
            DrawString(frm, pitch, 0, y++, buffer);
            ret |= clGetPlatformInfo(d->platformID, CL_PLATFORM_VERSION, sizeof(char) * 2048, str, NULL);
            snprintf(buffer, 2048, " Version: %s", str);
            DrawString(frm, pitch, 0, y++, buffer);
            DrawString(frm, pitch, 0, y++, "Device info");
            ret |= clGetDeviceInfo(d->deviceID, CL_DEVICE_NAME, sizeof(char) * 2048, str, NULL);
            snprintf(buffer, 2048, " Name: %s", str);
            DrawString(frm, pitch, 0, y++, buffer);
            ret |= clGetDeviceInfo(d->deviceID, CL_DEVICE_VENDOR, sizeof(char) * 2048, str, NULL);
            snprintf(buffer, 2048, " Vendor: %s", str);
            DrawString(frm, pitch, 0, y++, buffer);
            ret |= clGetDeviceInfo(d->deviceID, CL_DEVICE_VERSION, sizeof(char) * 2048, str, NULL);
            ret |= clGetDeviceInfo(d->deviceID, CL_DRIVER_VERSION, sizeof(char) * 2048, str1, NULL);
            snprintf(buffer, 2048, " Version: %s %s", str, str1);
            DrawString(frm, pitch, 0, y++, buffer);
            if (ret != CL_SUCCESS) {
                vsapi->setFilterError("knlm.KNLMeansCL: fatal error!\n (VapourSynthInfo)", frameCtx);
                vsapi->freeFrame(dst);
                return 0;
            }
        }
        return dst;
    }
    return 0;
}
#endif //__VAPOURSYNTH_H__

//////////////////////////////////////////
// AviSynthFree
#ifdef __AVISYNTH_6_H__
_NLMAvisynth::~_NLMAvisynth() {
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
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthFree
#ifdef VAPOURSYNTH_H
static void VS_CC VapourSynthPluginFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    NLMVapoursynth *d = (NLMVapoursynth*) instanceData;
    clReleaseCommandQueue(d->command_queue);
    if (d->pre_processing) {
        // d->mem_P[5] is only required to AviSynth
        // d->mem_P[4] is only required to AviSynth
        // d->mem_P[3] is only required to AviSynth
        clReleaseMemObject(d->mem_P[2]);
        clReleaseMemObject(d->mem_P[1]);
        clReleaseMemObject(d->mem_P[0]);
    }
    clReleaseMemObject(d->mem_U[memU5]);
    clReleaseMemObject(d->mem_U[memU4b]);
    clReleaseMemObject(d->mem_U[memU4a]);
    clReleaseMemObject(d->mem_U[memU2]);
    clReleaseMemObject(d->mem_U[memU1z]);
    if (d->knot) {
        clReleaseMemObject(d->mem_U[memU1b]);
    }
    clReleaseMemObject(d->mem_U[memU1a]);
    clReleaseKernel(d->kernel[nlmUnpack]);
    clReleaseKernel(d->kernel[nlmPack]);
    clReleaseKernel(d->kernel[nlmFinish]);
    clReleaseKernel(d->kernel[nlmAccumulation]);
    clReleaseKernel(d->kernel[nlmVertical]);
    clReleaseKernel(d->kernel[nlmHorizontal]);
    clReleaseKernel(d->kernel[nlmDistance]);
    clReleaseProgram(d->program);
    clReleaseContext(d->context);
    vsapi->freeNode(d->node);
    vsapi->freeNode(d->knot);
    free(d);
}
#endif //__VAPOURSYNTH_H__

//////////////////////////////////////////
// AviSynthCreate
#ifdef __AVISYNTH_6_H__
AVSValue __cdecl AviSynthPluginCreate(AVSValue args, void* user_data, IScriptEnvironment* env) {
    return new _NLMAvisynth(args[0].AsClip(), args[1].AsInt(DFT_d), args[2].AsInt(DFT_a), args[3].AsInt(DFT_s),
        args[4].AsFloat(DFT_h), args[5].AsString(DFT_channels), args[6].AsInt(DFT_wmode), args[7].AsFloat(DFT_wref),
        args[8].Defined() ? args[8].AsClip() : nullptr, args[9].AsString(DFT_ocl_device), args[10].AsInt(DFT_ocl_id),
        args[11].AsInt(DFT_ocl_x), args[12].AsInt(DFT_ocl_y), args[13].AsInt(DFT_ocl_r), args[14].AsBool(DFT_lsb),
        args[15].AsBool(DFT_info), env);
}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthCreate
#ifdef VAPOURSYNTH_H
static void VS_CC VapourSynthPluginCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {

    // Check source clip and rclip
    NLMVapoursynth d;
    int err;
    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.knot = vsapi->propGetNode(in, "rclip", 0, &err);
    d.vi = vsapi->getVideoInfo(d.node);
    if (err) {
        d.knot = nullptr;
        d.clip_t = NLM_CLIP_EXTRA_FALSE;
    } else d.clip_t = NLM_CLIP_EXTRA_TRUE;
    if (d.knot) {      
        const VSVideoInfo *vi2 = vsapi->getVideoInfo(d.knot);
        if (!d.equals(d.vi, vi2)) {
            vsapi->setError(out, "knlm.KNLMeansCL: 'rclip' does not match the source clip!");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.knot);
            return;
        }
    }

    // Set default value
    d.d = vsapi->propGetInt(in, "d", 0, &err);
    if (err) d.d = DFT_d;
    d.a = vsapi->propGetInt(in, "a", 0, &err);
    if (err) d.a = DFT_a;
    d.s = vsapi->propGetInt(in, "s", 0, &err);
    if (err) d.s = DFT_s;
    d.h = vsapi->propGetFloat(in, "h", 0, &err);
    if (err) d.h = DFT_h;
    d.channels = vsapi->propGetData(in, "channels", 0, &err);
    if (err) d.channels = DFT_channels;
    d.wmode = vsapi->propGetInt(in, "wmode", 0, &err);
    if (err) d.wmode = DFT_wmode;
    d.wref = vsapi->propGetFloat(in, "wref", 0, &err);
    if (err) d.wref = DFT_wref;
    d.ocl_device = vsapi->propGetData(in, "device_type", 0, &err);
    if (err) d.ocl_device = DFT_ocl_device;
    d.ocl_id = vsapi->propGetInt(in, "device_id", 0, &err);
    if (err) d.ocl_id = DFT_ocl_id;
    d.ocl_x = vsapi->propGetInt(in, "ocl_x", 0, &err);
    if (err) d.ocl_x = DFT_ocl_x;
    d.ocl_y = vsapi->propGetInt(in, "ocl_y", 0, &err);
    if (err) d.ocl_y = DFT_ocl_y;
    d.ocl_r = vsapi->propGetInt(in, "ocl_r", 0, &err);
    if (err) d.ocl_r = DFT_ocl_r;
    d.info = vsapi->propGetInt(in, "info", 0, &err);
    if (err) d.info = DFT_info;

    // Check user value
    if (d.d < 0) {
        vsapi->setError(out, "knlm.KNLMeansCL: 'd' must be greater than or equal to 0!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (d.a < 1) {
        vsapi->setError(out, "knlm.KNLMeansCL: 'a' must be greater than or equal to 1!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (d.s < 0 || d.s > 8) {
        vsapi->setError(out, "knlm.KNLMeansCL: 's' must be in range [0, 8]!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (d.h <= 0.0) {
        vsapi->setError(out, "knlm.KNLMeansCL: 'h' must be greater than 0!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (!isConstantFormat(d.vi)) {
        vsapi->setError(out, "knlm.KNLMeansCL: only constant format!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    switch (d.vi->format->colorFamily) {
        case VSColorFamily::cmGray:
            if (strcasecmp(d.channels, "Y") && strcasecmp(d.channels, "auto")) {
                vsapi->setError(out, "knlm.KNLMeansCL: 'channels' must be 'Y' with Gray color space!");
                vsapi->freeNode(d.node);
                vsapi->freeNode(d.knot);
                return;
            }
            break;
        case VSColorFamily::cmYUV:
        case VSColorFamily::cmYCoCg:
            if (strcasecmp(d.channels, "YUV") && strcasecmp(d.channels, "Y") &&
                strcasecmp(d.channels, "UV") && strcasecmp(d.channels, "auto")) {
                vsapi->setError(out, "knlm.KNLMeansCL: 'channels' must be 'YUV', 'Y', or 'UV' with YUV color space!");
                vsapi->freeNode(d.node);
                vsapi->freeNode(d.knot);
                return;
            }
            break;
        case VSColorFamily::cmRGB:
            if (strcasecmp(d.channels, "RGB") && strcasecmp(d.channels, "auto")) {
                vsapi->setError(out, "knlm.KNLMeansCL: 'channels' must be 'RGB' with RGB color space!");
                vsapi->freeNode(d.node);
                vsapi->freeNode(d.knot);
                return;
            }
            break;
        default:
            vsapi->setError(out, "knlm.KNLMeansCL: video format not supported!");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.knot);
            return;
    }
    if (d.wmode < 0 || d.wmode > 3) {
        vsapi->setError(out, "knlm.KNLMeansCL: 'wmode' must be in range [0, 3]!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (d.wref < 0.0) {
        vsapi->setError(out, "knlm.KNLMeansCL: 'wref' must be greater than or equal to 0!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    cl_uint ocl_device_type = 0;
    if (!strcasecmp(d.ocl_device, "CPU"))
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_CPU;
    else if (!strcasecmp(d.ocl_device, "GPU"))
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_GPU;
    else if (!strcasecmp(d.ocl_device, "ACCELERATOR"))
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_ACCELERATOR;
    else if (!strcasecmp(d.ocl_device, "AUTO"))
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_AUTO;
    else {
        vsapi->setError(out, "knlm.KNLMeansCL: 'device_type' must be 'cpu', 'gpu', 'accelerator' or 'auto'!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (d.ocl_id < 0) {
        vsapi->setError(out, "knlm.KNLMeansCL: 'device_id' must be greater than or equal to 0!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (d.ocl_x < 0 || d.ocl_y < 0 || d.ocl_r < 0) {
        vsapi->setError(out, "knlm.KNLMeansCL: 'ocl_x', 'ocl_y' and 'ocl_r' must be greater than 0!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    } else if (!(d.ocl_x == 0 && d.ocl_y == 0 && d.ocl_r == 0) && !(d.ocl_x > 0 && d.ocl_y > 0 && d.ocl_r > 0)) {
        vsapi->setError(out, "knlm.KNLMeansCL: 'ocl_x', 'ocl_y' and 'ocl_r' must be set!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (d.info && d.vi->format->bitsPerSample != 8) {
        vsapi->setError(out, "knlm.KNLMeansCL: 'info' requires Gray8 or YUVP8 color space!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }

    // Set image dimensions
    if (!strcasecmp(d.channels, "UV")) {
        d.idmn[0] = (cl_uint) d.vi->width >> d.vi->format->subSamplingW;
        d.idmn[1] = (cl_uint) d.vi->height >> d.vi->format->subSamplingH;
    } else {
        d.idmn[0] = (cl_uint) d.vi->width;
        d.idmn[1] = (cl_uint) d.vi->height;
    }

    // Set pre_processing, clip_t, channel_order and channel_num
    d.pre_processing = false;
    cl_channel_order channel_order;
    if (!strcasecmp(d.channels, "YUV")) {
        d.pre_processing = true;
        d.clip_t |= NLM_CLIP_REF_YUV;
        channel_order = CL_RGBA;
        d.channel_num = 4; /* 3 + buffer */
    } else if (!strcasecmp(d.channels, "Y")) {
        d.clip_t |= NLM_CLIP_REF_LUMA;
        channel_order = CL_R;
        d.channel_num = 2; /* 1 + buffer */
    } else if (!strcasecmp(d.channels, "UV")) {
        d.pre_processing = true;
        d.clip_t |= NLM_CLIP_REF_CHROMA;
        channel_order = CL_RG;
        d.channel_num = 3; /* 2 + buffer */
    } else if (!strcasecmp(d.channels, "RGB")) {
        d.pre_processing = true;
        d.clip_t |= NLM_CLIP_REF_RGB;
        channel_order = CL_RGBA;
        d.channel_num = 4; /* 3 + buffer */
    } else {
        if (d.vi->format->colorFamily == VSColorFamily::cmRGB) {
            d.pre_processing = true;
            d.clip_t |= NLM_CLIP_REF_RGB;
            channel_order = CL_RGBA;
            d.channel_num = 4; /* 3 + buffer */
        } else {
            d.clip_t |= NLM_CLIP_REF_LUMA;
            channel_order = CL_R;
            d.channel_num = 2; /* 1 + buffer */
        }
    }

    // Set channel_type
    cl_channel_type channel_type_u, channel_type_p;
    if (d.vi->format->sampleType == VSSampleType::stInteger) {
        if (d.vi->format->bitsPerSample == 8) {
            d.clip_t |= NLM_CLIP_TYPE_UNORM;
            channel_type_u = channel_type_p = CL_UNORM_INT8;
        } else if (d.vi->format->bitsPerSample == 10) {
            if (d.clip_t & NLM_CLIP_REF_YUV || d.clip_t & NLM_CLIP_REF_RGB) {
                d.clip_t |= NLM_CLIP_TYPE_UNSIGNED;
                channel_order = CL_RGB;
                channel_type_u = CL_UNORM_INT_101010;
                channel_type_p = CL_UNORM_INT16;
            } else {
                vsapi->setError(out, "knlm.KNLMeansCL: only YUV444P10 and RGB30 are supported!");
                vsapi->freeNode(d.node);
                vsapi->freeNode(d.knot);
                return;
            }
        } else if (d.vi->format->bitsPerSample == 16) {
            d.clip_t |= NLM_CLIP_TYPE_UNORM;
            channel_type_u = channel_type_p = CL_UNORM_INT16;
        } else {
            vsapi->setError(out, "knlm.KNLMeansCL: P8, P10, P16, Half and Single are supported!");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.knot);
            return;
        }
    } else if (d.vi->format->sampleType == VSSampleType::stFloat) {
        if (d.vi->format->bitsPerSample == 16) {
            d.clip_t |= NLM_CLIP_TYPE_UNORM;
            channel_type_u = channel_type_p = CL_HALF_FLOAT;
        } else if (d.vi->format->bitsPerSample == 32) {
            d.clip_t |= NLM_CLIP_TYPE_UNORM;
            channel_type_u = channel_type_p = CL_FLOAT;
        } else {
            vsapi->setError(out, "knlm.KNLMeansCL: P8, P10, P16, Half and Single are supported!");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.knot);
            return;
        }
    } else {
        vsapi->setError(out, "knlm.KNLMeansCL: video sample type not supported!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }

    // Get platformID and deviceID
    cl_int ret = oclUtilsGetPlaformDeviceIDs(ocl_device_type, (cl_uint) d.ocl_id, &d.platformID, &d.deviceID);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("oclUtilsGetPlaformDeviceIDs", ret, out, vsapi); return; }

    // Get maximum number of work-items
    if (d.ocl_x > 0 || d.ocl_y > 0 || d.ocl_r > 0) {
        d.hrz_block[0] = d.vrt_block[0] = (size_t) d.ocl_x;
        d.hrz_block[1] = d.vrt_block[1] = (size_t) d.ocl_y;
        d.hrz_result = d.vrt_result = (size_t) d.ocl_r;
    } else {
        size_t max_work_group_size;
        ret = clGetDeviceInfo(d.deviceID, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &max_work_group_size, NULL);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clGetDeviceInfo", ret, out, vsapi); return; }
        switch (max_work_group_size) {
            case 256: // AMD
                d.hrz_block[0] = d.vrt_block[0] = 32;
                d.hrz_block[1] = d.vrt_block[1] = 8;
                d.hrz_result = d.vrt_result = 1;
                break;
            case 512: // INTEL GPU
                d.hrz_block[0] = d.vrt_block[0] = 32;
                d.hrz_block[1] = d.vrt_block[1] = 8;
                d.hrz_result = d.vrt_result = 3;
                break;
            case 1024: // NVIDIA
                d.hrz_block[0] = d.vrt_block[0] = 16;
                d.hrz_block[1] = d.vrt_block[1] = 8;
                d.hrz_result = d.vrt_result = 3;
                break;
            case 8192: // INTEL
                d.hrz_block[0] = d.vrt_block[0] = 64;
                d.hrz_block[1] = d.vrt_block[1] = 8;
                d.hrz_result = d.vrt_result = 5;
                break;
            default:
                d.hrz_block[0] = d.vrt_block[0] = 8;
                d.hrz_block[1] = d.vrt_block[1] = 8;
                d.hrz_result = d.vrt_result = 1;
                break;
        }
    }

    // Create an OpenCL context
    d.context = clCreateContext(NULL, 1, &d.deviceID, NULL, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateContext", ret, out, vsapi); return; }

    // Create a command queue
    d.command_queue = clCreateCommandQueue(d.context, d.deviceID, 0, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateCommandQueue", ret, out, vsapi); return; }

    // Create mem_in[] and mem_out
    size_t size_u2 = sizeof(cl_float) * d.idmn[0] * d.idmn[1] * d.channel_num;
    size_t size_u5 = sizeof(cl_float) * d.idmn[0] * d.idmn[1];
    cl_mem_flags flags_u1ab, flags_u1z;
    if (d.clip_t & NLM_CLIP_REF_LUMA) {
        flags_u1ab = CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY;
        flags_u1z = CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY;
    } else {
        flags_u1ab = flags_u1z = CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS;
    }
    const cl_image_format format_u1 = { channel_order, channel_type_u };
    const cl_image_format format_u4 = { CL_R, CL_FLOAT };
    size_t array_size = (size_t) (2 * d.d + 1);
    const cl_image_desc desc_u = { CL_MEM_OBJECT_IMAGE2D_ARRAY, d.idmn[0], d.idmn[1], 1, array_size, 0, 0, 0, 0, NULL };
    const cl_image_desc desc_u1z = { CL_MEM_OBJECT_IMAGE2D, d.idmn[0], d.idmn[1], 1, 1, 0, 0, 0, 0, NULL };
    d.mem_U[memU1a] = clCreateImage(d.context, flags_u1ab, &format_u1, &desc_u, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_P[memU1a])", ret, out, vsapi); return; }
    if (d.knot) {
        d.mem_U[memU1b] = clCreateImage(d.context, flags_u1ab, &format_u1, &desc_u, NULL, &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_P[memU1b])", ret, out, vsapi); return; }
    }
    d.mem_U[memU1z] = clCreateImage(d.context, flags_u1z, &format_u1, &desc_u1z, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_P[memU1z])", ret, out, vsapi); return; }
    d.mem_U[memU2] = clCreateBuffer(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u2, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_P[memU2])", ret, out, vsapi); return; }
    d.mem_U[memU4a] = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &format_u4, &desc_u, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_P[memU4a])", ret, out, vsapi); return; }
    d.mem_U[memU4b] = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &format_u4, &desc_u, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_P[memU4b])", ret, out, vsapi); return; }
    d.mem_U[memU5] = clCreateBuffer(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u5, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_P[memU0])", ret, out, vsapi); return; }

    // Create mem_P[]
    if (d.pre_processing) {
        const cl_image_format format_p = { CL_R, channel_type_p };
        const cl_image_desc desc_p = { CL_MEM_OBJECT_IMAGE2D, d.idmn[0], d.idmn[1], 1, 1, 0, 0, 0, 0, NULL };
        d.mem_P[0] = clCreateImage(d.context, CL_MEM_READ_WRITE, &format_p, &desc_p, NULL, &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_P[0])", ret, out, vsapi); return; }
        d.mem_P[1] = clCreateImage(d.context, CL_MEM_READ_WRITE, &format_p, &desc_p, NULL, &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_P[1])", ret, out, vsapi); return; }
        d.mem_P[2] = clCreateImage(d.context, CL_MEM_READ_WRITE, &format_p, &desc_p, NULL, &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_P[2])", ret, out, vsapi); return; }
        // d.mem_P[3] is only required to AviSynth
        // d.mem_P[4] is only required to AviSynth
        // d.mem_P[5] is only required to AviSynth
    }

    // Create and Build a program executable from the program source
    d.program = clCreateProgramWithSource(d.context, 1, &kernel_source_code, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateProgramWithSource", ret, out, vsapi); return; }
    char options[2048];
    setlocale(LC_ALL, "C");
#    ifdef __APPLE__
    snprintf(options, 2048, "-cl-denorms-are-zero -cl-fast-relaxed-math -Werror "
        "-D %s -D %s -D %s -D VI_DIM_X=%u -D VI_DIM_Y=%u -D HRZ_RESULT=%zu -D VRT_RESULT=%zu "
        "-D HRZ_BLOCK_X=%zu -D HRZ_BLOCK_Y=%zu -D VRT_BLOCK_X=%zu -D VRT_BLOCK_Y=%zu "
        "-D NLM_D=%i -D NLM_S=%i -D NLM_H=%ff -D NLM_WREF=%ff",
        oclUtilsNlmClipTypeToString(d.clip_t), oclUtilsNlmClipRefToString(d.clip_t),
        oclUtilsNlmWmodeToString(int64ToIntS(d.wmode)),
        d.idmn[0], d.idmn[1], d.hrz_result, d.vrt_result,
        d.hrz_block[0], d.hrz_block[1], d.vrt_block[0], d.vrt_block[1],
        int64ToIntS(d.d), int64ToIntS(d.s), d.h, d.wref);
#    else
    snprintf(options, 2048, "-cl-single-precision-constant -cl-denorms-are-zero -cl-fast-relaxed-math -Werror \
        -D %s -D %s -D %s -D VI_DIM_X=%u -D VI_DIM_Y=%u -D HRZ_RESULT=%zu -D VRT_RESULT=%zu \
        -D HRZ_BLOCK_X=%zu -D HRZ_BLOCK_Y=%zu -D VRT_BLOCK_X=%zu -D VRT_BLOCK_Y=%zu \
        -D NLM_D=%i -D NLM_S=%i -D NLM_H=%f -D NLM_WREF=%f",
        oclUtilsNlmClipTypeToString(d.clip_t), oclUtilsNlmClipRefToString(d.clip_t),
        oclUtilsNlmWmodeToString(int64ToIntS(d.wmode)),
        d.idmn[0], d.idmn[1], d.hrz_result, d.vrt_result,
        d.hrz_block[0], d.hrz_block[1], d.vrt_block[0], d.vrt_block[1],
        int64ToIntS(d.d), int64ToIntS(d.s), d.h, d.wref);
#    endif
    ret = clBuildProgram(d.program, 1, &d.deviceID, options, NULL, NULL);
    if (ret != CL_SUCCESS) {
        oclUtilsDebugInfo(d.platformID, d.deviceID, d.program, ret);
        vsapi->setError(out, "knlm.KNLMeansCL: build programm error!\n Please report Log-KNLMeansCL.txt.");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    setlocale(LC_ALL, "");

    // Create kernel objects
    d.kernel[nlmDistance] = clCreateKernel(d.program, "nlmDistance", &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmDistance)", ret, out, vsapi); return; }
    d.kernel[nlmHorizontal] = clCreateKernel(d.program, "nlmHorizontal", &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmHorizontal)", ret, out, vsapi); return; }
    d.kernel[nlmVertical] = clCreateKernel(d.program, "nlmVertical", &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmVertical)", ret, out, vsapi); return; }
    d.kernel[nlmAccumulation] = clCreateKernel(d.program, "nlmAccumulation", &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmAccumulation)", ret, out, vsapi); return; }
    d.kernel[nlmFinish] = clCreateKernel(d.program, "nlmFinish", &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmFinish)", ret, out, vsapi); return; }
    d.kernel[nlmPack] = clCreateKernel(d.program, "nlmPack", &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmPack)", ret, out, vsapi); return; }
    d.kernel[nlmUnpack] = clCreateKernel(d.program, "nlmUnpack", &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmUnpack)", ret, out, vsapi); return; }

    // Get some kernel info
    size_t dst_work_group;
    ret = clGetKernelWorkGroupInfo(d.kernel[nlmDistance], d.deviceID,
        CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &dst_work_group, NULL);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clGetKernelWorkGroupInfo(nlmDistance)", ret, out, vsapi); return; }
    if (dst_work_group >= 1024) {
        d.dst_block[0] = d.dst_block[1] = 32;
    } else if (dst_work_group >= 256) {
        d.dst_block[0] = d.dst_block[1] = 16;
    } else if (dst_work_group >= 64) {
        d.dst_block[0] = d.dst_block[1] = 8;
    } else {
        d.dst_block[0] = d.dst_block[1] = 1;
    }

    // Set kernel arguments - nlmDistanceLeft
    int index_u1 = (d.knot) ? memU1b : memU1a;
    ret = clSetKernelArg(d.kernel[nlmDistance], 0, sizeof(cl_mem), &d.mem_U[index_u1]);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmDistance[0])", ret, out, vsapi); return; }
    ret = clSetKernelArg(d.kernel[nlmDistance], 1, sizeof(cl_mem), &d.mem_U[memU4a]);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmDistance[1])", ret, out, vsapi); return; }
    // d.kernel[nlmDistance] -> 2 is set by VapourSynthPluginGetFrame
    // d.kernel[nlmDistance] -> 3 is set by VapourSynthPluginGetFrame

    // nlmHorizontal
    ret = clSetKernelArg(d.kernel[nlmHorizontal], 0, sizeof(cl_mem), &d.mem_U[memU4a]);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmHorizontal[0])", ret, out, vsapi); return; }
    ret = clSetKernelArg(d.kernel[nlmHorizontal], 1, sizeof(cl_mem), &d.mem_U[memU4b]);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmHorizontal[1])", ret, out, vsapi); return; }
    // d.kernel[nlmHorizontal] -> 2 is set by VapourSynthPluginGetFrame

    // nlmVertical
    ret = clSetKernelArg(d.kernel[nlmVertical], 0, sizeof(cl_mem), &d.mem_U[memU4b]);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmVertical[0])", ret, out, vsapi); return; }
    ret = clSetKernelArg(d.kernel[nlmVertical], 1, sizeof(cl_mem), &d.mem_U[memU4a]);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmVertical[1])", ret, out, vsapi); return; }
    // d.kernel[nlmVertical] -> 2 is set by VapourSynthPluginGetFrame

    // nlmAccumulation
    ret = clSetKernelArg(d.kernel[nlmAccumulation], 0, sizeof(cl_mem), &d.mem_U[memU1a]);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmAccumulation[0])", ret, out, vsapi); return; }
    ret = clSetKernelArg(d.kernel[nlmAccumulation], 1, sizeof(cl_mem), &d.mem_U[memU2]);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmAccumulation[1])", ret, out, vsapi); return; }
    ret = clSetKernelArg(d.kernel[nlmAccumulation], 2, sizeof(cl_mem), &d.mem_U[memU4a]);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmAccumulation[2])", ret, out, vsapi); return; }
    ret = clSetKernelArg(d.kernel[nlmAccumulation], 3, sizeof(cl_mem), &d.mem_U[memU5]);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmAccumulation[3])", ret, out, vsapi); return; }
    // d.kernel[nlmAccumulation] -> 4 is set by VapourSynthPluginGetFrame

    // nlmFinish
    ret = clSetKernelArg(d.kernel[nlmFinish], 0, sizeof(cl_mem), &d.mem_U[memU1a]);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmFinish[0])", ret, out, vsapi); return; }
    ret = clSetKernelArg(d.kernel[nlmFinish], 1, sizeof(cl_mem), &d.mem_U[memU1z]);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmFinish[1])", ret, out, vsapi); return; }
    ret = clSetKernelArg(d.kernel[nlmFinish], 2, sizeof(cl_mem), &d.mem_U[memU2]);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmFinish[2])", ret, out, vsapi); return; }
    ret = clSetKernelArg(d.kernel[nlmFinish], 3, sizeof(cl_mem), &d.mem_U[memU5]);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmFinish[3])", ret, out, vsapi); return; }

    // nlmPack
    if (d.pre_processing) {
        ret = clSetKernelArg(d.kernel[nlmPack], 0, sizeof(cl_mem), &d.mem_P[0]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmPack[0])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmPack], 1, sizeof(cl_mem), &d.mem_P[1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmPack[1])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmPack], 2, sizeof(cl_mem), &d.mem_P[2]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmPack[2])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmPack], 3, sizeof(cl_mem), &d.mem_P[0]); // dummy, 3 is reserved for AviSynth
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmPack[3])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmPack], 4, sizeof(cl_mem), &d.mem_P[1]); // dummy, 4 is reserved for AviSynth
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmPack[4])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmPack], 5, sizeof(cl_mem), &d.mem_P[2]); // dummy, 5 is reserved for AviSynth
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmPack[5])", ret, out, vsapi); return; }
        // d.kernel[nlmPack] -> 6 is set by VapourSynthPluginGetFrame
        // d.kernel[nlmPack] -> 7 is set by VapourSynthPluginGetFrame
    }

    // nlmUnpack
    if (d.pre_processing) {
        ret = clSetKernelArg(d.kernel[nlmUnpack], 0, sizeof(cl_mem), &d.mem_P[0]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmUnpack[0])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmUnpack], 1, sizeof(cl_mem), &d.mem_P[1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmUnpack[1])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmUnpack], 2, sizeof(cl_mem), &d.mem_P[2]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmUnpack[2])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmUnpack], 3, sizeof(cl_mem), &d.mem_P[0]); // dummy, 3 is reserved for AviSynth
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmUnpack[3])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmUnpack], 4, sizeof(cl_mem), &d.mem_P[1]); // dummy, 4 is reserved for AviSynth
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmUnpack[4])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmUnpack], 5, sizeof(cl_mem), &d.mem_P[2]); // dummy, 5 is reserved for AviSynth
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmUnpack[5])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmUnpack], 6, sizeof(cl_mem), &d.mem_U[memU1z]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmUnpack[6])", ret, out, vsapi); return; }
    }

    // Create a new filter and return a reference to it
    NLMVapoursynth *data = (NLMVapoursynth*) malloc(sizeof(d));
    if (data) *data = d;
    else {
        vsapi->setError(out, "knlm.KNLMeansCL: fatal error!\n (malloc fail)");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    vsapi->createFilter(in, out, "KNLMeansCL", VapourSynthPluginViInit, VapourSynthPluginGetFrame, VapourSynthPluginFree,
        fmParallelRequests, 0, data, core);
}
#endif //__VAPOURSYNTH_H__

//////////////////////////////////////////
// AviSynthPluginInit
#ifdef __AVISYNTH_6_H__
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

//////////////////////////////////////////
// VapourSynthPluginInit
#ifdef VAPOURSYNTH_H
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.Khanattila.KNLMeansCL", "knlm", "KNLMeansCL for VapourSynth", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("KNLMeansCL", "clip:clip;d:int:opt;a:int:opt;s:int:opt;h:float:opt;channels:data:opt;wmode:int:opt;\
wref:float:opt;rclip:clip:opt;device_type:data:opt;device_id:int:opt;ocl_x:int:opt;ocl_y:int:opt;ocl_r:int:opt;info:int:opt",
VapourSynthPluginCreate, nullptr, plugin);
}
#endif //__VAPOURSYNTH_H__