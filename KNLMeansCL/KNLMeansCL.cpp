/*
*    This file is part of KNLMeansCL,
*    Copyright(C) 2015-2016  Edoardo Brunetti.
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
#define DFT_wmode         1
#define DFT_wref          1.0f
#define DFT_ocl_device    "AUTO"
#define DFT_ocl_id        0
#define DFT_lsb           false
#define DFT_info          false

#ifdef _MSC_VER
#    define _stdcall __stdcall
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
    const int _wmode, const double _wref, PClip _baby, const char* _ocl_device, const int _ocl_id, const bool _lsb,
    const bool _info, IScriptEnvironment *env) : GenericVideoFilter(_child), d(_d), a(_a), s(_s), h(_h), channels(_channels),
    wmode(_wmode), wref(_wref), baby(_baby), ocl_device(_ocl_device), ocl_id(_ocl_id), lsb(_lsb), info(_info) {

    // Check AviSynth Version
    env->CheckVersion(5);
    child->SetCacheHints(CACHE_WINDOW, d);

    // Check source clip and rclip
    if (!vi.IsPlanar() && !vi.IsRGB32())
        env->ThrowError("KNLMeansCL: planar YUV or RGB32 data!");
    if (baby) {
        VideoInfo rvi = baby->GetVideoInfo();
        if (!equals(&vi, &rvi)) env->ThrowError("KNLMeansCL: 'rclip' does not match the source clip!");
        baby->SetCacheHints(CACHE_WINDOW, d);
        clip_t = NLM_CLIP_EXTRA_TRUE;
    } else
        clip_t = NLM_CLIP_EXTRA_FALSE;

    // Checks user value
    if (d < 0)
        env->ThrowError("KNLMeansCL: 'd' must be greater than or equal to 0!");
    if (a < 1)
        env->ThrowError("KNLMeansCL: 'a' must be greater than or equal to 1!");
    if (s < 0 || s > 4)
        env->ThrowError("KNLMeansCL: 's' must be in range [0, 4]!");
    if (h <= 0.0f)
        env->ThrowError("KNLMeansCL: 'h' must be greater than 0!");
    if (vi.IsY8() && strcasecmp(channels, "Y") && strcasecmp(channels, "auto"))
        env->ThrowError("KNLMeansCL: 'channels' must be 'Y' with Y8 pixel format!");
    else if (vi.IsPlanar() && strcasecmp(channels, "YUV") && strcasecmp(channels, "Y") &&
        strcasecmp(channels, "UV") && strcasecmp(channels, "auto"))
        env->ThrowError("KNLMeansCL: 'channels' must be 'YUV', 'Y', 'UV' with YUV color space!");
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
    if (lsb && vi.IsRGB())
        env->ThrowError("KNLMeansCL: RGB48y is not supported!");
    if (info && vi.IsRGB())
        env->ThrowError("KNLMeansCL: 'info' requires YUV color space!");

    // Set image dimensions
    if (!strcasecmp(channels, "UV")) {
        int subSamplingW = vi.IsYV24() ? 0 : 1;
        int subSamplingH = lsb ? (vi.IsYV12() ? 2 : 1) : (vi.IsYV12() ? 1 : 0);
        idmn[0] = (cl_uint) vi.width >> subSamplingW;
        idmn[1] = (cl_uint) vi.height >> subSamplingH;
    } else {
        idmn[0] = (cl_uint) vi.width;
        idmn[1] = (cl_uint) (lsb ? (vi.height >> 1) : vi.height);
    }

    // Set clip_t, channel_order and channel_type
    cl_channel_order channel_order;
    cl_channel_type channel_type;
    if (!strcasecmp(channels, "YUV")) {
        clip_t |= NLM_CLIP_REF_YUV;
        channel_order = CL_RGBA;
        channel_num = 4; /* 3 + buffer */
    } else if (!strcasecmp(channels, "Y")) {
        clip_t |= NLM_CLIP_REF_LUMA;
        channel_order = CL_R;
        channel_num = 2; /* 1 + buffer */
    } else if (!strcasecmp(channels, "UV")) {
        clip_t |= NLM_CLIP_REF_CHROMA;
        channel_order = CL_RG;
        channel_num = 4; /* 2 + padding + buffer */
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
    if (lsb) {
        clip_t |= NLM_CLIP_TYPE_STACKED;
        channel_type = CL_UNORM_INT16;
    } else {
        clip_t |= NLM_CLIP_TYPE_UNORM;
        channel_type = CL_UNORM_INT8;
    }

    // Get platformID and deviceID
    cl_int ret = oclUtilsGetPlaformDeviceIDs(ocl_device_type, (cl_uint) ocl_id, &platformID, &deviceID);
    oclErrorCheck("oclUtilsGetPlaformDeviceIDs", ret, env);

    // Create an OpenCL context
    context = clCreateContext(NULL, 1, &deviceID, NULL, NULL, &ret);
    oclErrorCheck("clCreateContext", ret, env);

    // Create a command queue
    command_queue = clCreateCommandQueue(context, deviceID, 0, &ret);

    // Create mem_in[] and mem_out
    const cl_image_format format = { channel_order, channel_type };
    const cl_image_desc desc_in = { (cl_mem_object_type) (d ? CL_MEM_OBJECT_IMAGE2D_ARRAY : CL_MEM_OBJECT_IMAGE2D),
        idmn[0], idmn[1], 1, 2 * (size_t) d + 1, 0, 0, 0, 0, NULL };
    const cl_image_desc desc_out = { CL_MEM_OBJECT_IMAGE2D, idmn[0], idmn[1], 1, 1, 0, 0, 0, 0, NULL };
    if (!(clip_t & NLM_CLIP_REF_CHROMA) && !(clip_t & NLM_CLIP_REF_YUV) && !lsb) {
        mem_in[0] = clCreateImage(context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, &format, &desc_in, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_in[0])", ret, env);
        mem_in[1] = clCreateImage(context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, &format, &desc_in, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_in[1])", ret, env);
        mem_out = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY, &format, &desc_out, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_out)", ret, env);  /* (CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY) */
    } else {
        mem_in[0] = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &format, &desc_in, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_in[0])", ret, env);
        mem_in[1] = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &format, &desc_in, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_in[1])", ret, env);
        mem_out = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &format, &desc_out, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_out)", ret, env);
    }

    // Create mem_U[]   
    size_t size_u0 = sizeof(cl_float) * idmn[0] * idmn[1] * channel_num;
    size_t size_u3 = sizeof(cl_float) * idmn[0] * idmn[1];
    const cl_image_format format_u = { CL_R, CL_FLOAT };
    mem_U[0] = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u0, NULL, &ret);
    oclErrorCheck("clCreateBuffer(mem_U[0])", ret, env);
    mem_U[1] = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &format_u, &desc_in, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_U[1])", ret, env);
    mem_U[2] = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &format_u, &desc_in, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_U[2])", ret, env);
    mem_U[3] = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u3, NULL, &ret);
    oclErrorCheck("clCreateBuffer(mem_U[3])", ret, env);
    
    // Create mem_P[]
    const cl_image_format format_p = { CL_R, (cl_channel_type) (lsb ? CL_UNSIGNED_INT8 : CL_UNORM_INT8) };
    mem_P[0] = clCreateImage(context, CL_MEM_READ_WRITE, &format_p, &desc_out, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_P[0])", ret, env);
    mem_P[1] = clCreateImage(context, CL_MEM_READ_WRITE, &format_p, &desc_out, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_P[1])", ret, env);
    mem_P[2] = clCreateImage(context, CL_MEM_READ_WRITE, &format_p, &desc_out, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_P[2])", ret, env);
    mem_P[3] = clCreateImage(context, CL_MEM_READ_WRITE, &format_p, &desc_out, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_P[3])", ret, env);
    mem_P[4] = clCreateImage(context, CL_MEM_READ_WRITE, &format_p, &desc_out, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_P[4])", ret, env);
    mem_P[5] = clCreateImage(context, CL_MEM_READ_WRITE, &format_p, &desc_out, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_P[5])", ret, env);

    // Creates and Build a program executable from the program source
    program = clCreateProgramWithSource(context, 1, d ? &kernel_source_code : &kernel_source_code_spatial, NULL, NULL);
    char options[2048];
    setlocale(LC_ALL, "C");
    snprintf(options, 2048, "-cl-single-precision-constant -cl-denorms-are-zero -cl-fast-relaxed-math -Werror \
        -D NLM_CLIP_TYPE_UNORM=%u -D NLM_CLIP_TYPE_UNSIGNED=%u -D NLM_CLIP_TYPE_STACKED=%u \
        -D NLM_CLIP_REF_LUMA=%u -D NLM_CLIP_REF_CHROMA=%u -D NLM_CLIP_REF_YUV=%u -D NLM_CLIP_REF_RGB=%u \
        -D NLM_WMODE_CAUCHY=%u -D NLM_WMODE_WELSCH=%u -D NLM_WMODE_BISQUARE=%u -D NLM_WMODE_MOD_BISQUARE=%u \
        -D HRZ_BLOCK_X=%u -D HRZ_BLOCK_Y=%u -D HRZ_RESULT=%u -D VRT_BLOCK_X=%u -D VRT_BLOCK_Y=%u -D VRT_RESULT=%u \
        -D NLM_TCLIP=%u -D NLM_D=%i -D NLM_S=%i -D NLM_WMODE=%i -D NLM_WREF=%.17f -D NLM_H2_INV_NORM=%.17f -D NLM_UNORM_MAX=%f",
        NLM_CLIP_TYPE_UNORM, NLM_CLIP_TYPE_UNSIGNED, NLM_CLIP_TYPE_STACKED,
        NLM_CLIP_REF_LUMA, NLM_CLIP_REF_CHROMA, NLM_CLIP_REF_YUV, NLM_CLIP_REF_RGB,
        NLM_WMODE_CAUCHY, NLM_WMODE_WELSCH, NLM_WMODE_BISQUARE, NLM_WMODE_MOD_BISQUARE,
        HRZ_BLOCK_X, HRZ_BLOCK_Y, HRZ_RESULT, VRT_BLOCK_X, VRT_BLOCK_Y, VRT_RESULT,
        clip_t, d, s, wmode, wref, 65025.0 / (3 * h * h * (2 * s + 1)*(2 * s + 1)), (double) maxvalue(8));
    ret = clBuildProgram(program, 1, &deviceID, options, NULL, NULL);
    if (ret != CL_SUCCESS) {
        oclUtilsDebugInfo(platformID, deviceID, program);
        env->ThrowError("KNLMeansCL: build program error!\n Please report Log-KNLMeansCL.txt.");
    }
    setlocale(LC_ALL, "");

    // Creates kernel objects.
    if (d) {
        kernel[nlmDistanceLeft] = clCreateKernel(program, "nlmDistanceLeft", &ret);
        oclErrorCheck("clCreateKernel(nlmDistanceLeft)", ret, env);
        kernel[nlmDistanceRight] = clCreateKernel(program, "nlmDistanceRight", &ret);
        oclErrorCheck("clCreateKernel(nlmDistanceRight)", ret, env);
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
    } else {
        kernel[nlmSpatialDistance] = clCreateKernel(program, "nlmSpatialDistance", &ret);
        oclErrorCheck("clCreateKernel(nlmSpatialDistance)", ret, env);
        kernel[nlmSpatialHorizontal] = clCreateKernel(program, "nlmSpatialHorizontal", &ret);
        oclErrorCheck("clCreateKernel(nlmSpatialHorizontal)", ret, env);
        kernel[nlmSpatialVertical] = clCreateKernel(program, "nlmSpatialVertical", &ret);
        oclErrorCheck("clCreateKernel(nlmSpatialVertical)", ret, env);
        kernel[nlmSpatialAccumulation] = clCreateKernel(program, "nlmSpatialAccumulation", &ret);
        oclErrorCheck("clCreateKernel(nlmSpatialAccumulation)", ret, env);
        kernel[nlmSpatialFinish] = clCreateKernel(program, "nlmSpatialFinish", &ret);
        oclErrorCheck("clCreateKernel(nlmSpatialFinish)", ret, env);
        kernel[nlmSpatialPack] = clCreateKernel(program, "nlmSpatialPack", &ret);
        oclErrorCheck("clCreateKernel(nlmSpatialPack)", ret, env);
        kernel[nlmSpatialUnpack] = clCreateKernel(program, "nlmSpatialUnpack", &ret);
        oclErrorCheck("clCreateKernel(nlmSpatialUnpack)", ret, env);
    }

    // Set kernel arguments
    if (d) {
        // nlmDistanceLeft
        ret = clSetKernelArg(kernel[nlmDistanceLeft], 0, sizeof(cl_mem), &mem_in[(clip_t & NLM_CLIP_EXTRA_FALSE) ? 0 : 1]);
        oclErrorCheck("clSetKernelArg(nlmDistanceLeft[0])", ret, env);
        ret = clSetKernelArg(kernel[nlmDistanceLeft], 1, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(nlmDistanceLeft[1])", ret, env);
        ret = clSetKernelArg(kernel[nlmDistanceLeft], 2, 2 * sizeof(cl_uint), &idmn);
        oclErrorCheck("clSetKernelArg(nlmDistanceLeft[2])", ret, env);

        // nlmDistanceRight
        ret = clSetKernelArg(kernel[nlmDistanceRight], 0, sizeof(cl_mem), &mem_in[(clip_t & NLM_CLIP_EXTRA_FALSE) ? 0 : 1]);
        oclErrorCheck("clSetKernelArg(nlmDistanceRight[0])", ret, env);
        ret = clSetKernelArg(kernel[nlmDistanceRight], 1, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(nlmDistanceRight[1])", ret, env);
        ret = clSetKernelArg(kernel[nlmDistanceRight], 2, 2 * sizeof(cl_uint), &idmn);
        oclErrorCheck("clSetKernelArg(nlmDistanceRight[2])", ret, env);

        // nlmHorizontal
        ret = clSetKernelArg(kernel[nlmHorizontal], 0, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(nlmHorizontal[0])", ret, env);
        ret = clSetKernelArg(kernel[nlmHorizontal], 1, sizeof(cl_mem), &mem_U[2]);
        oclErrorCheck("clSetKernelArg(nlmHorizontal[1])", ret, env);
        // kernel[nlmHorizontal] -> 2 is set by AviSynthPluginGetFrame
        ret = clSetKernelArg(kernel[nlmHorizontal], 3, 2 * sizeof(cl_uint), &idmn);
        oclErrorCheck("clSetKernelArg(nlmHorizontal[3])", ret, env);

        // nlmVertical
        ret = clSetKernelArg(kernel[nlmVertical], 0, sizeof(cl_mem), &mem_U[2]);
        oclErrorCheck("clSetKernelArg(nlmVertical[0])", ret, env);
        ret = clSetKernelArg(kernel[nlmVertical], 1, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(nlmVertical[1])", ret, env);
        // kernel[nlmVertical] -> 2 is set by AviSynthPluginGetFrame
        ret = clSetKernelArg(kernel[nlmVertical], 3, 2 * sizeof(cl_uint), &idmn);
        oclErrorCheck("clSetKernelArg(nlmVertical[3])", ret, env);

        // nlmAccumulation
        ret = clSetKernelArg(kernel[nlmAccumulation], 0, sizeof(cl_mem), &mem_in[0]);
        oclErrorCheck("clSetKernelArg(nlmAccumulation[0])", ret, env);
        ret = clSetKernelArg(kernel[nlmAccumulation], 1, sizeof(cl_mem), &mem_U[0]);
        oclErrorCheck("clSetKernelArg(nlmAccumulation[1])", ret, env);
        ret = clSetKernelArg(kernel[nlmAccumulation], 2, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(nlmAccumulation[2])", ret, env);
        ret = clSetKernelArg(kernel[nlmAccumulation], 3, sizeof(cl_mem), &mem_U[3]);
        oclErrorCheck("clSetKernelArg(nlmAccumulation[3])", ret, env);
        ret = clSetKernelArg(kernel[nlmAccumulation], 4, 2 * sizeof(cl_uint), &idmn);
        oclErrorCheck("clSetKernelArg(nlmAccumulation[4])", ret, env);

        // nlmFinish
        ret = clSetKernelArg(kernel[nlmFinish], 0, sizeof(cl_mem), &mem_in[0]);
        oclErrorCheck("clSetKernelArg(nlmFinish[0])", ret, env);
        ret = clSetKernelArg(kernel[nlmFinish], 1, sizeof(cl_mem), &mem_out);
        oclErrorCheck("clSetKernelArg(nlmFinish[1])", ret, env);
        ret = clSetKernelArg(kernel[nlmFinish], 2, sizeof(cl_mem), &mem_U[0]);
        oclErrorCheck("clSetKernelArg(nlmFinish[2])", ret, env);
        ret = clSetKernelArg(kernel[nlmFinish], 3, sizeof(cl_mem), &mem_U[3]);
        oclErrorCheck("clSetKernelArg(nlmFinish[3])", ret, env);
        ret = clSetKernelArg(kernel[nlmFinish], 4, 2 * sizeof(cl_uint), &idmn);
        oclErrorCheck("clSetKernelArg(nlmFinish[4])", ret, env);

        // nlmPack
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
        ret = clSetKernelArg(kernel[nlmPack], 8, 2 * sizeof(cl_uint), &idmn);
        oclErrorCheck("clSetKernelArg(nlmPack[8])", ret, env);

        // nlmUnpack
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
        ret = clSetKernelArg(kernel[nlmUnpack], 6, sizeof(cl_mem), &mem_out);
        oclErrorCheck("clSetKernelArg(nlmUnpack[6])", ret, env);
        ret = clSetKernelArg(kernel[nlmUnpack], 7, 2 * sizeof(cl_uint), &idmn);
        oclErrorCheck("clSetKernelArg(nlmUnpack[7])", ret, env);
    } else {
        // nlmSpatialDistance
        ret = clSetKernelArg(kernel[nlmSpatialDistance], 0, sizeof(cl_mem), &mem_in[(clip_t & NLM_CLIP_EXTRA_FALSE) ? 0 : 1]);
        oclErrorCheck("clSetKernelArg(nlmSpatialDistance[0])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialDistance], 1, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(nlmSpatialDistance[1])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialDistance], 2, 2 * sizeof(cl_uint), &idmn);
        oclErrorCheck("clSetKernelArg(nlmSpatialDistance[2])", ret, env);

        // nlmSpatialHorizontal
        ret = clSetKernelArg(kernel[nlmSpatialHorizontal], 0, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(nlmSpatialHorizontal[0])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialHorizontal], 1, sizeof(cl_mem), &mem_U[2]);
        oclErrorCheck("clSetKernelArg(nlmSpatialHorizontal[1])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialHorizontal], 2, 2 * sizeof(cl_uint), &idmn);
        oclErrorCheck("clSetKernelArg(nlmSpatialHorizontal[2])", ret, env);

        // nlmSpatialVertical
        ret = clSetKernelArg(kernel[nlmSpatialVertical], 0, sizeof(cl_mem), &mem_U[2]);
        oclErrorCheck("clSetKernelArg(nlmSpatialVertical[0])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialVertical], 1, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(nlmSpatialVertical[1])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialVertical], 2, 2 * sizeof(cl_uint), &idmn);
        oclErrorCheck("clSetKernelArg(nlmSpatialVertical[2])", ret, env);

        // nlmSpatialAccumulation
        ret = clSetKernelArg(kernel[nlmSpatialAccumulation], 0, sizeof(cl_mem), &mem_in[0]);
        oclErrorCheck("clSetKernelArg(nlmSpatialAccumulation[0])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialAccumulation], 1, sizeof(cl_mem), &mem_U[0]);
        oclErrorCheck("clSetKernelArg(nlmSpatialAccumulation[1])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialAccumulation], 2, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(nlmSpatialAccumulation[2])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialAccumulation], 3, sizeof(cl_mem), &mem_U[3]);
        oclErrorCheck("clSetKernelArg(nlmSpatialAccumulation[3])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialAccumulation], 4, 2 * sizeof(cl_uint), &idmn);
        oclErrorCheck("clSetKernelArg(nlmSpatialAccumulation[4])", ret, env);

        // nlmSpatialFinish
        ret = clSetKernelArg(kernel[nlmSpatialFinish], 0, sizeof(cl_mem), &mem_in[0]);
        oclErrorCheck("clSetKernelArg(nlmSpatialFinish[0])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialFinish], 1, sizeof(cl_mem), &mem_out);
        oclErrorCheck("clSetKernelArg(nlmSpatialFinish[1])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialFinish], 2, sizeof(cl_mem), &mem_U[0]);
        oclErrorCheck("clSetKernelArg(nlmSpatialFinish[2])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialFinish], 3, sizeof(cl_mem), &mem_U[3]);
        oclErrorCheck("clSetKernelArg(nlmSpatialFinish[3])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialFinish], 4, 2 * sizeof(cl_uint), &idmn);
        oclErrorCheck("clSetKernelArg(nlmSpatialFinish[4])", ret, env);

        // nlmSpatialPack
        ret = clSetKernelArg(kernel[nlmSpatialPack], 0, sizeof(cl_mem), &mem_P[0]);
        oclErrorCheck("clSetKernelArg(nlmSpatialPack[0])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialPack], 1, sizeof(cl_mem), &mem_P[1]);
        oclErrorCheck("clSetKernelArg(nlmSpatialPack[1])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialPack], 2, sizeof(cl_mem), &mem_P[2]);
        oclErrorCheck("clSetKernelArg(nlmSpatialPack[2])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialPack], 3, sizeof(cl_mem), &mem_P[3]);
        oclErrorCheck("clSetKernelArg(nlmSpatialPack[3])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialPack], 4, sizeof(cl_mem), &mem_P[4]);
        oclErrorCheck("clSetKernelArg(nlmSpatialPack[4])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialPack], 5, sizeof(cl_mem), &mem_P[5]);
        oclErrorCheck("clSetKernelArg(nlmSpatialPack[5])", ret, env);
        // kernel[nlmSpatialPack] -> 6 is set by AviSynthPluginGetFrame 
        ret = clSetKernelArg(kernel[nlmSpatialPack], 7, 2 * sizeof(cl_uint), &idmn);
        oclErrorCheck("clSetKernelArg(nlmSpatialPack[7])", ret, env);

        // nlmSpatialUnpack
        ret = clSetKernelArg(kernel[nlmSpatialUnpack], 0, sizeof(cl_mem), &mem_P[0]);
        oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[0])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialUnpack], 1, sizeof(cl_mem), &mem_P[1]);
        oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[1])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialUnpack], 2, sizeof(cl_mem), &mem_P[2]);
        oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[2])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialUnpack], 3, sizeof(cl_mem), &mem_P[3]);
        oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[3])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialUnpack], 4, sizeof(cl_mem), &mem_P[4]);
        oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[4])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialUnpack], 5, sizeof(cl_mem), &mem_P[5]);
        oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[5])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialUnpack], 6, sizeof(cl_mem), &mem_out);
        oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[6])", ret, env);
        ret = clSetKernelArg(kernel[nlmSpatialUnpack], 7, 2 * sizeof(cl_uint), &idmn);
        oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[7])", ret, env);
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
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame ref = (baby) ? baby->GetFrame(n, env) : nullptr;
    PVideoFrame dst = env->NewVideoFrame(vi);
    int maxframe = vi.num_frames - 1;
    size_t size_u0 = sizeof(cl_float) * idmn[0] * idmn[1] * channel_num;
    size_t size_u3 = sizeof(cl_float) * idmn[0] * idmn[1];
    const cl_int t = d;
    const cl_float pattern_u0 = 0.0f;
    const cl_float pattern_u3 = CL_FLT_EPSILON;
    const size_t origin[3] = { 0, 0, 0 };
    const size_t region[3] = { idmn[0], idmn[1], 1 };
    const size_t global_work[2] = { mrounds(idmn[0], 16), mrounds(idmn[1], 16) };
    const size_t global_work_hrz[2] = {
        mrounds(idmn[0], HRZ_RESULT * HRZ_BLOCK_X) / HRZ_RESULT,
        mrounds(idmn[1], HRZ_BLOCK_Y)
    };
    const size_t global_work_vrt[2] = {
        mrounds(idmn[0], VRT_BLOCK_X),
        mrounds(idmn[1], VRT_RESULT * VRT_BLOCK_X) / VRT_RESULT
    };
    const size_t local_work_dst[2] = { 16, 16 };
    const size_t local_work_hrz[2] = { HRZ_BLOCK_X, HRZ_BLOCK_Y };
    const size_t local_work_vrt[2] = { VRT_BLOCK_X, VRT_BLOCK_Y };

    // Copy other data   
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
            for (int x = 0; x < src->GetRowSize(); x+=4) {
                *(dstp + x + 3) = *(srcp + x + 3);
            }
        }
        srcp += src->GetPitch();
        dstp += dst->GetPitch();
    }

    // Set-up buffers
    cl_int ret = CL_SUCCESS;
    ret |= clEnqueueFillBuffer(command_queue, mem_U[0], &pattern_u0, sizeof(cl_float), 0, size_u0, 0, NULL, NULL);
    ret |= clEnqueueFillBuffer(command_queue, mem_U[3], &pattern_u3, sizeof(cl_float), 0, size_u3, 0, NULL, NULL);
    if (d) {
        // Spatio-temporal processing
        /*for (int k = -d; k <= d; k++) {
            src = child->GetFrame(clamp(n + k, 0, maxframe), env);
            ref = (baby) ? baby->GetFrame(clamp(n + k, 0, maxframe), env) : nullptr;
            const cl_int t_pk = t + k;
            const size_t origin_in[3] = { 0, 0, (size_t) t_pk };
            switch (clip_t) {
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                    ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_FALSE, origin_in, region,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                    ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_FALSE, origin_in, region,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_in[1], CL_FALSE, origin_in, region,
                        (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_LUMA):
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_Y),
                        0, src->GetReadPtr(PLANAR_Y) + src->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_in[0]);
                    ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_LUMA):
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_Y),
                        0, src->GetReadPtr(PLANAR_Y) + src->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_in[0]);
                    ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                        (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) ref->GetPitch(PLANAR_Y),
                        0, ref->GetReadPtr(PLANAR_Y) + ref->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_in[1]);
                    ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA):
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_in[0]);
                    ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA):
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_in[0]);
                    ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_in[1]);
                    ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_CHROMA):
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_U),
                        0, src->GetReadPtr(PLANAR_U) + src->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[4], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_V),
                        0, src->GetReadPtr(PLANAR_V) + src->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_in[0]);
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
                    ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_in[0]);
                    ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_U),
                        0, src->GetReadPtr(PLANAR_U) + src->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[4], CL_FALSE, origin, region, (size_t) src->GetPitch(PLANAR_V),
                        0, src->GetReadPtr(PLANAR_V) + src->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_in[1]);
                    ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_in[0]);
                    ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_FALSE, origin, region,
                        (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_in[0]);
                    ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_FALSE, origin, region,
                        (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_FALSE, origin, region,
                        (size_t) ref->GetPitch(PLANAR_U), 0, ref->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_FALSE, origin, region,
                        (size_t) ref->GetPitch(PLANAR_V), 0, ref->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_in[1]);
                    ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
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
                    ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_in[0]);
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
                    ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_in[0]);
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
                    ret |= clSetKernelArg(kernel[nlmPack], 6, sizeof(cl_mem), &mem_in[1]);
                    ret |= clSetKernelArg(kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                    ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_FALSE, origin_in, region,
                        (size_t) src->GetPitch(), 0, src->GetReadPtr(), 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                    ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_FALSE, origin_in, region,
                        (size_t) src->GetPitch(), 0, src->GetReadPtr(), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_in[1], CL_FALSE, origin_in, region,
                        (size_t) ref->GetPitch(), 0, ref->GetReadPtr(), 0, NULL, NULL);
                    break;
                default:
                    env->ThrowError("KNLMeansCL: clip_t error!\n (AviSynthGetFrame)");
                    break;
            }
        }*/
        for (int k = -d; k <= 0; k++) {
            for (int j = -a; j <= a; j++) {
                for (int i = -a; i <= a; i++) {
                    if (k * (2 * a + 1) * (2 * a + 1) + j * (2 * a + 1) + i < 0) {
                        const cl_int q[4] = { i, j, k, 0 };
                        /*ret |= clSetKernelArg(kernel[nlmDistanceLeft], 3, 4 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmDistanceLeft],
                            2, NULL, global_work, local_work_dst, 0, NULL, NULL);
                        ret |= clSetKernelArg(kernel[nlmHorizontal], 2, sizeof(cl_int), &t);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmHorizontal],
                            2, NULL, global_work_hrz, local_work_hrz, 0, NULL, NULL);
                        ret |= clSetKernelArg(kernel[nlmVertical], 2, sizeof(cl_int), &t);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmVertical],
                            2, NULL, global_work_vrt, local_work_vrt, 0, NULL, NULL);
                        if (k) {
                            const cl_int t_mq = t - k;
                            /*ret |= clSetKernelArg(kernel[nlmDistanceRight], 3, 4 * sizeof(cl_int), &q);
                            ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmDistanceRight],
                                2, NULL, global_work, local_work_dst, 0, NULL, NULL);
                            ret |= clSetKernelArg(kernel[nlmHorizontal], 2, sizeof(cl_int), &t_mq);
                            ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmHorizontal],
                                2, NULL, global_work_hrz, local_work_hrz, 0, NULL, NULL);
                            ret |= clSetKernelArg(kernel[nlmVertical], 2, sizeof(cl_int), &t_mq);
                            ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmVertical],
                                2, NULL, global_work_vrt, local_work_vrt, 0, NULL, NULL);
                        }*/
                        ret |= clSetKernelArg(kernel[nlmAccumulation], 5, 4 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmAccumulation],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                    }
                }
            }
        }
        /*ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmFinish], 2, NULL, global_work, NULL, 0, NULL, NULL);
        switch (clip_t) {
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                ret |= clEnqueueReadImage(command_queue, mem_out, CL_FALSE, origin, region,
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
                ret |= clEnqueueReadImage(command_queue, mem_out, CL_TRUE, origin, region,
                    (size_t) dst->GetPitch(), 0, dst->GetWritePtr(), 0, NULL, NULL);
                break;
            default:
                env->ThrowError("KNLMeansCL: clip_t error!\n (AviSynthGetFrame)");
                break;
        }*/
    } else {
        // Spatial processing
        switch (clip_t) {
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_in[1], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_LUMA):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 6, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_LUMA):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 6, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 6, sizeof(cl_mem), &mem_in[1]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 6, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 6, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_U), 0, ref->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_V), 0, ref->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 6, sizeof(cl_mem), &mem_in[1]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_CHROMA):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_TRUE, origin, region, (size_t) src->GetPitch(PLANAR_U),
                    0, src->GetReadPtr(PLANAR_U) + src->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[4], CL_TRUE, origin, region, (size_t) src->GetPitch(PLANAR_V),
                    0, src->GetReadPtr(PLANAR_V) + src->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 6, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_CHROMA):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_TRUE, origin, region, (size_t) src->GetPitch(PLANAR_U),
                    0, src->GetReadPtr(PLANAR_U) + src->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[4], CL_TRUE, origin, region, (size_t) src->GetPitch(PLANAR_V),
                    0, src->GetReadPtr(PLANAR_V) + src->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 6, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_U), 0, ref->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_V), 0, ref->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_TRUE, origin, region, (size_t) ref->GetPitch(PLANAR_U),
                    0, ref->GetReadPtr(PLANAR_U) + ref->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[4], CL_TRUE, origin, region, (size_t) ref->GetPitch(PLANAR_V),
                    0, ref->GetReadPtr(PLANAR_V) + ref->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 6, sizeof(cl_mem), &mem_in[1]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 6, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 6, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_U), 0, ref->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_V), 0, ref->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 6, sizeof(cl_mem), &mem_in[1]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_YUV):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_TRUE, origin, region, (size_t) src->GetPitch(PLANAR_Y),
                    0, src->GetReadPtr(PLANAR_Y) + src->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[4], CL_TRUE, origin, region, (size_t) src->GetPitch(PLANAR_U),
                    0, src->GetReadPtr(PLANAR_U) + src->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[5], CL_TRUE, origin, region, (size_t) src->GetPitch(PLANAR_V),
                    0, src->GetReadPtr(PLANAR_V) + src->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 6, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_YUV):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_TRUE, origin, region, (size_t) src->GetPitch(PLANAR_Y),
                    0, src->GetReadPtr(PLANAR_Y) + src->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[4], CL_TRUE, origin, region, (size_t) src->GetPitch(PLANAR_U),
                    0, src->GetReadPtr(PLANAR_U) + src->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[5], CL_TRUE, origin, region, (size_t) src->GetPitch(PLANAR_V),
                    0, src->GetReadPtr(PLANAR_V) + src->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 6, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_U), 0, ref->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_V), 0, ref->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[3], CL_TRUE, origin, region, (size_t) ref->GetPitch(PLANAR_Y),
                    0, ref->GetReadPtr(PLANAR_Y) + ref->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[4], CL_TRUE, origin, region, (size_t) ref->GetPitch(PLANAR_U),
                    0, ref->GetReadPtr(PLANAR_U) + ref->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[5], CL_TRUE, origin, region, (size_t) ref->GetPitch(PLANAR_V),
                    0, ref->GetReadPtr(PLANAR_V) + ref->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 6, sizeof(cl_mem), &mem_in[1]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(), 0, src->GetReadPtr(), 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(), 0, src->GetReadPtr(), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_in[1], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(), 0, ref->GetReadPtr(), 0, NULL, NULL);
                break;
            default:
                env->ThrowError("KNLMeansCL: clip_t error!\n (AviSynthGetFrame)");
                break;
        }
        for (int j = -a; j <= 0; j++) {
            for (int i = -a; i <= a; i++) {
                if (j * (2 * a + 1) + i < 0) {
                    const cl_int q[2] = { i, j };
                    ret |= clSetKernelArg(kernel[nlmSpatialDistance], 3, 2 * sizeof(cl_int), &q);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialDistance],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialHorizontal],
                        2, NULL, global_work_hrz, local_work_hrz, 0, NULL, NULL);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialVertical],
                        2, NULL, global_work_vrt, local_work_vrt, 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmSpatialAccumulation], 5, 2 * sizeof(cl_int), &q);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialAccumulation],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                }
            }
        }
        ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialFinish], 2, NULL, global_work, NULL, 0, NULL, NULL);
        switch (clip_t) {
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                ret |= clEnqueueReadImage(command_queue, mem_out, CL_TRUE, origin, region,
                    (size_t) dst->GetPitch(PLANAR_Y), 0, dst->GetWritePtr(PLANAR_Y), 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_LUMA):
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_LUMA):
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialUnpack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) dst->GetPitch(PLANAR_Y), 0, dst->GetWritePtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[3], CL_TRUE, origin, region, (size_t) dst->GetPitch(PLANAR_Y),
                    0, dst->GetWritePtr(PLANAR_Y) + dst->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA):
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA):
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialUnpack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) dst->GetPitch(PLANAR_U), 0, dst->GetWritePtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) dst->GetPitch(PLANAR_V), 0, dst->GetWritePtr(PLANAR_V), 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_CHROMA):
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_CHROMA):
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialUnpack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) dst->GetPitch(PLANAR_U), 0, dst->GetWritePtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) dst->GetPitch(PLANAR_V), 0, dst->GetWritePtr(PLANAR_V), 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[3], CL_TRUE, origin, region, (size_t) dst->GetPitch(PLANAR_U),
                    0, dst->GetWritePtr(PLANAR_U) + dst->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[4], CL_TRUE, origin, region, (size_t) dst->GetPitch(PLANAR_V),
                    0, dst->GetWritePtr(PLANAR_V) + dst->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialUnpack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) dst->GetPitch(PLANAR_Y), 0, dst->GetWritePtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) dst->GetPitch(PLANAR_U), 0, dst->GetWritePtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[2], CL_TRUE, origin, region,
                    (size_t) dst->GetPitch(PLANAR_V), 0, dst->GetWritePtr(PLANAR_V), 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_YUV):
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_YUV):
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialUnpack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[0], CL_TRUE, origin, region,
                    (size_t) dst->GetPitch(PLANAR_Y), 0, dst->GetWritePtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[1], CL_TRUE, origin, region,
                    (size_t) dst->GetPitch(PLANAR_U), 0, dst->GetWritePtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[2], CL_TRUE, origin, region,
                    (size_t) dst->GetPitch(PLANAR_V), 0, dst->GetWritePtr(PLANAR_V), 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[3], CL_TRUE, origin, region, (size_t) dst->GetPitch(PLANAR_Y),
                    0, dst->GetWritePtr(PLANAR_Y) + dst->GetPitch(PLANAR_Y) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[4], CL_TRUE, origin, region, (size_t) dst->GetPitch(PLANAR_U),
                    0, dst->GetWritePtr(PLANAR_U) + dst->GetPitch(PLANAR_U) * idmn[1], 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, mem_P[5], CL_TRUE, origin, region, (size_t) dst->GetPitch(PLANAR_V),
                    0, dst->GetWritePtr(PLANAR_V) + dst->GetPitch(PLANAR_V) * idmn[1], 0, NULL, NULL);
                break;
            case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
            case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                ret |= clEnqueueReadImage(command_queue, mem_out, CL_TRUE, origin, region,
                    (size_t) dst->GetPitch(), 0, dst->GetWritePtr(), 0, NULL, NULL);
                break;
            default:
                env->ThrowError("KNLMeansCL: clip_t error!\n (AviSynthGetFrame)");
                break;
        }
    }
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
        snprintf(buffer, 2048, " Search window: %ix%ix%i", 2 * a + 1, 2 * a + 1, 2 * d + 1);
        DrawString(frm, pitch, 0, y++, buffer);
        snprintf(buffer, 2048, " Similarity neighborhood: %ix%i", 2 * s + 1, 2 * s + 1);
        DrawString(frm, pitch, 0, y++, buffer);
        snprintf(buffer, 2048, " Num of ref pixels: %i", (2 * a + 1)*(2 * a + 1)*(2 * d + 1) - 1);
        DrawString(frm, pitch, 0, y++, buffer);
        snprintf(buffer, 2048, " Global work size: %zux%zu - %zux%zu - %zux%zu",
            global_work[0], global_work[1], global_work_hrz[0], global_work_hrz[1], global_work_vrt[0], global_work_vrt[1]);
        DrawString(frm, pitch, 0, y++, buffer);
        snprintf(buffer, 2048, " Local work size: %ux%u - %ux%u", HRZ_BLOCK_X, HRZ_BLOCK_Y, VRT_BLOCK_X, VRT_BLOCK_Y);       
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
    const int maxframe = d->vi->numFrames - 1;
    if (activationReason == arInitial) {
        for (int k = int64ToIntS(-d->d); k <= d->d; k++) {
            vsapi->requestFrameFilter(clamp(n + k, 0, maxframe), d->node, frameCtx);
            if (d->knot) vsapi->requestFrameFilter(clamp(n + k, 0, maxframe), d->knot, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        // Variables
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx), *ref;
        const VSFormat *fi = d->vi->format;
        VSFrameRef *dst;
        const cl_int t = int64ToIntS(d->d);
        const cl_float pattern_u0 = 0.0f;
        const cl_float pattern_u3 = CL_FLT_EPSILON;
        const size_t size_u0 = sizeof(cl_float) * d->idmn[0] * d->idmn[1] * ((d->clip_t & NLM_CLIP_REF_LUMA) ? 2 : 4);
        const size_t size_u3 = sizeof(cl_float) * d->idmn[0] * d->idmn[1];
        const size_t origin[3] = { 0, 0, 0 };
        const size_t region[3] = { d->idmn[0], d->idmn[1], 1 };
        const size_t global_work[2] = { mrounds(d->idmn[0], 16), mrounds(d->idmn[1], 8) };
        const size_t global_work_hrz[2] = {
            mrounds(d->idmn[0], HRZ_RESULT * HRZ_BLOCK_X) / HRZ_RESULT,
            mrounds(d->idmn[1], HRZ_BLOCK_Y)
        };
        const size_t global_work_vrt[2] = {
            mrounds(d->idmn[0], VRT_BLOCK_X),
            mrounds(d->idmn[1], VRT_RESULT * VRT_BLOCK_X) / VRT_RESULT
        };
        const size_t local_work_hrz[2] = { HRZ_BLOCK_X, HRZ_BLOCK_Y };
        const size_t local_work_vrt[2] = { VRT_BLOCK_X, VRT_BLOCK_Y };

        // Copy other data
        if (fi->colorFamily != cmGray && (d->clip_t & NLM_CLIP_REF_LUMA)) {
            const VSFrameRef * planeSrc[] = { NULL, src, src };
            const int planes[] = { 0, 1, 2 };
            dst = vsapi->newVideoFrame2(fi, (int) d->idmn[0], (int) d->idmn[1], planeSrc, planes, src, core);
        } else if (d->clip_t & NLM_CLIP_REF_CHROMA) {
            const VSFrameRef * planeSrc[] = { src, NULL, NULL };
            const int planes[] = { 0, 1, 2 };
            dst = vsapi->newVideoFrame2(fi, (int) d->idmn[0], (int) d->idmn[1], planeSrc, planes, src, core);
        } else {
            dst = vsapi->newVideoFrame(fi, (int) d->idmn[0], (int) d->idmn[1], src, core);
        }
        vsapi->freeFrame(src);

        // Set-up buffers
        cl_int ret = CL_SUCCESS;
        cl_command_queue command_queue = clCreateCommandQueue(d->context, d->deviceID, 0, NULL);
        ret |= clEnqueueFillBuffer(command_queue, d->mem_U[0], &pattern_u0, sizeof(cl_float), 0, size_u0, 0, NULL, NULL);
        ret |= clEnqueueFillBuffer(command_queue, d->mem_U[3], &pattern_u3, sizeof(cl_float), 0, size_u3, 0, NULL, NULL);
        if (d->d) {
            // Spatio-temporal processing
            for (int k = int64ToIntS(-d->d); k <= d->d; k++) {
                src = vsapi->getFrameFilter(clamp(n + k, 0, maxframe), d->node, frameCtx);
                ref = (d->knot) ? vsapi->getFrameFilter(clamp(n + k, 0, maxframe), d->knot, frameCtx) : nullptr;
                const cl_int t_pk = t + k;
                const size_t origin_in[3] = { 0, 0, (size_t) t_pk };
                switch (d->clip_t) {
                    case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                        ret |= clEnqueueWriteImage(command_queue, d->mem_in[0], CL_TRUE, origin_in, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        break;
                    case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                        ret |= clEnqueueWriteImage(command_queue, d->mem_in[0], CL_TRUE, origin_in, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_in[1], CL_TRUE, origin_in, region,
                            (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                        break;
                    case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_LUMA):
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 6, sizeof(cl_mem), &d->mem_in[0]);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmPack],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        break;
                    case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_LUMA):
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 6, sizeof(cl_mem), &d->mem_in[0]);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmPack],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 6, sizeof(cl_mem), &d->mem_in[1]);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmPack],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        break;
                    case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
                    case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
                    case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                    case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB):
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 6, sizeof(cl_mem), &d->mem_in[0]);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmPack],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        break;
                    case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
                    case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
                    case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                    case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB):
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 6, sizeof(cl_mem), &d->mem_in[0]);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmPack],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 1), 0, vsapi->getReadPtr(ref, 1), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 2), 0, vsapi->getReadPtr(ref, 2), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 6, sizeof(cl_mem), &d->mem_in[1]);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 7, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmPack],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        break;
                    default:
                        vsapi->setFilterError("knlm.KNLMeansCL: fatal error!\n (VapourSynthGetFrame)", frameCtx);
                        vsapi->freeFrame(dst);
                        return 0;
                }
                vsapi->freeFrame(src);
                vsapi->freeFrame(ref);
            }
            for (int k = int64ToIntS(-d->d); k <= 0; k++) {
                for (int j = int64ToIntS(-d->a); j <= d->a; j++) {
                    for (int i = int64ToIntS(-d->a); i <= d->a; i++) {
                        if (k * (2 * int64ToIntS(d->a) + 1) * (2 * int64ToIntS(d->a) + 1) +
                            j * (2 * int64ToIntS(d->a) + 1) + i < 0) {

                            const cl_int q[4] = { i, j, k, 0 };
                            ret |= clSetKernelArg(d->kernel[nlmDistanceLeft], 3, 4 * sizeof(cl_int), &q);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmDistanceLeft],
                                2, NULL, global_work, NULL, 0, NULL, NULL);
                            ret |= clSetKernelArg(d->kernel[nlmHorizontal], 2, sizeof(cl_int), &t);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmHorizontal],
                                2, NULL, global_work_hrz, local_work_hrz, 0, NULL, NULL);
                            ret |= clSetKernelArg(d->kernel[nlmVertical], 2, sizeof(cl_int), &t);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmVertical],
                                2, NULL, global_work_vrt, local_work_vrt, 0, NULL, NULL);
                            if (k) {
                                const cl_int t_mq = t - k;
                                ret |= clSetKernelArg(d->kernel[nlmDistanceRight], 3, 4 * sizeof(cl_int), &q);
                                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmDistanceRight],
                                    2, NULL, global_work, NULL, 0, NULL, NULL);
                                ret |= clSetKernelArg(d->kernel[nlmHorizontal], 2, sizeof(cl_int), &t_mq);
                                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmHorizontal],
                                    2, NULL, global_work_hrz, local_work_hrz, 0, NULL, NULL);
                                ret |= clSetKernelArg(d->kernel[nlmVertical], 2, sizeof(cl_int), &t_mq);
                                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmVertical],
                                    2, NULL, global_work_vrt, local_work_vrt, 0, NULL, NULL);
                            }
                            ret |= clSetKernelArg(d->kernel[nlmAccumulation], 5, 4 * sizeof(cl_int), &q);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmAccumulation],
                                2, NULL, global_work, NULL, 0, NULL, NULL);
                        }
                    }
                }
            }
            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmFinish], 2, NULL, global_work, NULL, 0, NULL, NULL);
            switch (d->clip_t) {
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                    ret |= clEnqueueReadImage(command_queue, d->mem_out, CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(dst, 0), 0, vsapi->getWritePtr(dst, 0), 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_LUMA):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_LUMA):
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmUnpack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueReadImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(dst, 0), 0, vsapi->getWritePtr(dst, 0), 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB):
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmUnpack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueReadImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(dst, 0), 0, vsapi->getWritePtr(dst, 0), 0, NULL, NULL);
                    ret |= clEnqueueReadImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(dst, 1), 0, vsapi->getWritePtr(dst, 1), 0, NULL, NULL);
                    ret |= clEnqueueReadImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(dst, 2), 0, vsapi->getWritePtr(dst, 2), 0, NULL, NULL);
                    break;
                default:
                    vsapi->setFilterError("knlm.KNLMeansCL: fatal error!\n (VapourSynthGetFrame)", frameCtx);
                    vsapi->freeFrame(dst);
                    return 0;
            }
        } else {
            // Spatial processing
            src = vsapi->getFrameFilter(n, d->node, frameCtx);
            ref = (d->knot) ? vsapi->getFrameFilter(n, d->knot, frameCtx) : nullptr;
            switch (d->clip_t) {
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                    ret |= clEnqueueWriteImage(command_queue, d->mem_in[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                    ret |= clEnqueueWriteImage(command_queue, d->mem_in[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_in[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_LUMA):
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmSpatialPack], 6, sizeof(cl_mem), &d->mem_in[0]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_LUMA):
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmSpatialPack], 6, sizeof(cl_mem), &d->mem_in[0]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmSpatialPack], 6, sizeof(cl_mem), &d->mem_in[1]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB):
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmSpatialPack], 6, sizeof(cl_mem), &d->mem_in[0]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB):
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmSpatialPack], 6, sizeof(cl_mem), &d->mem_in[0]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(ref, 1), 0, vsapi->getReadPtr(ref, 1), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(ref, 2), 0, vsapi->getReadPtr(ref, 2), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmSpatialPack], 6, sizeof(cl_mem), &d->mem_in[1]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                default:
                    vsapi->setFilterError("knlm.KNLMeansCL: fatal error!\n (VapourSynthGetFrame)", frameCtx);
                    vsapi->freeFrame(dst);
                    return 0;
            }
            vsapi->freeFrame(src);
            vsapi->freeFrame(ref);
            for (int j = int64ToIntS(-d->a); j <= 0; j++) {
                for (int i = int64ToIntS(-d->a); i <= d->a; i++) {
                    if (j * (2 * int64ToIntS(d->a) + 1) + i < 0) {
                        const cl_int q[2] = { i, j };
                        ret |= clSetKernelArg(d->kernel[nlmSpatialDistance], 3, 2 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialDistance],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialHorizontal],
                            2, NULL, global_work_hrz, local_work_hrz, 0, NULL, NULL);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialVertical],
                            2, NULL, global_work_vrt, local_work_vrt, 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[nlmSpatialAccumulation], 5, 2 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialAccumulation],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                    }
                }
            }
            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialFinish], 2, NULL, global_work, NULL, 0, NULL, NULL);
            switch (d->clip_t) {
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_LUMA):
                    ret |= clEnqueueReadImage(command_queue, d->mem_out, CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(dst, 0), 0, vsapi->getWritePtr(dst, 0), 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_LUMA):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_LUMA):
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialUnpack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueReadImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(dst, 0), 0, vsapi->getWritePtr(dst, 0), 0, NULL, NULL);
                    break;
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV):
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB):
                case (NLM_CLIP_EXTRA_FALSE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB):
                case (NLM_CLIP_EXTRA_TRUE | NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB):
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialUnpack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueReadImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(dst, 0), 0, vsapi->getWritePtr(dst, 0), 0, NULL, NULL);
                    ret |= clEnqueueReadImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(dst, 1), 0, vsapi->getWritePtr(dst, 1), 0, NULL, NULL);
                    ret |= clEnqueueReadImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(dst, 2), 0, vsapi->getWritePtr(dst, 2), 0, NULL, NULL);
                    break;
                default:
                    vsapi->setFilterError("knlm.KNLMeansCL: fatal error!\n (VapourSynthGetFrame)", frameCtx);
                    vsapi->freeFrame(dst);
                    return 0;
            }
        }
        ret |= clFinish(command_queue);
        ret |= clReleaseCommandQueue(command_queue);
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
            snprintf(buffer, 2048, " Search window: %" PRId64 "x%" PRId64 "x%" PRId64,
                2 * d->a + 1, 2 * d->a + 1, 2 * d->d + 1);
            DrawString(frm, pitch, 0, y++, buffer);
            snprintf(buffer, 2048, " Similarity neighborhood: %" PRId64 "x%" PRId64, 2 * d->s + 1, 2 * d->s + 1);
            DrawString(frm, pitch, 0, y++, buffer);
            snprintf(buffer, 2048, " Num of ref pixels: %" PRId64, (2 * d->a + 1)*(2 * d->a + 1)*(2 * d->d + 1) - 1);
            DrawString(frm, pitch, 0, y++, buffer);
            snprintf(buffer, 2048, " Global work size: %zux%zu", global_work[0], global_work[1]);
            DrawString(frm, pitch, 0, y++, buffer);
            snprintf(buffer, 2048, " Local work size: %ux%u - %ux%u",
                HRZ_BLOCK_X, HRZ_BLOCK_Y, VRT_BLOCK_X, VRT_BLOCK_Y);
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
    clReleaseMemObject(mem_P[5]);
    clReleaseMemObject(mem_P[4]);
    clReleaseMemObject(mem_P[3]);
    clReleaseMemObject(mem_P[2]);
    clReleaseMemObject(mem_P[1]);
    clReleaseMemObject(mem_P[0]);
    clReleaseMemObject(mem_U[3]);
    clReleaseMemObject(mem_U[2]);
    clReleaseMemObject(mem_U[1]);
    clReleaseMemObject(mem_U[0]);
    clReleaseMemObject(mem_out);
    clReleaseMemObject(mem_in[1]);
    clReleaseMemObject(mem_in[0]);
    if (d) {
        clReleaseKernel(kernel[nlmUnpack]);
        clReleaseKernel(kernel[nlmPack]);
        clReleaseKernel(kernel[nlmFinish]);
        clReleaseKernel(kernel[nlmAccumulation]);
        clReleaseKernel(kernel[nlmVertical]);
        clReleaseKernel(kernel[nlmHorizontal]);
        clReleaseKernel(kernel[nlmDistanceRight]);
        clReleaseKernel(kernel[nlmDistanceLeft]);
    } else {
        clReleaseKernel(kernel[nlmSpatialUnpack]);
        clReleaseKernel(kernel[nlmSpatialPack]);
        clReleaseKernel(kernel[nlmSpatialFinish]);
        clReleaseKernel(kernel[nlmSpatialAccumulation]);
        clReleaseKernel(kernel[nlmSpatialVertical]);
        clReleaseKernel(kernel[nlmSpatialHorizontal]);
        clReleaseKernel(kernel[nlmSpatialDistance]);
    }
    clReleaseProgram(program);
    clReleaseCommandQueue(command_queue);
    clReleaseContext(context);
}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthFree
#ifdef VAPOURSYNTH_H

static void VS_CC VapourSynthPluginFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    NLMVapoursynth *d = (NLMVapoursynth*) instanceData;
    vsapi->freeNode(d->node);
    vsapi->freeNode(d->knot);
    // d->mem_P[5] is only required to AviSynth
    // d->mem_P[4] is only required to AviSynth
    // d->mem_P[3] is only required to AviSynth
    clReleaseMemObject(d->mem_P[2]);
    clReleaseMemObject(d->mem_P[1]);
    clReleaseMemObject(d->mem_P[0]);
    clReleaseMemObject(d->mem_U[3]);
    clReleaseMemObject(d->mem_U[2]);
    clReleaseMemObject(d->mem_U[1]);
    clReleaseMemObject(d->mem_U[0]);
    clReleaseMemObject(d->mem_out);
    clReleaseMemObject(d->mem_in[1]);
    clReleaseMemObject(d->mem_in[0]);
    if (d->d) {
        clReleaseKernel(d->kernel[nlmUnpack]);
        clReleaseKernel(d->kernel[nlmPack]);
        clReleaseKernel(d->kernel[nlmFinish]);
        clReleaseKernel(d->kernel[nlmAccumulation]);
        clReleaseKernel(d->kernel[nlmVertical]);
        clReleaseKernel(d->kernel[nlmHorizontal]);
        clReleaseKernel(d->kernel[nlmDistanceRight]);
        clReleaseKernel(d->kernel[nlmDistanceLeft]);
    } else {
        clReleaseKernel(d->kernel[nlmSpatialUnpack]);
        clReleaseKernel(d->kernel[nlmSpatialPack]);
        clReleaseKernel(d->kernel[nlmSpatialFinish]);
        clReleaseKernel(d->kernel[nlmSpatialAccumulation]);
        clReleaseKernel(d->kernel[nlmSpatialVertical]);
        clReleaseKernel(d->kernel[nlmSpatialHorizontal]);
        clReleaseKernel(d->kernel[nlmSpatialDistance]);
    }
    clReleaseProgram(d->program);
    clReleaseContext(d->context);
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
        args[11].AsBool(DFT_lsb), args[12].AsBool(DFT_info), env);
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
    if (err) {
        d.knot = nullptr;
        d.clip_t = NLM_CLIP_EXTRA_FALSE;
    } else d.clip_t = NLM_CLIP_EXTRA_TRUE;
    if (d.knot && !d.equals(d.vi, vsapi->getVideoInfo(d.knot))) {
        vsapi->setError(out, "knlm.KNLMeansCL: 'rclip' does not match the source clip!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
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
    if (d.s < 0 || d.s > 4) {
        vsapi->setError(out, "knlm.KNLMeansCL: 's' must be in range [0, 4]!");
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
    d.vi = vsapi->getVideoInfo(d.node);
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
                vsapi->setError(out, "knlm.KNLMeansCL: 'channels' must be 'RGB' with YUV color space!");
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
    if (d.info && d.vi->format->bitsPerSample != 8) {
        vsapi->setError(out, "knlm.KNLMeansCL: 'info' requires Gray8 or YUVP8 color space!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }

    // Set image dimensions
    d.idmn[0] = (cl_uint) d.vi->width;
    d.idmn[1] = (cl_uint) d.vi->height;

    // Set clip_t, channel_order and channel_type
    cl_channel_order channel_order;
    cl_channel_type channel_type;
    if (!strcasecmp(d.channels, "YUV")) {
        d.clip_t |= NLM_CLIP_REF_YUV;
        channel_order = CL_RGBA;
        d.channel_num = 4; /* 3 + buffer */
    } else if (!strcasecmp(d.channels, "Y")) {
        d.clip_t |= NLM_CLIP_REF_LUMA;
        channel_order = CL_R;
        d.channel_num = 2; /* 1 + buffer */
    } else if (!strcasecmp(d.channels, "UV")) {
        d.clip_t |= NLM_CLIP_REF_CHROMA;
        channel_order = CL_RG;
        d.channel_num = 4; /* 2 + padding + buffer */
    } else if (!strcasecmp(d.channels, "RGB")) {
        d.clip_t |= NLM_CLIP_REF_RGB;
        channel_order = CL_RGBA;
        d.channel_num = 4; /* 3 + buffer */
    } else {
        if (d.vi->format->colorFamily == VSColorFamily::cmRGB) {
            d.clip_t |= NLM_CLIP_REF_RGB;
            channel_order = CL_RGBA;
            d.channel_num = 4; /* 3 + buffer */
        } else {
            d.clip_t |= NLM_CLIP_REF_LUMA;
            channel_order = CL_R;
            d.channel_num = 2; /* 1 + buffer */
        }
    }
    if (d.vi->format->bitsPerSample == 8) {
        d.clip_t |= NLM_CLIP_TYPE_UNORM;
        channel_type = CL_UNORM_INT8;
    } else if (d.vi->format->bitsPerSample == 16) {
        d.clip_t |= NLM_CLIP_TYPE_UNORM;
        channel_type = CL_UNORM_INT16;
    } else {
        d.clip_t |= NLM_CLIP_TYPE_UNSIGNED;
        channel_type = CL_UNSIGNED_INT16;
    }

    // Get platformID, deviceID, deviceTYPE and deviceIMAGE2DARRAY
    cl_int ret = oclUtilsGetPlaformDeviceIDs(ocl_device_type, (cl_uint) d.ocl_id, &d.platformID, &d.deviceID);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("oclUtilsGetPlaformDeviceIDs", ret, out, vsapi); return; }

    // Create an OpenCL context
    d.context = clCreateContext(NULL, 1, &d.deviceID, NULL, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateContext", ret, out, vsapi); return; }

    // Create mem_in[] and mem_out
    const cl_image_format image_format = { channel_order, channel_type };
    const cl_image_desc desc_in = { (cl_mem_object_type) (d.d ? CL_MEM_OBJECT_IMAGE2D_ARRAY : CL_MEM_OBJECT_IMAGE2D),
        d.idmn[0], d.idmn[1], 1, 2 * (size_t) d.d + 1, 0, 0, 0, 0, NULL };
    const cl_image_desc desc_out = { CL_MEM_OBJECT_IMAGE2D, d.idmn[0], d.idmn[1], 1, 1, 0, 0, 0, 0, NULL };
    if ((d.clip_t & NLM_CLIP_REF_LUMA) && (d.vi->format->bitsPerSample != 9) && (d.vi->format->bitsPerSample != 10)) {
        d.mem_in[0] = clCreateImage(d.context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, &image_format, &desc_in, NULL, &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_in[0])", ret, out, vsapi);  return; }
        d.mem_in[1] = clCreateImage(d.context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, &image_format, &desc_in, NULL, &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_in[1])", ret, out, vsapi);  return; }
        d.mem_out = clCreateImage(d.context, CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY, &image_format, &desc_out, NULL, &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_out)", ret, out, vsapi);  return; }
    } else {
        d.mem_in[0] = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format, &desc_in, NULL, &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_in[0])", ret, out, vsapi);  return; }
        d.mem_in[1] = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format, &desc_in, NULL, &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_in[1])", ret, out, vsapi);  return; }
        d.mem_out = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format, &desc_out, NULL, &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_out)", ret, out, vsapi);  return; }
    }

    // Create mem_U[]
    size_t size_u0 = sizeof(cl_float) * d.idmn[0] * d.idmn[1] * d.channel_num;
    size_t size_u3 = sizeof(cl_float) * d.idmn[0] * d.idmn[1];
    const cl_image_format format_u = { CL_R, CL_FLOAT };
    d.mem_U[0] = clCreateBuffer(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u0, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateBuffer(d.mem_U[0])", ret, out, vsapi); return; }
    d.mem_U[1] = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &format_u, &desc_in, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_U[1])", ret, out, vsapi); return; }
    d.mem_U[2] = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &format_u, &desc_in, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_U[2])", ret, out, vsapi); return; }
    d.mem_U[3] = clCreateBuffer(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u3, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateBuffer(d.mem_U[3])", ret, out, vsapi); return; }

    // Create mem_P[]
    const cl_image_format format_p = { CL_R, channel_type };
    d.mem_P[0] = clCreateImage(d.context, CL_MEM_READ_WRITE, &format_p, &desc_out, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_P[0])", ret, out, vsapi); return; }
    d.mem_P[1] = clCreateImage(d.context, CL_MEM_READ_WRITE, &format_p, &desc_out, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_P[1])", ret, out, vsapi); return; }
    d.mem_P[2] = clCreateImage(d.context, CL_MEM_READ_WRITE, &format_p, &desc_out, NULL, &ret);
    if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateImage(d.mem_P[2])", ret, out, vsapi); return; }
    // d.mem_P[3] is only required to AviSynth
    // d.mem_P[4] is only required to AviSynth
    // d.mem_P[5] is only required to AviSynth

    // Create and Build a program executable from the program source
    d.program = clCreateProgramWithSource(d.context, 1, d.d ? &kernel_source_code : &kernel_source_code_spatial, NULL, NULL);
    char options[2048];
    setlocale(LC_ALL, "C");
#    ifdef __APPLE__
    snprintf(options, 2048, "-cl-denorms-are-zero -cl-fast-relaxed-math -cl-mad-enable \
        -D NLM_CLIP_TYPE_UNORM=%u -D NLM_CLIP_TYPE_UNSIGNED=%u -D NLM_CLIP_TYPE_STACKED=%u \
        -D NLM_CLIP_REF_LUMA=%u -D NLM_CLIP_REF_CHROMA=%u -D NLM_CLIP_REF_YUV=%u -D NLM_CLIP_REF_RGB=%u \
        -D NLM_WMODE_CAUCHY=%u -D NLM_WMODE_WELSCH=%u -D NLM_WMODE_BISQUARE=%u -D NLM_WMODE_MOD_BISQUARE=%u \
        -D HRZ_BLOCK_X=%u -D HRZ_BLOCK_Y=%u -D HRZ_RESULT=%u -D VRT_BLOCK_X=%u -D VRT_BLOCK_Y=%u -D VRT_RESULT=%u \
        -D NLM_TCLIP=%u -D NLM_D=%i -D NLM_S=%i -D NLM_WMODE=%i -D NLM_WREF=%.17ff -D NLM_H2_INV_NORM=%.17ff -D NLM_UNORM_MAX=%ff",
        NLM_CLIP_TYPE_UNORM, NLM_CLIP_TYPE_UNSIGNED, NLM_CLIP_TYPE_STACKED,
        NLM_CLIP_REF_LUMA, NLM_CLIP_REF_CHROMA, NLM_CLIP_REF_YUV, NLM_CLIP_REF_RGB,
        NLM_WMODE_CAUCHY, NLM_WMODE_WELSCH, NLM_WMODE_BISQUARE, NLM_WMODE_MOD_BISQUARE,
        HRZ_BLOCK_X, HRZ_BLOCK_Y, HRZ_RESULT, VRT_BLOCK_X, VRT_BLOCK_Y, VRT_RESULT,
        d.clip_t, int64ToIntS(d.d), int64ToIntS(d.s), int64ToIntS(d.wmode), d.wref,
        65025.0 / (3 * d.h * d.h * (2 * d.s + 1)*(2 * d.s + 1)), (double) maxvalue(d.vi->format->bitsPerSample));
#    else
    snprintf(options, 2048, "-cl-single-precision-constant -cl-denorms-are-zero -cl-fast-relaxed-math -Werror \
        -D NLM_CLIP_TYPE_UNORM=%u -D NLM_CLIP_TYPE_UNSIGNED=%u -D NLM_CLIP_TYPE_STACKED=%u \
        -D NLM_CLIP_REF_LUMA=%u -D NLM_CLIP_REF_CHROMA=%u -D NLM_CLIP_REF_YUV=%u -D NLM_CLIP_REF_RGB=%u \
        -D NLM_WMODE_CAUCHY=%u -D NLM_WMODE_WELSCH=%u -D NLM_WMODE_BISQUARE=%u -D NLM_WMODE_MOD_BISQUARE=%u \
        -D HRZ_BLOCK_X=%u -D HRZ_BLOCK_Y=%u -D HRZ_RESULT=%u -D VRT_BLOCK_X=%u -D VRT_BLOCK_Y=%u -D VRT_RESULT=%u \
        -D NLM_TCLIP=%u -D NLM_D=%i -D NLM_S=%i -D NLM_WMODE=%i -D NLM_WREF=%.17f -D NLM_H2_INV_NORM=%.17f -D NLM_UNORM_MAX=%f",
        NLM_CLIP_TYPE_UNORM, NLM_CLIP_TYPE_UNSIGNED, NLM_CLIP_TYPE_STACKED,
        NLM_CLIP_REF_LUMA, NLM_CLIP_REF_CHROMA, NLM_CLIP_REF_YUV, NLM_CLIP_REF_RGB,
        NLM_WMODE_CAUCHY, NLM_WMODE_WELSCH, NLM_WMODE_BISQUARE, NLM_WMODE_MOD_BISQUARE,
        HRZ_BLOCK_X, HRZ_BLOCK_Y, HRZ_RESULT, VRT_BLOCK_X, VRT_BLOCK_Y, VRT_RESULT,
        d.clip_t, int64ToIntS(d.d), int64ToIntS(d.s), int64ToIntS(d.wmode), d.wref,
        65025.0 / (3 * d.h * d.h * (2 * d.s + 1)*(2 * d.s + 1)), (double) maxvalue(d.vi->format->bitsPerSample));
#    endif
    ret = clBuildProgram(d.program, 1, &d.deviceID, options, NULL, NULL);
    if (ret != CL_SUCCESS) {
        oclUtilsDebugInfo(d.platformID, d.deviceID, d.program);
        vsapi->setError(out, "knlm.KNLMeansCL: build programm error!\n Please report Log-KNLMeansCL.txt.");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    setlocale(LC_ALL, "");

    // Create kernel objects
    if (d.d) {
        d.kernel[nlmDistanceLeft] = clCreateKernel(d.program, "nlmDistanceLeft", &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmDistanceLeft)", ret, out, vsapi); return; }
        d.kernel[nlmDistanceRight] = clCreateKernel(d.program, "nlmDistanceRight", &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmDistanceRight)", ret, out, vsapi); return; }
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
    } else {
        d.kernel[nlmSpatialDistance] = clCreateKernel(d.program, "nlmSpatialDistance", &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmSpatialDistance)", ret, out, vsapi); return; }
        d.kernel[nlmSpatialHorizontal] = clCreateKernel(d.program, "nlmSpatialHorizontal", &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmSpatialHorizontal)", ret, out, vsapi); return; }
        d.kernel[nlmSpatialVertical] = clCreateKernel(d.program, "nlmSpatialVertical", &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmSpatialVertical)", ret, out, vsapi); return; }
        d.kernel[nlmSpatialAccumulation] = clCreateKernel(d.program, "nlmSpatialAccumulation", &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmSpatialAccumulation)", ret, out, vsapi); return; }
        d.kernel[nlmSpatialFinish] = clCreateKernel(d.program, "nlmSpatialFinish", &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmSpatialFinish)", ret, out, vsapi); return; }
        d.kernel[nlmSpatialPack] = clCreateKernel(d.program, "nlmSpatialPack", &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmSpatialPack)", ret, out, vsapi); return; }
        d.kernel[nlmSpatialUnpack] = clCreateKernel(d.program, "nlmSpatialUnpack", &ret);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clCreateKernel(nlmSpatialUnpack)", ret, out, vsapi); return; }
    }

    // Set kernel arguments
    if (d.d) {
        // nlmDistanceLeft
        ret = clSetKernelArg(d.kernel[nlmDistanceLeft], 0, sizeof(cl_mem), &d.mem_in[(d.clip_t & NLM_CLIP_EXTRA_FALSE) ? 0 : 1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmDistanceLeft[0])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmDistanceLeft], 1, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmDistanceLeft[1])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmDistanceLeft], 2, 2 * sizeof(cl_uint), &d.idmn);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmDistanceLeft[2])", ret, out, vsapi); return; }

        // nlmDistanceRight
        ret = clSetKernelArg(d.kernel[nlmDistanceRight], 0, sizeof(cl_mem), &d.mem_in[(d.clip_t & NLM_CLIP_EXTRA_FALSE) ? 0 : 1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmDistanceRight[0])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmDistanceRight], 1, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmDistanceRight[1])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmDistanceRight], 2, 2 * sizeof(cl_uint), &d.idmn);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmDistanceRight[2])", ret, out, vsapi); return; }

        // nlmHorizontal
        ret = clSetKernelArg(d.kernel[nlmHorizontal], 0, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmHorizontal[0])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmHorizontal], 1, sizeof(cl_mem), &d.mem_U[2]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmHorizontal[1])", ret, out, vsapi); return; }
        // d.kernel[nlmHorizontal] -> 2 is set by VapourSynthPluginGetFrame
        ret = clSetKernelArg(d.kernel[nlmHorizontal], 3, 2 * sizeof(cl_uint), &d.idmn);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmHorizontal[3])", ret, out, vsapi); return; }

        // nlmVertical
        ret = clSetKernelArg(d.kernel[nlmVertical], 0, sizeof(cl_mem), &d.mem_U[2]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmVertical[0])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmVertical], 1, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmVertical[1])", ret, out, vsapi); return; }
        // d.kernel[nlmVertical] -> 2 is set by VapourSynthPluginGetFrame
        ret = clSetKernelArg(d.kernel[nlmVertical], 3, 2 * sizeof(cl_uint), &d.idmn);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmVertical[3])", ret, out, vsapi); return; }

        // nlmAccumulation
        ret = clSetKernelArg(d.kernel[nlmAccumulation], 0, sizeof(cl_mem), &d.mem_in[0]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmAccumulation[0])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmAccumulation], 1, sizeof(cl_mem), &d.mem_U[0]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmAccumulation[1])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmAccumulation], 2, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmAccumulation[2])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmAccumulation], 3, sizeof(cl_mem), &d.mem_U[3]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmAccumulation[3])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmAccumulation], 4, 2 * sizeof(cl_uint), &d.idmn);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmAccumulation[4])", ret, out, vsapi); return; }

        // nlmFinish
        ret = clSetKernelArg(d.kernel[nlmFinish], 0, sizeof(cl_mem), &d.mem_in[0]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmFinish[0])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmFinish], 1, sizeof(cl_mem), &d.mem_out);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmFinish[1])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmFinish], 2, sizeof(cl_mem), &d.mem_U[0]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmFinish[2])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmFinish], 3, sizeof(cl_mem), &d.mem_U[3]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmFinish[3])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmFinish], 4, 2 * sizeof(cl_uint), &d.idmn);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmFinish[4])", ret, out, vsapi); return; }

        // nlmPack
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
        ret = clSetKernelArg(d.kernel[nlmPack], 8, 2 * sizeof(cl_uint), &d.idmn);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmPack[8])", ret, out, vsapi); return; }

        // nlmUnpack
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
        ret = clSetKernelArg(d.kernel[nlmUnpack], 6, sizeof(cl_mem), &d.mem_out);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmUnpack[6])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmUnpack], 7, 2 * sizeof(cl_uint), &d.idmn);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmUnpack[7])", ret, out, vsapi); return; }
    } else {
        // nlmSpatialDistance
        ret = clSetKernelArg(d.kernel[nlmSpatialDistance], 0, sizeof(cl_mem), &d.mem_in[(d.clip_t & NLM_CLIP_EXTRA_FALSE) ? 0 : 1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialDistance[0])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialDistance], 1, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialDistance[1])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialDistance], 2, 2 * sizeof(cl_uint), &d.idmn);

        // nlmSpatialHorizontal
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialDistance[2])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialHorizontal], 0, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialHorizontal[0])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialHorizontal], 1, sizeof(cl_mem), &d.mem_U[2]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialHorizontal[1])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialHorizontal], 2, 2 * sizeof(cl_uint), &d.idmn);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialHorizontal[2])", ret, out, vsapi); return; }

        // nlmSpatialVertical
        ret = clSetKernelArg(d.kernel[nlmSpatialVertical], 0, sizeof(cl_mem), &d.mem_U[2]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialVertical[0])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialVertical], 1, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialVertical[1])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialVertical], 2, 2 * sizeof(cl_uint), &d.idmn);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialVertical[2])", ret, out, vsapi); return; }

        // nlmSpatialAccumulation
        ret = clSetKernelArg(d.kernel[nlmSpatialAccumulation], 0, sizeof(cl_mem), &d.mem_in[0]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialAccumulation[0])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialAccumulation], 1, sizeof(cl_mem), &d.mem_U[0]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialAccumulation[1])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialAccumulation], 2, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialAccumulation[2])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialAccumulation], 3, sizeof(cl_mem), &d.mem_U[3]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialAccumulation[3])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialAccumulation], 4, 2 * sizeof(cl_uint), &d.idmn);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialAccumulation[4])", ret, out, vsapi); return; }

        // nlmSpatialFinish
        ret = clSetKernelArg(d.kernel[nlmSpatialFinish], 0, sizeof(cl_mem), &d.mem_in[0]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialFinish[0])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialFinish], 1, sizeof(cl_mem), &d.mem_out);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialFinish[1])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialFinish], 2, sizeof(cl_mem), &d.mem_U[0]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialFinish[2])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialFinish], 3, sizeof(cl_mem), &d.mem_U[3]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialFinish[3])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialFinish], 4, 2 * sizeof(cl_uint), &d.idmn);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialFinish[4])", ret, out, vsapi); return; }

        // nlmSpatialPack
        ret = clSetKernelArg(d.kernel[nlmSpatialPack], 0, sizeof(cl_mem), &d.mem_P[0]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialPack[0])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialPack], 1, sizeof(cl_mem), &d.mem_P[1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialPack[1])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialPack], 2, sizeof(cl_mem), &d.mem_P[2]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialPack[2])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialPack], 3, sizeof(cl_mem), &d.mem_P[0]); // dummy, 3 is reserved for AviSynth
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialPack[3])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialPack], 4, sizeof(cl_mem), &d.mem_P[1]); // dummy, 4 is reserved for AviSynth
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialPack[4])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialPack], 5, sizeof(cl_mem), &d.mem_P[2]); // dummy, 5 is reserved for AviSynth
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialPack[5])", ret, out, vsapi); return; }
        // d.kernel[nlmSpatialPack] -> 6 is set by VapourSynthPluginGetFrame
        ret = clSetKernelArg(d.kernel[nlmSpatialPack], 7, 2 * sizeof(cl_uint), &d.idmn);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialPack[7])", ret, out, vsapi); return; }

        // nlmSpatialUnpack
        ret = clSetKernelArg(d.kernel[nlmSpatialUnpack], 0, sizeof(cl_mem), &d.mem_P[0]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[0])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialUnpack], 1, sizeof(cl_mem), &d.mem_P[1]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[1])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialUnpack], 2, sizeof(cl_mem), &d.mem_P[2]);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[2])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialUnpack], 3, sizeof(cl_mem), &d.mem_P[0]); // dummy, 3 is reserved for AviSynth
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[3])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialUnpack], 4, sizeof(cl_mem), &d.mem_P[1]); // dummy, 4 is reserved for AviSynth
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[4])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialUnpack], 5, sizeof(cl_mem), &d.mem_P[2]); // dummy, 5 is reserved for AviSynth
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[5])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialUnpack], 6, sizeof(cl_mem), &d.mem_out);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[6])", ret, out, vsapi); return; }
        ret = clSetKernelArg(d.kernel[nlmSpatialUnpack], 7, 2 * sizeof(cl_uint), &d.idmn);
        if (ret != CL_SUCCESS) { d.oclErrorCheck("clSetKernelArg(nlmSpatialUnpack[7])", ret, out, vsapi); return; }
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
    env->AddFunction("KNLMeansCL", "c[d]i[a]i[s]i[h]f[channels]s[wmode]i[wref]f[rclip]c[device_type]s[device_id]i[lsb_inout]b\
[info]b", AviSynthPluginCreate, 0);
    return "KNLMeansCL for AviSynth";
}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthPluginInit
#ifdef VAPOURSYNTH_H
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.Khanattila.KNLMeansCL", "knlm", "KNLMeansCL for VapourSynth", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("KNLMeansCL", "clip:clip;d:int:opt;a:int:opt;s:int:opt;h:float:opt;channels:data:opt;wmode:int:opt;\
wref:float:opt;rclip:clip:opt;device_type:data:opt;device_id:int:opt;info:int:opt", VapourSynthPluginCreate, nullptr, plugin);
}
#endif //__VAPOURSYNTH_H__