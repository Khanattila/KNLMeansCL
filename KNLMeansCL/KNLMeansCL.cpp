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

#define COLOR_GRAY       (1 << 0)
#define COLOR_YUV        (1 << 1)
#define COLOR_RGB        (1 << 2)
#define CLIP_UNORM       (1 << 3)
#define CLIP_UNSIGNED    (1 << 4)
#define CLIP_STACKED     (1 << 5)
#define EXTRA_FALSE      (1 << 6)
#define EXTRA_TRUE       (1 << 7)

#define DFT_d             1
#define DFT_a             2
#define DFT_s             4
#define DFT_h             1.2f
#define DFT_cmode         false
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
    if (errcode != CL_SUCCESS) {
        char buffer[2048];
        snprintf(buffer, 2048, "KNLMeansCL: fatal error!\n (%s: %s)", function, oclUtilsErrorToString(errcode));
        env->ThrowError(buffer);
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
    char buffer[2048];
    snprintf(buffer, 2048, "knlm.KNLMeansCL: fatal error!\n (%s: %s)", function, oclUtilsErrorToString(errcode));
    vsapi->setError(out, buffer);
    vsapi->freeNode(node);
    vsapi->freeNode(knot);
}
#endif //__VAPOURSYNTH_H__

//////////////////////////////////////////
// AviSynthInit
#ifdef __AVISYNTH_6_H__
_NLMAvisynth::_NLMAvisynth(PClip _child, const int _d, const int _a, const int _s, const double _h, const bool _cmode, const int _wmode,
    const double _wref, PClip _baby, const char* _ocl_device, const int _ocl_id, const bool _lsb, const bool _info,
    IScriptEnvironment *env) : GenericVideoFilter(_child), d(_d), a(_a), s(_s), h(_h), cmode(_cmode), wmode(_wmode), wref(_wref),
    baby(_baby), ocl_device(_ocl_device), ocl_id(_ocl_id), lsb(_lsb), info(_info) {

    // Checks AviSynth Version.
    env->CheckVersion(5);
    child->SetCacheHints(CACHE_WINDOW, d);

    // Checks source clip and rclip.
    cl_channel_order channel_order = 0;
    cl_channel_type channel_type = 0;
    if (!vi.IsPlanar() && !vi.IsRGB32())
        env->ThrowError("KNLMeansCL: planar YUV or RGB32 data!");
    if (cmode && !vi.IsYV24() && !vi.IsRGB32())
        env->ThrowError("KNLMeansCL: 'cmode' requires 4:4:4 subsampling!");
    if (baby) {
        VideoInfo rvi = baby->GetVideoInfo();
        if (!equals(&vi, &rvi))
            env->ThrowError("KNLMeansCL: 'rclip' do not math source clip!");
        baby->SetCacheHints(CACHE_WINDOW, d);
        clip_t = EXTRA_TRUE;
    } else clip_t = EXTRA_FALSE;
    if (lsb) {
        if (vi.IsRGB32()) env->ThrowError("KNLMeansCL: RGB48y is not supported!");
        else if (cmode) clip_t |= (CLIP_STACKED | COLOR_YUV);
        else clip_t |= (CLIP_STACKED | COLOR_GRAY);
    } else {
        if (vi.IsRGB32()) clip_t |= (CLIP_UNORM | COLOR_RGB);
        else if (cmode) clip_t |= (CLIP_UNORM | COLOR_YUV);
        else clip_t |= (CLIP_UNORM | COLOR_GRAY);
    }
    if (clip_t & COLOR_GRAY) channel_order = CL_LUMINANCE;
    else if (clip_t & COLOR_YUV) channel_order = CL_RGBA;
    else if (clip_t & COLOR_RGB) channel_order = CL_RGBA;
    if (clip_t & CLIP_UNORM) channel_type = CL_UNORM_INT8;
    else if (clip_t & CLIP_STACKED) channel_type = CL_UNORM_INT16;

    // Checks user value.
    if (d < 0)
        env->ThrowError("KNLMeansCL: 'd' must be greater than or equal to 0!");
    if (a < 1)
        env->ThrowError("KNLMeansCL: 'a' must be greater than or equal to 1!");
    if (s < 0 || s > 4)
        env->ThrowError("KNLMeansCL: 's' must be in range [0, 4]!");
    if (h <= 0.0f)
        env->ThrowError("KNLMeansCL: 'h' must be greater than 0!");
    if (wmode < 0 || wmode > 2)
        env->ThrowError("KNLMeansCL: 'wmode' must be in range [0, 2]!");
    if (wref < 0.0f)
        env->ThrowError("KNLMeansCL: 'wref' must be greater than or equal to 0!");
    ocl_utils_device_type ocl_device_type = 0;   
    if (!strcasecmp(ocl_device, "CPU")) {
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_CPU;
    } else if (!strcasecmp(ocl_device, "GPU")) {
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_GPU;
    } else if (!strcasecmp(ocl_device, "ACCELERATOR")) {
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_ACCELERATOR;
    } else if (!strcasecmp(ocl_device, "AUTO")) {
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_AUTO;
    } else {
        env->ThrowError("KNLMeansCL: 'device_type' must be 'cpu', 'gpu', 'accelerator' or 'auto'!");
    }
    if (ocl_id < 0)
        env->ThrowError("KNLMeansCL: 'device_id' must be greater than or equal to 0!");
    if (info && vi.IsRGB())
        env->ThrowError("KNLMeansCL: 'info' requires YUV color space!");

    // Gets PlatformID and DeviceID.
    cl_device_type deviceTYPE;
    cl_int ret = oclUtilsGetPlaformDeviceIDs(OCL_UTILS_OPENCL_1_2, ocl_device_type, (cl_uint) ocl_id, &platformID, &deviceID, &deviceTYPE);
    if (ret == OCL_UTILS_NO_DEVICE_AVAILABLE) env->ThrowError("KNLMeansCL: no compatible opencl platforms available!");
    else if (ret != CL_SUCCESS) oclErrorCheck("oclUtilsGetPlaformDeviceIDs", ret, env);

    // Sets local work size
    switch (deviceTYPE) {
        case CL_DEVICE_TYPE_CPU:
            HRZ_BLOCK_X = 4; HRZ_BLOCK_Y = 8;
            VRT_BLOCK_X = 8; VRT_BLOCK_Y = 4;
            break;
        case CL_DEVICE_TYPE_GPU:
        case CL_DEVICE_TYPE_ACCELERATOR:
            HRZ_BLOCK_X = 16; HRZ_BLOCK_Y = 8;
            VRT_BLOCK_X = 8; VRT_BLOCK_Y = 16;
            break;  
        default:
            env->ThrowError("KNLMeansCL: fatal error!\n (Local work size)");
            break;
    }

    // Creates an OpenCL context, 2D images and buffers object.
    idmn[0] = vi.width;
    idmn[1] = lsb ? (vi.height / 2) : (vi.height);
    idmn[2] = vi.height;
    context = clCreateContext(NULL, 1, &deviceID, NULL, NULL, &ret);
    oclErrorCheck("clCreateContext", ret, env);
    const cl_image_format image_format = { channel_order, channel_type };
    const cl_image_desc image_desc = { (cl_mem_object_type) (d ? CL_MEM_OBJECT_IMAGE2D_ARRAY : CL_MEM_OBJECT_IMAGE2D),
        (size_t) idmn[0], (size_t) idmn[1], 1, 2 * (size_t) d + 1, 0, 0, 0, 0, NULL };
    const cl_image_desc image_desc_out = { CL_MEM_OBJECT_IMAGE2D, (size_t) idmn[0], (size_t) idmn[1], 1, 1, 0, 0, 0, 0, NULL };
    if (!(clip_t & COLOR_YUV) && !lsb) {
        mem_in[0] = clCreateImage(context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, &image_format, &image_desc, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_in[0])", ret, env);
        mem_in[1] = clCreateImage(context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, &image_format, &image_desc, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_in[1])", ret, env);
        mem_out = clCreateImage(context, CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY, &image_format, &image_desc_out, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_out)", ret, env);
    } else {
        mem_in[0] = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format, &image_desc, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_in[0])", ret, env);
        mem_in[1] = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format, &image_desc, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_in[1])", ret, env);
        mem_out = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format, &image_desc_out, NULL, &ret);
        oclErrorCheck("clCreateImage(mem_out)", ret, env);
    }
    const cl_image_format image_format_u = { CL_LUMINANCE, CL_HALF_FLOAT };
    const size_t size_u0 = sizeof(cl_float) * idmn[0] * idmn[1] * ((clip_t & COLOR_GRAY) ? 2 : 4);
    const size_t size_u3 = sizeof(cl_float) * idmn[0] * idmn[1];
    mem_U[0] = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u0, NULL, &ret);
    oclErrorCheck("clCreateBuffer(mem_U[0])", ret, env);
    mem_U[1] = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format_u, &image_desc, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_U[1])", ret, env);
    mem_U[2] = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format_u, &image_desc, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_U[2])", ret, env);
    mem_U[3] = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u3, NULL, &ret);
    oclErrorCheck("clCreateBuffer(mem_U[3])", ret, env);
    const cl_image_format image_format_p = { CL_LUMINANCE, CL_UNORM_INT8 };
    const cl_image_desc image_desc_p = { CL_MEM_OBJECT_IMAGE2D, (size_t) idmn[0], (size_t) idmn[2], 1, 1, 0, 0, 0, 0, NULL };
    mem_P[0] = clCreateImage(context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_p, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_P[0])", ret, env);
    mem_P[1] = clCreateImage(context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_p, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_P[1])", ret, env);
    mem_P[2] = clCreateImage(context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_p, NULL, &ret);
    oclErrorCheck("clCreateImage(mem_P[2])", ret, env);

    // Creates and Build a program executable from the program source.
    program = clCreateProgramWithSource(context, 1, d ? &kernel_source_code : &kernel_source_code_spatial, NULL, NULL);
    char options[2048];
    setlocale(LC_ALL, "C");
    snprintf(options, 2048, "-cl-single-precision-constant -cl-denorms-are-zero -cl-fast-relaxed-math -Werror "
        "-D HRZ_BLOCK_X=%i -D HRZ_BLOCK_Y=%i -D VRT_BLOCK_X=%i -D VRT_BLOCK_Y=%i -D NLMK_TCLIP=%u -D NLMK_S=%i "
        "-D NLMK_WMODE=%i -D NLMK_WREF=%f -D NLMK_D=%i -D NLMK_H2_INV_NORM=%f -D NLMK_BIT_SHIFT=%u",
        HRZ_BLOCK_X, HRZ_BLOCK_Y, VRT_BLOCK_X, VRT_BLOCK_Y, clip_t, s,
        wmode, wref, d, 65025.0 / (3 * h * h * (2 * s + 1)*(2 * s + 1)), 0u);
    ret = clBuildProgram(program, 1, &deviceID, options, NULL, NULL);
    if (ret != CL_SUCCESS) {
        size_t options_size, log_size;
        clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_OPTIONS, 0, NULL, &options_size);
        clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char *options_txt = (char*) malloc(options_size);
        char *log_txt = (char*) malloc(log_size);
        clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_OPTIONS, options_size, options_txt, NULL);
        clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_LOG, log_size, log_txt, NULL);
        std::ofstream outfile("Log-KNLMeansCL.txt", std::ofstream::out);
        outfile << "---------------------------------" << std::endl;
        outfile << "*** Error in OpenCL compiler ***" << std::endl;
        outfile << "---------------------------------" << std::endl;
        outfile << std::endl << "# Build Options" << std::endl;
        outfile << options_txt << std::endl;
        outfile << std::endl << "# Build Log" << std::endl;
        outfile << log_txt << std::endl;
        outfile.close();
        free(log_txt);
        free(options_txt);
        env->ThrowError("KNLMeansCL: fatal error!\n (clBuildProgram: please report Log-KNLMeansCL.txt.)");
    }
    setlocale(LC_ALL, "");

    // Creates kernel objects.
    if (d) {
        kernel[NLM_DISTANCE_LEFT] = clCreateKernel(program, "NLM_DISTANCE_LEFT", &ret);
        oclErrorCheck("clCreateKernel(NLM_DISTANCE_LEFT)", ret, env);
        kernel[NLM_DISTANCE_RIGHT] = clCreateKernel(program, "NLM_DISTANCE_RIGHT", &ret);
        oclErrorCheck("clCreateKernel(NLM_DISTANCE_RIGHT)", ret, env);
        kernel[NLM_HORIZONTAL] = clCreateKernel(program, "NLM_HORIZONTAL", &ret);
        oclErrorCheck("clCreateKernel(NLM_HORIZONTAL)", ret, env);
        kernel[NLM_VERTICAL] = clCreateKernel(program, "NLM_VERTICAL", &ret);
        oclErrorCheck("clCreateKernel(NLM_VERTICAL)", ret, env);
        kernel[NLM_ACCUMULATION] = clCreateKernel(program, "NLM_ACCUMULATION", &ret);
        oclErrorCheck("clCreateKernel(NLM_ACCUMULATION)", ret, env);
        kernel[NLM_FINISH] = clCreateKernel(program, "NLM_FINISH", &ret);
        oclErrorCheck("clCreateKernel(NLM_FINISH)", ret, env);
        kernel[NLM_PACK] = clCreateKernel(program, "NLM_PACK", &ret);
        oclErrorCheck("clCreateKernel(NLM_PACK)", ret, env);
        kernel[NLM_UNPACK] = clCreateKernel(program, "NLM_UNPACK", &ret);
        oclErrorCheck("clCreateKernel(NLM_UNPACK)", ret, env);
    } else {
        kernel[NLM_SPATIAL_DISTANCE] = clCreateKernel(program, "NLM_SPATIAL_DISTANCE", &ret);
        oclErrorCheck("clCreateKernel(NLM_SPATIAL_DISTANCE)", ret, env);
        kernel[NLM_SPATIAL_HORIZONTAL] = clCreateKernel(program, "NLM_SPATIAL_HORIZONTAL", &ret);
        oclErrorCheck("clCreateKernel(NLM_SPATIAL_HORIZONTAL)", ret, env);
        kernel[NLM_SPATIAL_VERTICAL] = clCreateKernel(program, "NLM_SPATIAL_VERTICAL", &ret);
        oclErrorCheck("clCreateKernel(NLM_SPATIAL_VERTICAL)", ret, env);
        kernel[NLM_SPATIAL_ACCUMULATION] = clCreateKernel(program, "NLM_SPATIAL_ACCUMULATION", &ret);
        oclErrorCheck("clCreateKernel(NLM_SPATIAL_ACCUMULATION)", ret, env);
        kernel[NLM_SPATIAL_FINISH] = clCreateKernel(program, "NLM_SPATIAL_FINISH", &ret);
        oclErrorCheck("clCreateKernel(NLM_SPATIAL_FINISH)", ret, env);
        kernel[NLM_SPATIAL_PACK] = clCreateKernel(program, "NLM_SPATIAL_PACK", &ret);
        oclErrorCheck("clCreateKernel(NLM_SPATIAL_PACK)", ret, env);
        kernel[NLM_SPATIAL_UNPACK] = clCreateKernel(program, "NLM_SPATIAL_UNPACK", &ret);
        oclErrorCheck("clCreateKernel(NLM_SPATIAL_UNPACK)", ret, env);
    }

    // Sets kernel arguments.
    if (d) {
        ret = clSetKernelArg(kernel[NLM_DISTANCE_LEFT], 0, sizeof(cl_mem), &mem_in[(clip_t & EXTRA_FALSE) ? 0 : 1]);
        oclErrorCheck("clSetKernelArg(NLM_DISTANCE_LEFT[0])", ret, env);
        ret = clSetKernelArg(kernel[NLM_DISTANCE_LEFT], 1, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(NLM_DISTANCE_LEFT[1])", ret, env);
        ret = clSetKernelArg(kernel[NLM_DISTANCE_LEFT], 2, 2 * sizeof(cl_int), &idmn);
        oclErrorCheck("clSetKernelArg(NLM_DISTANCE_LEFT[2])", ret, env);
        ret = clSetKernelArg(kernel[NLM_DISTANCE_RIGHT], 0, sizeof(cl_mem), &mem_in[(clip_t & EXTRA_FALSE) ? 0 : 1]);
        oclErrorCheck("clSetKernelArg(NLM_DISTANCE_RIGHT[0])", ret, env);
        ret = clSetKernelArg(kernel[NLM_DISTANCE_RIGHT], 1, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(NLM_DISTANCE_RIGHT[1])", ret, env);
        ret = clSetKernelArg(kernel[NLM_DISTANCE_RIGHT], 2, 2 * sizeof(cl_int), &idmn);
        oclErrorCheck("clSetKernelArg(NLM_DISTANCE_RIGHT[2])", ret, env);
        ret = clSetKernelArg(kernel[NLM_HORIZONTAL], 0, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(NLM_HORIZONTAL[0])", ret, env);
        ret = clSetKernelArg(kernel[NLM_HORIZONTAL], 1, sizeof(cl_mem), &mem_U[2]);
        oclErrorCheck("clSetKernelArg(NLM_HORIZONTAL[1])", ret, env);
        ret = clSetKernelArg(kernel[NLM_HORIZONTAL], 3, 2 * sizeof(cl_int), &idmn);
        oclErrorCheck("clSetKernelArg(NLM_HORIZONTAL[3])", ret, env);
        ret = clSetKernelArg(kernel[NLM_VERTICAL], 0, sizeof(cl_mem), &mem_U[2]);
        oclErrorCheck("clSetKernelArg(NLM_VERTICAL[0])", ret, env);
        ret = clSetKernelArg(kernel[NLM_VERTICAL], 1, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(NLM_VERTICAL[1])", ret, env);
        ret = clSetKernelArg(kernel[NLM_VERTICAL], 3, 2 * sizeof(cl_int), &idmn);
        oclErrorCheck("clSetKernelArg(NLM_VERTICAL[3])", ret, env);
        ret = clSetKernelArg(kernel[NLM_ACCUMULATION], 0, sizeof(cl_mem), &mem_in[0]);
        oclErrorCheck("clSetKernelArg(NLM_ACCUMULATION[0])", ret, env);
        ret = clSetKernelArg(kernel[NLM_ACCUMULATION], 1, sizeof(cl_mem), &mem_U[0]);
        oclErrorCheck("clSetKernelArg(NLM_ACCUMULATION[1])", ret, env);
        ret = clSetKernelArg(kernel[NLM_ACCUMULATION], 2, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(NLM_ACCUMULATION[2])", ret, env);
        ret = clSetKernelArg(kernel[NLM_ACCUMULATION], 3, sizeof(cl_mem), &mem_U[3]);
        oclErrorCheck("clSetKernelArg(NLM_ACCUMULATION[3])", ret, env);
        ret = clSetKernelArg(kernel[NLM_ACCUMULATION], 4, 2 * sizeof(cl_int), &idmn);
        oclErrorCheck("clSetKernelArg(NLM_ACCUMULATION[4])", ret, env);
        ret = clSetKernelArg(kernel[NLM_FINISH], 0, sizeof(cl_mem), &mem_in[0]);
        oclErrorCheck("clSetKernelArg(NLM_FINISH[0])", ret, env);
        ret = clSetKernelArg(kernel[NLM_FINISH], 1, sizeof(cl_mem), &mem_out);
        oclErrorCheck("clSetKernelArg(NLM_FINISH[1])", ret, env);
        ret = clSetKernelArg(kernel[NLM_FINISH], 2, sizeof(cl_mem), &mem_U[0]);
        oclErrorCheck("clSetKernelArg(NLM_FINISH[2])", ret, env);
        ret = clSetKernelArg(kernel[NLM_FINISH], 3, sizeof(cl_mem), &mem_U[3]);
        oclErrorCheck("clSetKernelArg(NLM_FINISH[3])", ret, env);
        ret = clSetKernelArg(kernel[NLM_FINISH], 4, 2 * sizeof(cl_int), &idmn);
        oclErrorCheck("clSetKernelArg(NLM_FINISH[4])", ret, env);
        ret = clSetKernelArg(kernel[NLM_PACK], 0, sizeof(cl_mem), &mem_P[0]);
        oclErrorCheck("clSetKernelArg(NLM_PACK[0])", ret, env);
        ret = clSetKernelArg(kernel[NLM_PACK], 1, sizeof(cl_mem), &mem_P[1]);
        oclErrorCheck("clSetKernelArg(NLM_PACK[1])", ret, env);
        ret = clSetKernelArg(kernel[NLM_PACK], 2, sizeof(cl_mem), &mem_P[2]);
        oclErrorCheck("clSetKernelArg(NLM_PACK[2])", ret, env);
        ret = clSetKernelArg(kernel[NLM_PACK], 5, 2 * sizeof(cl_int), &idmn);
        oclErrorCheck("clSetKernelArg(NLM_PACK[5])", ret, env);
        ret = clSetKernelArg(kernel[NLM_UNPACK], 0, sizeof(cl_mem), &mem_P[0]);
        oclErrorCheck("clSetKernelArg(NLM_UNPACK[0])", ret, env);
        ret = clSetKernelArg(kernel[NLM_UNPACK], 1, sizeof(cl_mem), &mem_P[1]);
        oclErrorCheck("clSetKernelArg(NLM_UNPACK[1])", ret, env);
        ret = clSetKernelArg(kernel[NLM_UNPACK], 2, sizeof(cl_mem), &mem_P[2]);
        oclErrorCheck("clSetKernelArg(NLM_UNPACK[2])", ret, env);
        ret = clSetKernelArg(kernel[NLM_UNPACK], 4, 2 * sizeof(cl_int), &idmn);
        oclErrorCheck("clSetKernelArg(NLM_UNPACK[4])", ret, env);
    } else {
        ret = clSetKernelArg(kernel[NLM_SPATIAL_DISTANCE], 0, sizeof(cl_mem), &mem_in[(clip_t & EXTRA_FALSE) ? 0 : 1]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_DISTANCE[0])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_DISTANCE], 1, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_DISTANCE[1])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_DISTANCE], 2, 2 * sizeof(cl_int), &idmn);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_DISTANCE[2])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_HORIZONTAL], 0, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_HORIZONTAL[0])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_HORIZONTAL], 1, sizeof(cl_mem), &mem_U[2]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_HORIZONTAL[1])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_HORIZONTAL], 2, 2 * sizeof(cl_int), &idmn);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_HORIZONTAL[2])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_VERTICAL], 0, sizeof(cl_mem), &mem_U[2]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_VERTICAL[0])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_VERTICAL], 1, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_VERTICAL[1])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_VERTICAL], 2, 2 * sizeof(cl_int), &idmn);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_VERTICAL[2])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_ACCUMULATION], 0, sizeof(cl_mem), &mem_in[0]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_ACCUMULATION[0])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_ACCUMULATION], 1, sizeof(cl_mem), &mem_U[0]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_ACCUMULATION[1])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_ACCUMULATION], 2, sizeof(cl_mem), &mem_U[1]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_ACCUMULATION[2])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_ACCUMULATION], 3, sizeof(cl_mem), &mem_U[3]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_ACCUMULATION[3])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_ACCUMULATION], 4, 2 * sizeof(cl_int), &idmn);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_ACCUMULATION[4])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_FINISH], 0, sizeof(cl_mem), &mem_in[0]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_FINISH[0])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_FINISH], 1, sizeof(cl_mem), &mem_out);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_FINISH[1])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_FINISH], 2, sizeof(cl_mem), &mem_U[0]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_FINISH[2])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_FINISH], 3, sizeof(cl_mem), &mem_U[3]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_FINISH[3])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_FINISH], 4, 2 * sizeof(cl_int), &idmn);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_FINISH[4])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_PACK], 0, sizeof(cl_mem), &mem_P[0]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_PACK[0])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_PACK], 1, sizeof(cl_mem), &mem_P[1]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_PACK[1])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_PACK], 2, sizeof(cl_mem), &mem_P[2]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_PACK[2])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_PACK], 4, 2 * sizeof(cl_int), &idmn);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_PACK[4])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_UNPACK], 0, sizeof(cl_mem), &mem_P[0]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_UNPACK[0])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_UNPACK], 1, sizeof(cl_mem), &mem_P[1]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_UNPACK[1])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_UNPACK], 2, sizeof(cl_mem), &mem_P[2]);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_UNPACK[2])", ret, env);
        ret = clSetKernelArg(kernel[NLM_SPATIAL_UNPACK], 4, 2 * sizeof(cl_int), &idmn);
        oclErrorCheck("clSetKernelArg(NLM_SPATIAL_UNPACK[4])", ret, env);
    }

}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthInit
#ifdef VAPOURSYNTH_H
static void VS_CC VapourSynthPluginViInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    NLMVapoursynth *d = (NLMVapoursynth*) * instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
}
#endif //__VAPOURSYNTH_H__

//////////////////////////////////////////
// AviSynthGetFrame
#ifdef __AVISYNTH_6_H__
PVideoFrame __stdcall _NLMAvisynth::GetFrame(int n, IScriptEnvironment* env) {
    // Variables.  
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame ref = (baby) ? baby->GetFrame(n, env) : nullptr;
    PVideoFrame dst = env->NewVideoFrame(vi);
    const int maxframe = vi.num_frames - 1;
    const cl_int t = d;
    const cl_float pattern_u0 = 0.0f;
    const cl_float pattern_u3 = CL_FLT_EPSILON;
    const size_t size_u0 = sizeof(cl_float) * idmn[0] * idmn[1] * ((clip_t & COLOR_GRAY) ? 2 : 4);
    const size_t size_u3 = sizeof(cl_float) * idmn[0] * idmn[1];
    const size_t origin[3] = { 0, 0, 0 };
    const size_t region[3] = { (size_t) idmn[0], (size_t) idmn[1], 1 };
    const size_t region_p[3] = { (size_t) idmn[0], (size_t) idmn[2], 1 };
    const size_t global_work[2] = {
        mrounds((size_t) idmn[0], fastmax(HRZ_BLOCK_X, VRT_BLOCK_X)),
        mrounds((size_t) idmn[1], fastmax(HRZ_BLOCK_Y, VRT_BLOCK_Y))
    };
    const size_t local_horiz[2] = { HRZ_BLOCK_X, HRZ_BLOCK_Y };
    const size_t local_vert[2] = { VRT_BLOCK_X, VRT_BLOCK_Y };

    // Copy chroma.   
    if (!vi.IsY8() && (clip_t & COLOR_GRAY)) {
        env->BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), src->GetReadPtr(PLANAR_U),
            src->GetPitch(PLANAR_U), src->GetRowSize(PLANAR_U), src->GetHeight(PLANAR_U));
        env->BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), src->GetReadPtr(PLANAR_V),
            src->GetPitch(PLANAR_V), src->GetRowSize(PLANAR_V), src->GetHeight(PLANAR_V));
    }

    // Processing.
    cl_int ret = CL_SUCCESS;
    cl_command_queue command_queue = clCreateCommandQueue(context, deviceID, 0, NULL);
    ret |= clEnqueueFillBuffer(command_queue, mem_U[0], &pattern_u0, sizeof(cl_float), 0, size_u0, 0, NULL, NULL);
    ret |= clEnqueueFillBuffer(command_queue, mem_U[3], &pattern_u3, sizeof(cl_float), 0, size_u3, 0, NULL, NULL);
    if (d) {
        for (int k = -d; k <= d; k++) {
            src = child->GetFrame(clamp(n + k, 0, maxframe), env);
            ref = (baby) ? baby->GetFrame(clamp(n + k, 0, maxframe), env) : nullptr;
            const cl_int t_pk = t + k;
            const size_t origin_in[3] = { 0, 0, (size_t) t_pk };
            switch (clip_t) {
                case (EXTRA_FALSE | CLIP_UNORM | COLOR_GRAY):
                    ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin_in, region,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    break;
                case (EXTRA_TRUE | CLIP_UNORM | COLOR_GRAY):
                    ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin_in, region,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_in[1], CL_TRUE, origin_in, region,
                        (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    break;
                case (EXTRA_FALSE | CLIP_STACKED | COLOR_GRAY):
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[NLM_PACK], 3, sizeof(cl_mem), &mem_in[0]);
                    ret |= clSetKernelArg(kernel[NLM_PACK], 4, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_PACK],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (EXTRA_TRUE | CLIP_STACKED | COLOR_GRAY):
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[NLM_PACK], 3, sizeof(cl_mem), &mem_in[0]);
                    ret |= clSetKernelArg(kernel[NLM_PACK], 4, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_PACK],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                        (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[NLM_PACK], 3, sizeof(cl_mem), &mem_in[1]);
                    ret |= clSetKernelArg(kernel[NLM_PACK], 4, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_PACK],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (EXTRA_FALSE | CLIP_UNORM | COLOR_YUV):
                case (EXTRA_FALSE | CLIP_STACKED | COLOR_YUV):
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[NLM_PACK], 3, sizeof(cl_mem), &mem_in[0]);
                    ret |= clSetKernelArg(kernel[NLM_PACK], 4, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_PACK],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (EXTRA_TRUE | CLIP_UNORM | COLOR_YUV):
                case (EXTRA_TRUE | CLIP_STACKED | COLOR_YUV):
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[NLM_PACK], 3, sizeof(cl_mem), &mem_in[0]);
                    ret |= clSetKernelArg(kernel[NLM_PACK], 4, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_PACK],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                        (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                        (size_t) ref->GetPitch(PLANAR_U), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region_p,
                        (size_t) ref->GetPitch(PLANAR_V), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[NLM_PACK], 3, sizeof(cl_mem), &mem_in[1]);
                    ret |= clSetKernelArg(kernel[NLM_PACK], 4, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_PACK],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (EXTRA_FALSE | CLIP_UNORM | COLOR_RGB):
                    ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin_in, region,
                        (size_t) src->GetPitch(), 0, src->GetReadPtr(), 0, NULL, NULL);
                    break;
                case (EXTRA_TRUE | CLIP_UNORM | COLOR_RGB):
                    ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin_in, region,
                        (size_t) src->GetPitch(), 0, src->GetReadPtr(), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_in[1], CL_TRUE, origin_in, region,
                        (size_t) ref->GetPitch(), 0, ref->GetReadPtr(), 0, NULL, NULL);
                    break;
                default:
                    env->ThrowError("KNLMeansCL: fatal error!\n (AviSynthGetFrame)");
                    break;
            }
        }
        for (int k = -d; k <= 0; k++) {
            for (int j = -a; j <= a; j++) {
                for (int i = -a; i <= a; i++) {
                    if (k * (2 * a + 1) * (2 * a + 1) + j * (2 * a + 1) + i < 0) {
                        const cl_int q[4] = { i, j, k, 0 };
                        ret |= clSetKernelArg(kernel[NLM_DISTANCE_LEFT], 3, 4 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_DISTANCE_LEFT],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        ret |= clSetKernelArg(kernel[NLM_HORIZONTAL], 2, sizeof(cl_int), &t);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_HORIZONTAL],
                            2, NULL, global_work, local_horiz, 0, NULL, NULL);
                        ret |= clSetKernelArg(kernel[NLM_VERTICAL], 2, sizeof(cl_int), &t);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_VERTICAL],
                            2, NULL, global_work, local_vert, 0, NULL, NULL);
                        if (k) {
                            const cl_int t_mq = t - k;
                            ret |= clSetKernelArg(kernel[NLM_DISTANCE_RIGHT], 3, 4 * sizeof(cl_int), &q);
                            ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_DISTANCE_RIGHT],
                                2, NULL, global_work, NULL, 0, NULL, NULL);
                            ret |= clSetKernelArg(kernel[NLM_HORIZONTAL], 2, sizeof(cl_int), &t_mq);
                            ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_HORIZONTAL],
                                2, NULL, global_work, local_horiz, 0, NULL, NULL);
                            ret |= clSetKernelArg(kernel[NLM_VERTICAL], 2, sizeof(cl_int), &t_mq);
                            ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_VERTICAL],
                                2, NULL, global_work, local_vert, 0, NULL, NULL);
                        }
                        ret |= clSetKernelArg(kernel[NLM_ACCUMULATION], 5, 4 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_ACCUMULATION],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                    }
                }
            }
        }
        ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_FINISH], 2, NULL, global_work, NULL, 0, NULL, NULL);
    } else {
        switch (clip_t) {
            case (EXTRA_FALSE | CLIP_UNORM | COLOR_GRAY):
                ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                break;
            case (EXTRA_TRUE | CLIP_UNORM | COLOR_GRAY):
                ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_in[1], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                break;
            case (EXTRA_FALSE | CLIP_STACKED | COLOR_GRAY):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[NLM_SPATIAL_PACK], 3, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_SPATIAL_PACK],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (EXTRA_TRUE | CLIP_STACKED | COLOR_GRAY):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[NLM_SPATIAL_PACK], 3, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_SPATIAL_PACK],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                    (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[NLM_SPATIAL_PACK], 3, sizeof(cl_mem), &mem_in[1]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_SPATIAL_PACK],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (EXTRA_FALSE | CLIP_UNORM | COLOR_YUV):
            case (EXTRA_FALSE | CLIP_STACKED | COLOR_YUV):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[NLM_SPATIAL_PACK], 3, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_SPATIAL_PACK],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (EXTRA_TRUE | CLIP_UNORM | COLOR_YUV):
            case (EXTRA_TRUE | CLIP_STACKED | COLOR_YUV):
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[NLM_SPATIAL_PACK], 3, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_SPATIAL_PACK],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                    (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                    (size_t) ref->GetPitch(PLANAR_U), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region_p,
                    (size_t) ref->GetPitch(PLANAR_V), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[NLM_SPATIAL_PACK], 3, sizeof(cl_mem), &mem_in[1]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_SPATIAL_PACK],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (EXTRA_FALSE | CLIP_UNORM | COLOR_RGB):
                ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(), 0, src->GetReadPtr(), 0, NULL, NULL);
                break;
            case (EXTRA_TRUE | CLIP_UNORM | COLOR_RGB):
                ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(), 0, src->GetReadPtr(), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_in[1], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(), 0, ref->GetReadPtr(), 0, NULL, NULL);
                break;
            default:
                env->ThrowError("KNLMeansCL: fatal error!\n (AviSynthGetFrame)");
                break;
        }
        for (int j = -a; j <= 0; j++) {
            for (int i = -a; i <= a; i++) {
                if (j * (2 * a + 1) + i < 0) {
                    const cl_int q[2] = { i, j };
                    ret |= clSetKernelArg(kernel[NLM_SPATIAL_DISTANCE], 3, 2 * sizeof(cl_int), &q);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_SPATIAL_DISTANCE],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_SPATIAL_HORIZONTAL],
                        2, NULL, global_work, local_horiz, 0, NULL, NULL);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_SPATIAL_VERTICAL],
                        2, NULL, global_work, local_vert, 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[NLM_SPATIAL_ACCUMULATION], 5, 2 * sizeof(cl_int), &q);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_SPATIAL_ACCUMULATION],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                }
            }
        }
        ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_SPATIAL_FINISH], 2, NULL, global_work, NULL, 0, NULL, NULL);
    }
    switch (clip_t) {
        case (EXTRA_FALSE | CLIP_UNORM | COLOR_GRAY):
        case (EXTRA_TRUE | CLIP_UNORM | COLOR_GRAY):
            ret |= clEnqueueReadImage(command_queue, mem_out, CL_TRUE, origin, region,
                (size_t) dst->GetPitch(PLANAR_Y), 0, dst->GetWritePtr(PLANAR_Y), 0, NULL, NULL);
            break;
        case (EXTRA_FALSE | CLIP_STACKED | COLOR_GRAY):
        case (EXTRA_TRUE | CLIP_STACKED | COLOR_GRAY):
            ret |= clSetKernelArg(kernel[NLM_UNPACK], 3, sizeof(cl_mem), &mem_out);
            ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_UNPACK],
                2, NULL, global_work, NULL, 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                (size_t) dst->GetPitch(PLANAR_Y), 0, dst->GetWritePtr(PLANAR_Y), 0, NULL, NULL);
            break;
        case (EXTRA_FALSE | CLIP_UNORM | COLOR_YUV):
        case (EXTRA_TRUE | CLIP_UNORM | COLOR_YUV):
        case (EXTRA_FALSE | CLIP_STACKED | COLOR_YUV):
        case (EXTRA_TRUE | CLIP_STACKED | COLOR_YUV):
            ret |= clSetKernelArg(kernel[NLM_UNPACK], 3, sizeof(cl_mem), &mem_out);
            ret |= clEnqueueNDRangeKernel(command_queue, kernel[NLM_UNPACK],
                2, NULL, global_work, NULL, 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                (size_t) dst->GetPitch(PLANAR_Y), 0, dst->GetWritePtr(PLANAR_Y), 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                (size_t) dst->GetPitch(PLANAR_U), 0, dst->GetWritePtr(PLANAR_U), 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[2], CL_TRUE, origin, region_p,
                (size_t) dst->GetPitch(PLANAR_V), 0, dst->GetWritePtr(PLANAR_V), 0, NULL, NULL);
            break;
        case (EXTRA_FALSE | CLIP_UNORM | COLOR_RGB):
        case (EXTRA_TRUE | CLIP_UNORM | COLOR_RGB):
            ret |= clEnqueueReadImage(command_queue, mem_out, CL_TRUE, origin, region,
                (size_t) dst->GetPitch(), 0, dst->GetWritePtr(), 0, NULL, NULL);
            break;
        default:
            env->ThrowError("KNLMeansCL: fatal error!\n (AviSynthGetFrame)");
            break;
    }
    ret |= clFinish(command_queue);
    ret |= clReleaseCommandQueue(command_queue);
    if (ret != CL_SUCCESS) env->ThrowError("KNLMeansCL: fatal error!\n (AviSynthGetFrame)");

    // Info.
    if (info) {
        uint8_t y = 0, *frm = dst->GetWritePtr(PLANAR_Y);
        int pitch = dst->GetPitch(PLANAR_Y);
        char buffer[2048], str[2048], str1[2048];
        DrawString(frm, pitch, 0, y++, "KNLMeansCL");
        DrawString(frm, pitch, 0, y++, " Version " VERSION);
        DrawString(frm, pitch, 0, y++, " Copyright(C) Khanattila");
        snprintf(buffer, 2048, " D:%i  A:%ix%i  S:%ix%i", 2 * d + 1, 2 * a + 1, 2 * a + 1, 2 * s + 1, 2 * s + 1);
        DrawString(frm, pitch, 0, y++, buffer);
        snprintf(buffer, 2048, " Num of ref pixels: %i", (2 * d + 1)*(2 * a + 1)*(2 * a + 1) - 1);
        DrawString(frm, pitch, 0, y++, buffer);
        snprintf(buffer, 2048, " Global work size: %lux%lu", (unsigned long) global_work[0], (unsigned long) global_work[1]);
        DrawString(frm, pitch, 0, y++, buffer);
        snprintf(buffer, 2048, " Number of devices: %u", num_devices);
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
        // Variables.
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx), *ref;
        const VSFormat *fi = d->vi->format;
        VSFrameRef *dst;
        const cl_int t = int64ToIntS(d->d);
        const cl_float pattern_u0 = 0.0f;
        const cl_float pattern_u3 = CL_FLT_EPSILON;
        const size_t size_u0 = sizeof(cl_float) * d->idmn[0] * d->idmn[1] * ((d->clip_t & COLOR_GRAY) ? 2 : 4);
        const size_t size_u3 = sizeof(cl_float) * d->idmn[0] * d->idmn[1];
        const size_t origin[3] = { 0, 0, 0 };
        const size_t region[3] = { (size_t) d->idmn[0], (size_t) d->idmn[1], 1 };
        const size_t global_work[2] = {
            mrounds((size_t) d->idmn[0], fastmax(d->HRZ_BLOCK_X, d->VRT_BLOCK_X)),
            mrounds((size_t) d->idmn[1], fastmax(d->HRZ_BLOCK_Y, d->VRT_BLOCK_Y))
        };
        const size_t local_horiz[2] = { d->HRZ_BLOCK_X, d->HRZ_BLOCK_Y };
        const size_t local_vert[2] = { d->VRT_BLOCK_X, d->VRT_BLOCK_Y };

        //Copy chroma.  
        if (fi->colorFamily == cmYUV && (d->clip_t & COLOR_GRAY)) {
            const VSFrameRef * planeSrc[] = { NULL, src, src };
            const int planes[] = { 0, 1, 2 };
            dst = vsapi->newVideoFrame2(fi, d->idmn[0], d->idmn[1], planeSrc, planes, src, core);
        } else {
            dst = vsapi->newVideoFrame(fi, d->idmn[0], d->idmn[1], src, core);
        }
        vsapi->freeFrame(src);

        // Processing.
        cl_int ret = CL_SUCCESS;
        cl_command_queue command_queue = clCreateCommandQueue(d->context, d->deviceID, 0, NULL);
        ret |= clEnqueueFillBuffer(command_queue, d->mem_U[0], &pattern_u0, sizeof(cl_float), 0, size_u0, 0, NULL, NULL);
        ret |= clEnqueueFillBuffer(command_queue, d->mem_U[3], &pattern_u3, sizeof(cl_float), 0, size_u3, 0, NULL, NULL);
        if (d->d) {
            for (int k = int64ToIntS(-d->d); k <= d->d; k++) {
                src = vsapi->getFrameFilter(clamp(n + k, 0, maxframe), d->node, frameCtx);
                ref = (d->knot) ? vsapi->getFrameFilter(clamp(n + k, 0, maxframe), d->knot, frameCtx) : nullptr;
                const cl_int t_pk = t + k;
                const size_t origin_in[3] = { 0, 0, (size_t) t_pk };
                switch (d->clip_t) {
                    case (EXTRA_FALSE | CLIP_UNORM | COLOR_GRAY):
                        ret |= clEnqueueWriteImage(command_queue, d->mem_in[0], CL_TRUE, origin_in, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        break;
                    case (EXTRA_TRUE | CLIP_UNORM | COLOR_GRAY):
                        ret |= clEnqueueWriteImage(command_queue, d->mem_in[0], CL_TRUE, origin_in, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_in[1], CL_TRUE, origin_in, region,
                            (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                        break;
                    case (EXTRA_FALSE | CLIP_UNSIGNED | COLOR_GRAY):
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[NLM_PACK], 3, sizeof(cl_mem), &d->mem_in[0]);
                        ret |= clSetKernelArg(d->kernel[NLM_PACK], 4, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_PACK],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        break;
                    case (EXTRA_TRUE | CLIP_UNSIGNED | COLOR_GRAY):
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[NLM_PACK], 3, sizeof(cl_mem), &d->mem_in[0]);
                        ret |= clSetKernelArg(d->kernel[NLM_PACK], 4, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_PACK],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[NLM_PACK], 3, sizeof(cl_mem), &d->mem_in[1]);
                        ret |= clSetKernelArg(d->kernel[NLM_PACK], 4, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_PACK],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        break;
                    case (EXTRA_FALSE | CLIP_UNORM | COLOR_YUV):
                    case (EXTRA_FALSE | CLIP_UNSIGNED | COLOR_YUV):
                    case (EXTRA_FALSE | CLIP_UNORM | COLOR_RGB):
                    case (EXTRA_FALSE | CLIP_UNSIGNED | COLOR_RGB):
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[NLM_PACK], 3, sizeof(cl_mem), &d->mem_in[0]);
                        ret |= clSetKernelArg(d->kernel[NLM_PACK], 4, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_PACK],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        break;
                    case (EXTRA_TRUE | CLIP_UNORM | COLOR_YUV):
                    case (EXTRA_TRUE | CLIP_UNSIGNED | COLOR_YUV):
                    case (EXTRA_TRUE | CLIP_UNORM | COLOR_RGB):
                    case (EXTRA_TRUE | CLIP_UNSIGNED | COLOR_RGB):
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[NLM_PACK], 3, sizeof(cl_mem), &d->mem_in[0]);
                        ret |= clSetKernelArg(d->kernel[NLM_PACK], 4, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_PACK],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 1), 0, vsapi->getReadPtr(ref, 1), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 2), 0, vsapi->getReadPtr(ref, 2), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[NLM_PACK], 3, sizeof(cl_mem), &d->mem_in[1]);
                        ret |= clSetKernelArg(d->kernel[NLM_PACK], 4, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_PACK],
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
                        if (k * (2 * int64ToIntS(d->a) + 1) * (2 * int64ToIntS(d->a) + 1) + j * (2 * int64ToIntS(d->a) + 1) + i < 0) {
                            const cl_int q[4] = { i, j, k, 0 };
                            ret |= clSetKernelArg(d->kernel[NLM_DISTANCE_LEFT], 3, 4 * sizeof(cl_int), &q);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_DISTANCE_LEFT],
                                2, NULL, global_work, NULL, 0, NULL, NULL);
                            ret |= clSetKernelArg(d->kernel[NLM_HORIZONTAL], 2, sizeof(cl_int), &t);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_HORIZONTAL],
                                2, NULL, global_work, local_horiz, 0, NULL, NULL);
                            ret |= clSetKernelArg(d->kernel[NLM_VERTICAL], 2, sizeof(cl_int), &t);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_VERTICAL],
                                2, NULL, global_work, local_vert, 0, NULL, NULL);
                            if (k) {
                                const cl_int t_mq = t - k;
                                ret |= clSetKernelArg(d->kernel[NLM_DISTANCE_RIGHT], 3, 4 * sizeof(cl_int), &q);
                                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_DISTANCE_RIGHT],
                                    2, NULL, global_work, NULL, 0, NULL, NULL);
                                ret |= clSetKernelArg(d->kernel[NLM_HORIZONTAL], 2, sizeof(cl_int), &t_mq);
                                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_HORIZONTAL],
                                    2, NULL, global_work, local_horiz, 0, NULL, NULL);
                                ret |= clSetKernelArg(d->kernel[NLM_VERTICAL], 2, sizeof(cl_int), &t_mq);
                                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_VERTICAL],
                                    2, NULL, global_work, local_vert, 0, NULL, NULL);
                            }
                            ret |= clSetKernelArg(d->kernel[NLM_ACCUMULATION], 5, 4 * sizeof(cl_int), &q);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_ACCUMULATION],
                                2, NULL, global_work, NULL, 0, NULL, NULL);
                        }
                    }
                }
            }
            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_FINISH], 2, NULL, global_work, NULL, 0, NULL, NULL);
        } else {
            src = vsapi->getFrameFilter(n, d->node, frameCtx);
            ref = (d->knot) ? vsapi->getFrameFilter(n, d->knot, frameCtx) : nullptr;
            switch (d->clip_t) {
                case (EXTRA_FALSE | CLIP_UNORM | COLOR_GRAY):
                    ret |= clEnqueueWriteImage(command_queue, d->mem_in[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    break;
                case (EXTRA_TRUE | CLIP_UNORM | COLOR_GRAY):
                    ret |= clEnqueueWriteImage(command_queue, d->mem_in[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_in[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                    break;
                case (EXTRA_FALSE | CLIP_UNSIGNED | COLOR_GRAY):
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[NLM_SPATIAL_PACK], 3, sizeof(cl_mem), &d->mem_in[0]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_SPATIAL_PACK],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (EXTRA_TRUE | CLIP_UNSIGNED | COLOR_GRAY):
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[NLM_SPATIAL_PACK], 3, sizeof(cl_mem), &d->mem_in[0]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_SPATIAL_PACK],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[NLM_SPATIAL_PACK], 3, sizeof(cl_mem), &d->mem_in[1]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_SPATIAL_PACK],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (EXTRA_FALSE | CLIP_UNORM | COLOR_YUV):
                case (EXTRA_FALSE | CLIP_UNSIGNED | COLOR_YUV):
                case (EXTRA_FALSE | CLIP_UNORM | COLOR_RGB):
                case (EXTRA_FALSE | CLIP_UNSIGNED | COLOR_RGB):
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[NLM_SPATIAL_PACK], 3, sizeof(cl_mem), &d->mem_in[0]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_SPATIAL_PACK],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (EXTRA_TRUE | CLIP_UNORM | COLOR_YUV):
                case (EXTRA_TRUE | CLIP_UNSIGNED | COLOR_YUV):
                case (EXTRA_TRUE | CLIP_UNORM | COLOR_RGB):
                case (EXTRA_TRUE | CLIP_UNSIGNED | COLOR_RGB):
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[NLM_SPATIAL_PACK], 3, sizeof(cl_mem), &d->mem_in[0]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_SPATIAL_PACK],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(ref, 1), 0, vsapi->getReadPtr(ref, 1), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(ref, 2), 0, vsapi->getReadPtr(ref, 2), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[NLM_SPATIAL_PACK], 3, sizeof(cl_mem), &d->mem_in[1]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_SPATIAL_PACK],
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
                        ret |= clSetKernelArg(d->kernel[NLM_SPATIAL_DISTANCE], 3, 2 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_SPATIAL_DISTANCE],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_SPATIAL_HORIZONTAL],
                            2, NULL, global_work, local_horiz, 0, NULL, NULL);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_SPATIAL_VERTICAL],
                            2, NULL, global_work, local_vert, 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[NLM_SPATIAL_ACCUMULATION], 5, 2 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_SPATIAL_ACCUMULATION],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                    }
                }
            }
            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_SPATIAL_FINISH], 2, NULL, global_work, NULL, 0, NULL, NULL);
        }
        switch (d->clip_t) {
            case (EXTRA_FALSE | CLIP_UNORM | COLOR_GRAY):
            case (EXTRA_TRUE | CLIP_UNORM | COLOR_GRAY):
                ret |= clEnqueueReadImage(command_queue, d->mem_out, CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(dst, 0), 0, vsapi->getWritePtr(dst, 0), 0, NULL, NULL);
                break;
            case (EXTRA_FALSE | CLIP_UNSIGNED | COLOR_GRAY):
            case (EXTRA_TRUE | CLIP_UNSIGNED | COLOR_GRAY):
                ret |= clSetKernelArg(d->kernel[NLM_UNPACK], 3, sizeof(cl_mem), &d->mem_out);
                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_UNPACK], 2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(dst, 0), 0, vsapi->getWritePtr(dst, 0), 0, NULL, NULL);
                break;
            case (EXTRA_FALSE | CLIP_UNORM | COLOR_YUV):
            case (EXTRA_TRUE | CLIP_UNORM | COLOR_YUV):
            case (EXTRA_FALSE | CLIP_UNSIGNED | COLOR_YUV):
            case (EXTRA_TRUE | CLIP_UNSIGNED | COLOR_YUV):
            case (EXTRA_FALSE | CLIP_UNORM | COLOR_RGB):
            case (EXTRA_TRUE | CLIP_UNORM | COLOR_RGB):
            case (EXTRA_FALSE | CLIP_UNSIGNED | COLOR_RGB):
            case (EXTRA_TRUE | CLIP_UNSIGNED | COLOR_RGB):
                ret |= clSetKernelArg(d->kernel[NLM_UNPACK], 3, sizeof(cl_mem), &d->mem_out);
                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[NLM_UNPACK], 2, NULL, global_work, NULL, 0, NULL, NULL);
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
        ret |= clFinish(command_queue);
        ret |= clReleaseCommandQueue(command_queue);
        if (ret != CL_SUCCESS) {
            vsapi->setFilterError("knlm.KNLMeansCL: fatal error!\n (VapourSynthGetFrame)", frameCtx);
            vsapi->freeFrame(dst);
            return 0;
        }

        // Info.
        if (d->info) {
            uint8_t y = 0, *frm = vsapi->getWritePtr(dst, 0);
            int pitch = vsapi->getStride(dst, 0);
            char buffer[2048], str[2048], str1[2048];
            DrawString(frm, pitch, 0, y++, "KNLMeansCL");
            DrawString(frm, pitch, 0, y++, " Version " VERSION);
            DrawString(frm, pitch, 0, y++, " Copyright(C) Khanattila");
            snprintf(buffer, 2048, " D:%i  A:%ix%i  S:%ix%i", 2 * int64ToIntS(d->d) + 1, 2 * int64ToIntS(d->a) + 1,
                2 * int64ToIntS(d->a) + 1, 2 * int64ToIntS(d->s) + 1, 2 * int64ToIntS(d->s) + 1);
            DrawString(frm, pitch, 0, y++, buffer);
            snprintf(buffer, 2048, " Num of ref pixels: %i", (2 * int64ToIntS(d->d) + 1)*(2 * int64ToIntS(d->a) + 1)*
                (2 * int64ToIntS(d->a) + 1) - 1);
            DrawString(frm, pitch, 0, y++, buffer);
            snprintf(buffer, 2048, " Global work size: %lux%lu", (unsigned long) global_work[0], (unsigned long) global_work[1]);
            DrawString(frm, pitch, 0, y++, buffer);
            snprintf(buffer, 2048, " Number of devices: %u", d->num_devices);
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
        clReleaseKernel(kernel[NLM_UNPACK]);
        clReleaseKernel(kernel[NLM_PACK]);
        clReleaseKernel(kernel[NLM_FINISH]);
        clReleaseKernel(kernel[NLM_ACCUMULATION]);
        clReleaseKernel(kernel[NLM_VERTICAL]);
        clReleaseKernel(kernel[NLM_HORIZONTAL]);
        clReleaseKernel(kernel[NLM_DISTANCE_RIGHT]);
        clReleaseKernel(kernel[NLM_DISTANCE_LEFT]);
    } else {
        clReleaseKernel(kernel[NLM_SPATIAL_UNPACK]);
        clReleaseKernel(kernel[NLM_SPATIAL_PACK]);
        clReleaseKernel(kernel[NLM_SPATIAL_FINISH]);
        clReleaseKernel(kernel[NLM_SPATIAL_ACCUMULATION]);
        clReleaseKernel(kernel[NLM_SPATIAL_VERTICAL]);
        clReleaseKernel(kernel[NLM_SPATIAL_HORIZONTAL]);
        clReleaseKernel(kernel[NLM_SPATIAL_DISTANCE]);
    }
    clReleaseProgram(program);
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
        clReleaseKernel(d->kernel[NLM_UNPACK]);
        clReleaseKernel(d->kernel[NLM_PACK]);
        clReleaseKernel(d->kernel[NLM_FINISH]);
        clReleaseKernel(d->kernel[NLM_ACCUMULATION]);
        clReleaseKernel(d->kernel[NLM_VERTICAL]);
        clReleaseKernel(d->kernel[NLM_HORIZONTAL]);
        clReleaseKernel(d->kernel[NLM_DISTANCE_RIGHT]);
        clReleaseKernel(d->kernel[NLM_DISTANCE_LEFT]);
    } else {
        clReleaseKernel(d->kernel[NLM_SPATIAL_UNPACK]);
        clReleaseKernel(d->kernel[NLM_SPATIAL_PACK]);
        clReleaseKernel(d->kernel[NLM_SPATIAL_FINISH]);
        clReleaseKernel(d->kernel[NLM_SPATIAL_ACCUMULATION]);
        clReleaseKernel(d->kernel[NLM_SPATIAL_VERTICAL]);
        clReleaseKernel(d->kernel[NLM_SPATIAL_HORIZONTAL]);
        clReleaseKernel(d->kernel[NLM_SPATIAL_DISTANCE]);
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
    return new _NLMAvisynth(args[0].AsClip(), args[1].AsInt(DFT_d), args[2].AsInt(DFT_a), args[3].AsInt(DFT_s), args[4].AsFloat(DFT_h),
        args[5].AsBool(DFT_cmode), args[6].AsInt(DFT_wmode), args[7].AsFloat(DFT_wref), args[8].Defined() ? args[8].AsClip() : nullptr,
        args[9].AsString(DFT_ocl_device), args[10].AsInt(DFT_ocl_id), args[11].AsBool(DFT_lsb), args[12].AsBool(DFT_info), env);
}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthCreate
#ifdef VAPOURSYNTH_H

static void VS_CC VapourSynthPluginCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {

    // Checks source clip and rclip.
    NLMVapoursynth d;
    cl_channel_order channel_order = 0;
    cl_channel_type channel_type = 0;
    int err;
    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.knot = vsapi->propGetNode(in, "rclip", 0, &err);
    if (err) {
        d.knot = nullptr;
        d.clip_t = EXTRA_FALSE;
    } else d.clip_t = EXTRA_TRUE;
    d.vi = vsapi->getVideoInfo(d.node);
    d.bit_shift = d.vi->format->bytesPerSample * 8u - d.vi->format->bitsPerSample;
    if (isConstantFormat(d.vi)) {
        d.cmode = vsapi->propGetInt(in, "cmode", 0, &err);
        if (err) d.cmode = DFT_cmode;
        if (d.cmode && (d.vi->format->subSamplingW != 0) && (d.vi->format->subSamplingH != 0)) {
            vsapi->setError(out, "knlm.KNLMeansCL: 'cmode' requires 4:4:4 subsampling!");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.knot);
            return;
        }
        switch (d.vi->format->id) {
            case VSPresetFormat::pfGray8:
            case VSPresetFormat::pfYUV420P8:
            case VSPresetFormat::pfYUV422P8:
            case VSPresetFormat::pfYUV410P8:
            case VSPresetFormat::pfYUV411P8:
            case VSPresetFormat::pfYUV440P8:
                d.clip_t |= (CLIP_UNORM | COLOR_GRAY);
                channel_order = CL_LUMINANCE;
                channel_type = CL_UNORM_INT8;
                break;
            case VSPresetFormat::pfYUV420P9:
            case VSPresetFormat::pfYUV422P9:
            case VSPresetFormat::pfYUV420P10:
            case VSPresetFormat::pfYUV422P10:
                d.clip_t |= (CLIP_UNSIGNED | COLOR_GRAY);
                channel_order = CL_LUMINANCE;
                channel_type = CL_UNORM_INT16;
                break;
            case VSPresetFormat::pfGray16:
            case VSPresetFormat::pfYUV420P16:
            case VSPresetFormat::pfYUV422P16:
                d.clip_t |= (CLIP_UNORM | COLOR_GRAY);
                channel_order = CL_LUMINANCE;
                channel_type = CL_UNORM_INT16;
                break;
            case VSPresetFormat::pfGrayH:
                d.clip_t |= (CLIP_UNORM | COLOR_GRAY);
                channel_order = CL_LUMINANCE;
                channel_type = CL_HALF_FLOAT;
                break;
            case VSPresetFormat::pfGrayS:
                d.clip_t |= (CLIP_UNORM | COLOR_GRAY);
                channel_order = CL_LUMINANCE;
                channel_type = CL_FLOAT;
                break;
            case VSPresetFormat::pfYUV444P8:
                d.clip_t |= d.cmode ? (CLIP_UNORM | COLOR_YUV) : (CLIP_UNORM | COLOR_GRAY);
                channel_order = (cl_channel_order) (d.cmode ? CL_RGBA : CL_LUMINANCE);
                channel_type = CL_UNORM_INT8;
                break;
            case VSPresetFormat::pfYUV444P9:
            case VSPresetFormat::pfYUV444P10:
                d.clip_t |= d.cmode ? (CLIP_UNSIGNED | COLOR_YUV) : (CLIP_UNSIGNED | COLOR_GRAY);
                channel_order = (cl_channel_order) (d.cmode ? CL_RGBA : CL_LUMINANCE);
                channel_type = CL_UNORM_INT16;
                break;
            case VSPresetFormat::pfYUV444P16:
                d.clip_t |= d.cmode ? (CLIP_UNORM | COLOR_YUV) : (CLIP_UNORM | COLOR_GRAY);
                channel_order = (cl_channel_order) (d.cmode ? CL_RGBA : CL_LUMINANCE);
                channel_type = CL_UNORM_INT16;
                break;
            case VSPresetFormat::pfYUV444PH:
                d.clip_t |= d.cmode ? (CLIP_UNORM | COLOR_YUV) : (CLIP_UNORM | COLOR_GRAY);
                channel_order = (cl_channel_order) (d.cmode ? CL_RGBA : CL_LUMINANCE);
                channel_type = CL_HALF_FLOAT;
                break;
            case VSPresetFormat::pfYUV444PS:
                d.clip_t |= d.cmode ? (CLIP_UNORM | COLOR_YUV) : (CLIP_UNORM | COLOR_GRAY);
                channel_order = (cl_channel_order) (d.cmode ? CL_RGBA : CL_LUMINANCE);
                channel_type = CL_FLOAT;
                break;
            case VSPresetFormat::pfRGB24:
                d.clip_t |= (CLIP_UNORM | COLOR_RGB);
                channel_order = CL_RGBA;
                channel_type = CL_UNORM_INT8;
                break;
            case VSPresetFormat::pfRGB27:
            case VSPresetFormat::pfRGB30:
                d.clip_t |= (CLIP_UNSIGNED | COLOR_RGB);
                channel_order = CL_RGBA;
                channel_type = CL_UNORM_INT16;
                break;
            case VSPresetFormat::pfRGB48:
                d.clip_t |= (CLIP_UNORM | COLOR_RGB);
                channel_order = CL_RGBA;
                channel_type = CL_UNORM_INT16;
                break;
            case VSPresetFormat::pfRGBH:
                d.clip_t |= (CLIP_UNORM | COLOR_RGB);
                channel_order = CL_RGBA;
                channel_type = CL_HALF_FLOAT;
                break;
            case VSPresetFormat::pfRGBS:
                d.clip_t |= (CLIP_UNORM | COLOR_RGB);
                channel_order = CL_RGBA;
                channel_type = CL_FLOAT;
                break;
            default:
                vsapi->setError(out, "knlm.KNLMeansCL: video format not supported!");
                vsapi->freeNode(d.node);
                vsapi->freeNode(d.knot);
                return;
        }
    } else {
        vsapi->setError(out, "knlm.KNLMeansCL: only constant format!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (d.knot && !d.equals(d.vi, vsapi->getVideoInfo(d.knot))) {
        vsapi->setError(out, "knlm.KNLMeansCL: 'rclip' do not math source clip!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }

    // Sets default value.
    d.d = vsapi->propGetInt(in, "d", 0, &err);
    if (err) d.d = DFT_d;
    d.a = vsapi->propGetInt(in, "a", 0, &err);
    if (err) d.a = DFT_a;
    d.s = vsapi->propGetInt(in, "s", 0, &err);
    if (err) d.s = DFT_s;
    d.h = vsapi->propGetFloat(in, "h", 0, &err);
    if (err) d.h = DFT_h;
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

    // Checks user value.
    if (d.info && d.vi->format->bitsPerSample != 8) {
        vsapi->setError(out, "knlm.KNLMeansCL: 'info' requires Gray8 or YUVP8 color space!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
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
    if (d.wmode < 0 || d.wmode > 2) {
        vsapi->setError(out, "knlm.KNLMeansCL: 'wmode' must be in range [0, 2]!");
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
    ocl_utils_device_type ocl_device_type = 0;
    if (!strcasecmp(d.ocl_device, "CPU")) {
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_CPU;
    } else if (!strcasecmp(d.ocl_device, "GPU")) {
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_GPU;
    } else if (!strcasecmp(d.ocl_device, "ACCELERATOR")) {
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_ACCELERATOR;
    } else if (!strcasecmp(d.ocl_device, "AUTO")) {
        ocl_device_type = OCL_UTILS_DEVICE_TYPE_AUTO;
    } else {
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

    // Gets PlatformID and DeviceID.
    cl_device_type deviceTYPE;
    cl_int ret = oclUtilsGetPlaformDeviceIDs(OCL_UTILS_OPENCL_1_2, ocl_device_type, 
        (cl_uint) d.ocl_id, &d.platformID, &d.deviceID, &deviceTYPE);
    if (ret == OCL_UTILS_NO_DEVICE_AVAILABLE) {
        vsapi->setError(out, "knlm.KNLMeansCL: no compatible opencl platforms available!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    } else if (ret != CL_SUCCESS) {
        d.oclErrorCheck("oclUtilsGetPlaformDeviceIDs", ret, out, vsapi);
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }

    // Sets local work size
    switch (deviceTYPE) {
        case CL_DEVICE_TYPE_CPU:
            d.HRZ_BLOCK_X = 4;
            d.HRZ_BLOCK_Y = 8;
            d.VRT_BLOCK_X = 8;
            d.VRT_BLOCK_Y = 4;
            break;
        case CL_DEVICE_TYPE_GPU:
        case CL_DEVICE_TYPE_ACCELERATOR:
            d.HRZ_BLOCK_X = 16; 
            d.HRZ_BLOCK_Y = 8;
            d.VRT_BLOCK_X = 8; 
            d.VRT_BLOCK_Y = 16;
            break;        
        default:
            vsapi->setError(out, "knlm.KNLMeansCL: fatal error!\n (Local work size)");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.knot);
            return;
    }

    // Creates an OpenCL context, 2D images and buffers object.
    d.idmn[0] = d.vi->width;
    d.idmn[1] = d.vi->height;
    d.context = clCreateContext(NULL, 1, &d.deviceID, NULL, NULL, &ret);
    if (ret != CL_SUCCESS) {
        d.oclErrorCheck("clCreateContext", ret, out, vsapi);
        return;
    }
    const cl_image_format image_format = { channel_order, channel_type };
    const cl_image_desc image_desc = { (cl_mem_object_type) (d.d ? CL_MEM_OBJECT_IMAGE2D_ARRAY : CL_MEM_OBJECT_IMAGE2D),
        (size_t) d.idmn[0], (size_t) d.idmn[1], 1, 2 * (size_t) d.d + 1, 0, 0, 0, 0, NULL };
    const cl_image_desc image_desc_out = { CL_MEM_OBJECT_IMAGE2D, (size_t) d.idmn[0], (size_t) d.idmn[1], 1, 1, 0, 0, 0, 0, NULL };
    if ((d.clip_t & COLOR_GRAY) && !d.bit_shift) {
        d.mem_in[0] = clCreateImage(d.context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, &image_format, &image_desc, NULL, &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateImage(d.mem_in[0])", ret, out, vsapi);
            return;
        }
        d.mem_in[1] = clCreateImage(d.context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, &image_format, &image_desc, NULL, &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateImage(d.mem_in[1])", ret, out, vsapi);
            return;
        }
        d.mem_out = clCreateImage(d.context, CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY, &image_format, &image_desc_out, NULL, &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateImage(d.mem_out)", ret, out, vsapi);
            return;
        }
    } else {
        d.mem_in[0] = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format, &image_desc, NULL, &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateImage(d.mem_in[0])", ret, out, vsapi);
            return;
        }
        d.mem_in[1] = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format, &image_desc, NULL, &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateImage(d.mem_in[1])", ret, out, vsapi);
            return;
        }
        d.mem_out = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format, &image_desc_out, NULL, &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateImage(d.mem_out)", ret, out, vsapi);
            return;
        }
    }
    const cl_image_format image_format_u = { CL_LUMINANCE, CL_HALF_FLOAT };
    const size_t size_u0 = sizeof(cl_float) * d.idmn[0] * d.idmn[1] * ((d.clip_t & COLOR_GRAY) ? 2 : 4);
    const size_t size_u3 = sizeof(cl_float) * d.idmn[0] * d.idmn[1];
    d.mem_U[0] = clCreateBuffer(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u0, NULL, &ret);
    if (ret != CL_SUCCESS) {
        d.oclErrorCheck("clCreateBuffer(d.mem_U[0])", ret, out, vsapi);
        return;
    }
    d.mem_U[1] = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format_u, &image_desc, NULL, &ret);
    if (ret != CL_SUCCESS) {
        d.oclErrorCheck("clCreateImage(d.mem_U[1])", ret, out, vsapi);
        return;
    }
    d.mem_U[2] = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format_u, &image_desc, NULL, &ret);
    if (ret != CL_SUCCESS) {
        d.oclErrorCheck("clCreateImage(d.mem_U[2])", ret, out, vsapi);
        return;
    }
    d.mem_U[3] = clCreateBuffer(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u3, NULL, &ret);
    if (ret != CL_SUCCESS) {
        d.oclErrorCheck("clCreateBuffer(d.mem_U[3])", ret, out, vsapi);
        return;
    }
    if (d.bit_shift) {
        const cl_image_format image_format_p = { CL_R, CL_UNSIGNED_INT16 };
        d.mem_P[0] = clCreateImage(d.context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_out, NULL, &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateImage(d.mem_P[0])", ret, out, vsapi);
            return;
        }
        d.mem_P[1] = clCreateImage(d.context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_out, NULL, &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateImage(d.mem_P[1])", ret, out, vsapi);
            return;
        }
        d.mem_P[2] = clCreateImage(d.context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_out, NULL, &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateImage(d.mem_P[2])", ret, out, vsapi);
            return;
        }
    } else {
        const cl_image_format image_format_p = { CL_LUMINANCE, channel_type };
        d.mem_P[0] = clCreateImage(d.context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_out, NULL, &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateImage(d.mem_P[0])", ret, out, vsapi);
            return;
        }
        d.mem_P[1] = clCreateImage(d.context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_out, NULL, &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateImage(d.mem_P[1])", ret, out, vsapi);
            return;
        }
        d.mem_P[2] = clCreateImage(d.context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_out, NULL, &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateImage(d.mem_P[2])", ret, out, vsapi);
            return;
        }
    }

    // Creates and Build a program executable from the program source.
    d.program = clCreateProgramWithSource(d.context, 1, d.d ? &kernel_source_code : &kernel_source_code_spatial, NULL, NULL);
    char options[2048];
    setlocale(LC_ALL, "C");
#    ifdef __APPLE__
    snprintf(options, 2048, "-cl-denorms-are-zero -cl-fast-relaxed-math -cl-mad-enable "
        "-D HRZ_BLOCK_X=%u -D HRZ_BLOCK_Y=%u -D VRT_BLOCK_X=%u -D VRT_BLOCK_Y=%u -D NLMK_TCLIP=%u -D NLMK_S=%i "
        "-D NLMK_WMODE=%i -D NLMK_WREF=%ff -D NLMK_D=%i -D NLMK_H2_INV_NORM=%ff -D NLMK_BIT_SHIFT=%u",
        d.H_BLOCK_X, d.H_BLOCK_Y, d.V_BLOCK_X, d.V_BLOCK_Y, d.clip_t, int64ToIntS(d.s),
        int64ToIntS(d.wmode), d.wref, int64ToIntS(d.d), 65025.0 / (3 * d.h * d.h * (2 * d.s + 1)*(2 * d.s + 1)), d.bit_shift);
#    else
    snprintf(options, 2048, "-cl-single-precision-constant -cl-denorms-are-zero -cl-fast-relaxed-math -Werror "
        "-D HRZ_BLOCK_X=%u -D HRZ_BLOCK_Y=%u -D VRT_BLOCK_X=%u -D VRT_BLOCK_Y=%u -D NLMK_TCLIP=%u -D NLMK_S=%i "
        "-D NLMK_WMODE=%i -D NLMK_WREF=%f -D NLMK_D=%i -D NLMK_H2_INV_NORM=%f -D NLMK_BIT_SHIFT=%u",
        d.HRZ_BLOCK_X, d.HRZ_BLOCK_Y, d.VRT_BLOCK_X, d.VRT_BLOCK_Y, d.clip_t, int64ToIntS(d.s),
        int64ToIntS(d.wmode), d.wref, int64ToIntS(d.d), 65025.0 / (3 * d.h * d.h * (2 * d.s + 1)*(2 * d.s + 1)), d.bit_shift);
#    endif
    ret = clBuildProgram(d.program, 1, &d.deviceID, options, NULL, NULL);
    if (ret != CL_SUCCESS) {
        size_t options_size, log_size;
        clGetProgramBuildInfo(d.program, d.deviceID, CL_PROGRAM_BUILD_OPTIONS, 0, NULL, &options_size);
        clGetProgramBuildInfo(d.program, d.deviceID, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char *options_txt = (char*) malloc(options_size);
        char *log_txt = (char*) malloc(log_size);
        clGetProgramBuildInfo(d.program, d.deviceID, CL_PROGRAM_BUILD_OPTIONS, options_size, options_txt, NULL);
        clGetProgramBuildInfo(d.program, d.deviceID, CL_PROGRAM_BUILD_LOG, log_size, log_txt, NULL);
        std::ofstream outfile("Log-KNLMeansCL.txt", std::ofstream::out);
        outfile << "---------------------------------" << std::endl;
        outfile << "*** Error in OpenCL compiler ***" << std::endl;
        outfile << "---------------------------------" << std::endl;
        outfile << std::endl << "# Build Options" << std::endl;
        outfile << options_txt << std::endl;
        outfile << std::endl << "# Build Log" << std::endl;
        outfile << log_txt << std::endl;
        outfile.close();
        free(log_txt);
        free(options_txt);
        vsapi->setError(out, "knlm.KNLMeansCL: fatal error!\n (clBuildProgram: please report Log-KNLMeansCL.txt.)");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    setlocale(LC_ALL, "");

    // Creates kernel objects.
    if (d.d) {
        d.kernel[NLM_DISTANCE_LEFT] = clCreateKernel(d.program, "NLM_DISTANCE_LEFT", &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_DISTANCE_LEFT)", ret, out, vsapi);
            return;
        }
        d.kernel[NLM_DISTANCE_RIGHT] = clCreateKernel(d.program, "NLM_DISTANCE_RIGHT", &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_DISTANCE_RIGHT)", ret, out, vsapi);
            return;
        }
        d.kernel[NLM_HORIZONTAL] = clCreateKernel(d.program, "NLM_HORIZONTAL", &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_HORIZONTAL)", ret, out, vsapi);
            return;
        }
        d.kernel[NLM_VERTICAL] = clCreateKernel(d.program, "NLM_VERTICAL", &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_VERTICAL)", ret, out, vsapi);
            return;
        }
        d.kernel[NLM_ACCUMULATION] = clCreateKernel(d.program, "NLM_ACCUMULATION", &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_ACCUMULATION)", ret, out, vsapi);
            return;
        }
        d.kernel[NLM_FINISH] = clCreateKernel(d.program, "NLM_FINISH", &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_FINISH)", ret, out, vsapi);
            return;
        }
        d.kernel[NLM_PACK] = clCreateKernel(d.program, "NLM_PACK", &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_PACK)", ret, out, vsapi);
            return;
        }
        d.kernel[NLM_UNPACK] = clCreateKernel(d.program, "NLM_UNPACK", &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_UNPACK)", ret, out, vsapi);
            return;
        }
    } else {
        d.kernel[NLM_SPATIAL_DISTANCE] = clCreateKernel(d.program, "NLM_SPATIAL_DISTANCE", &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_DISTANCE)", ret, out, vsapi);
            return;
        }
        d.kernel[NLM_SPATIAL_HORIZONTAL] = clCreateKernel(d.program, "NLM_SPATIAL_HORIZONTAL", &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_HORIZONTAL)", ret, out, vsapi);
            return;
        }
        d.kernel[NLM_SPATIAL_VERTICAL] = clCreateKernel(d.program, "NLM_SPATIAL_VERTICAL", &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_VERTICAL)", ret, out, vsapi);
            return;
        }
        d.kernel[NLM_SPATIAL_ACCUMULATION] = clCreateKernel(d.program, "NLM_SPATIAL_ACCUMULATION", &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_ACCUMULATION)", ret, out, vsapi);
            return;
        }
        d.kernel[NLM_SPATIAL_FINISH] = clCreateKernel(d.program, "NLM_SPATIAL_FINISH", &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_FINISH)", ret, out, vsapi);
            return;
        }
        d.kernel[NLM_SPATIAL_PACK] = clCreateKernel(d.program, "NLM_SPATIAL_PACK", &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_PACK)", ret, out, vsapi);
            return;
        }
        d.kernel[NLM_SPATIAL_UNPACK] = clCreateKernel(d.program, "NLM_SPATIAL_UNPACK", &ret);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_UNPACK)", ret, out, vsapi);
            return;
        }
    }

    // Sets kernel arguments.
    if (d.d) {
        ret = clSetKernelArg(d.kernel[NLM_DISTANCE_LEFT], 0, sizeof(cl_mem), &d.mem_in[(d.clip_t & EXTRA_FALSE) ? 0 : 1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_DISTANCE_LEFT[0])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_DISTANCE_LEFT], 1, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_DISTANCE_LEFT[1])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_DISTANCE_LEFT], 2, 2 * sizeof(cl_int), &d.idmn);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_DISTANCE_LEFT[2])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_DISTANCE_RIGHT], 0, sizeof(cl_mem), &d.mem_in[(d.clip_t & EXTRA_FALSE) ? 0 : 1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_DISTANCE_RIGHT[0])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_DISTANCE_RIGHT], 1, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_DISTANCE_RIGHT[1])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_DISTANCE_RIGHT], 2, 2 * sizeof(cl_int), &d.idmn);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_DISTANCE_RIGHT[2])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_HORIZONTAL], 0, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_HORIZONTAL[0])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_HORIZONTAL], 1, sizeof(cl_mem), &d.mem_U[2]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_HORIZONTAL[1])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_HORIZONTAL], 3, 2 * sizeof(cl_int), &d.idmn);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_HORIZONTAL[3])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_VERTICAL], 0, sizeof(cl_mem), &d.mem_U[2]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_VERTICAL[0])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_VERTICAL], 1, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_VERTICAL[1])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_VERTICAL], 3, 2 * sizeof(cl_int), &d.idmn);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_VERTICAL[3])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_ACCUMULATION], 0, sizeof(cl_mem), &d.mem_in[0]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_ACCUMULATION[0])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_ACCUMULATION], 1, sizeof(cl_mem), &d.mem_U[0]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_ACCUMULATION[1])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_ACCUMULATION], 2, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_ACCUMULATION[2])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_ACCUMULATION], 3, sizeof(cl_mem), &d.mem_U[3]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_ACCUMULATION[3])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_ACCUMULATION], 4, 2 * sizeof(cl_int), &d.idmn);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_ACCUMULATION[4])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_FINISH], 0, sizeof(cl_mem), &d.mem_in[0]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_FINISH[0])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_FINISH], 1, sizeof(cl_mem), &d.mem_out);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_FINISH[1])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_FINISH], 2, sizeof(cl_mem), &d.mem_U[0]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_FINISH[2])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_FINISH], 3, sizeof(cl_mem), &d.mem_U[3]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_FINISH[3])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_FINISH], 4, 2 * sizeof(cl_int), &d.idmn);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_FINISH[4])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_PACK], 0, sizeof(cl_mem), &d.mem_P[0]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_PACK[0])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_PACK], 1, sizeof(cl_mem), &d.mem_P[1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_PACK[1])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_PACK], 2, sizeof(cl_mem), &d.mem_P[2]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_PACK[2])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_PACK], 5, 2 * sizeof(cl_int), &d.idmn);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_PACK[5])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_UNPACK], 0, sizeof(cl_mem), &d.mem_P[0]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_UNPACK[0])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_UNPACK], 1, sizeof(cl_mem), &d.mem_P[1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_UNPACK[1])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_UNPACK], 2, sizeof(cl_mem), &d.mem_P[2]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_UNPACK[2])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_UNPACK], 4, 2 * sizeof(cl_int), &d.idmn);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_UNPACK[4])", ret, out, vsapi);
            return;
        }
    } else {
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_DISTANCE], 0, sizeof(cl_mem), &d.mem_in[(d.clip_t & EXTRA_FALSE) ? 0 : 1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_DISTANCE[0])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_DISTANCE], 1, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_DISTANCE[1])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_DISTANCE], 2, 2 * sizeof(cl_int), &d.idmn);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_DISTANCE[2])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_HORIZONTAL], 0, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_HORIZONTAL[0])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_HORIZONTAL], 1, sizeof(cl_mem), &d.mem_U[2]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_HORIZONTAL[1])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_HORIZONTAL], 2, 2 * sizeof(cl_int), &d.idmn);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_HORIZONTAL[2])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_VERTICAL], 0, sizeof(cl_mem), &d.mem_U[2]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_VERTICAL[0])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_VERTICAL], 1, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_VERTICAL[1])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_VERTICAL], 2, 2 * sizeof(cl_int), &d.idmn);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_VERTICAL[2])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_ACCUMULATION], 0, sizeof(cl_mem), &d.mem_in[0]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_ACCUMULATION[0])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_ACCUMULATION], 1, sizeof(cl_mem), &d.mem_U[0]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_ACCUMULATION[1])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_ACCUMULATION], 2, sizeof(cl_mem), &d.mem_U[1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_ACCUMULATION[2])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_ACCUMULATION], 3, sizeof(cl_mem), &d.mem_U[3]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_ACCUMULATION[3])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_ACCUMULATION], 4, 2 * sizeof(cl_int), &d.idmn);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_ACCUMULATION[4])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_FINISH], 0, sizeof(cl_mem), &d.mem_in[0]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_FINISH[0])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_FINISH], 1, sizeof(cl_mem), &d.mem_out);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_FINISH[1])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_FINISH], 2, sizeof(cl_mem), &d.mem_U[0]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_FINISH[2])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_FINISH], 3, sizeof(cl_mem), &d.mem_U[3]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_FINISH[3])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_FINISH], 4, 2 * sizeof(cl_int), &d.idmn);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_FINISH[4])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_PACK], 0, sizeof(cl_mem), &d.mem_P[0]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_PACK[0])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_PACK], 1, sizeof(cl_mem), &d.mem_P[1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_PACK[1])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_PACK], 2, sizeof(cl_mem), &d.mem_P[2]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_PACK[2])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_PACK], 4, 2 * sizeof(cl_int), &d.idmn);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_PACK[4])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_UNPACK], 0, sizeof(cl_mem), &d.mem_P[0]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_UNPACK[0])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_UNPACK], 1, sizeof(cl_mem), &d.mem_P[1]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_UNPACK[1])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_UNPACK], 2, sizeof(cl_mem), &d.mem_P[2]);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_UNPACK[2])", ret, out, vsapi);
            return;
        }
        ret = clSetKernelArg(d.kernel[NLM_SPATIAL_UNPACK], 4, 2 * sizeof(cl_int), &d.idmn);
        if (ret != CL_SUCCESS) {
            d.oclErrorCheck("clCreateKernel(NLM_SPATIAL_UNPACK[4])", ret, out, vsapi);
            return;
        }
    }

    // Creates a new filter and returns a reference to it.
    NLMVapoursynth *data = (NLMVapoursynth*) malloc(sizeof(d));
    if (data) *data = d;
    else {
        vsapi->setError(out, "knlm.KNLMeansCL: fatal error!\n (malloc fail)");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    vsapi->createFilter(in, out, "KNLMeansCL", VapourSynthPluginViInit, VapourSynthPluginGetFrame,
        VapourSynthPluginFree, fmParallelRequests, 0, data, core);
}
#endif //__VAPOURSYNTH_H__

//////////////////////////////////////////
// AviSynthPluginInit
#ifdef __AVISYNTH_6_H__
const AVS_Linkage *AVS_linkage = 0;
extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage * const vectors) {
    AVS_linkage = vectors;
    env->AddFunction("KNLMeansCL", "c[d]i[a]i[s]i[h]f[cmode]b[wmode]i[wref]f[rclip]c[device_type]s[device_id]i[lsb_inout]b[info]b",
        AviSynthPluginCreate, 0);
    return "KNLMeansCL for AviSynth";
}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthPluginInit
#ifdef VAPOURSYNTH_H
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.Khanattila.KNLMeansCL", "knlm", "KNLMeansCL for VapourSynth", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("KNLMeansCL", "clip:clip;d:int:opt;a:int:opt;s:int:opt;h:float:opt;cmode:int:opt;wmode:int:opt;wref:float:opt;\
rclip:clip:opt;device_type:data:opt;device_id:int:opt;info:int:opt", VapourSynthPluginCreate, nullptr, plugin);
}
#endif //__VAPOURSYNTH_H__