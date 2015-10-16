/*
*	This file is part of KNLMeansCL,
*	Copyright(C) 2015  Edoardo Brunetti.
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

#define DFT_D           1
#define DFT_A           2
#define DFT_S           4
#define DFT_cmode       false
#define DFT_wmode       1
#define DFT_h           1.2f
#define DFT_ocl_device "AUTO"
#define DFT_ocl_id      0
#define DFT_lsb         false
#define DFT_info        false

#ifdef _WIN32
    #define _stdcall __stdcall
#endif

#ifdef _MSC_VER
    #define snprintf _snprintf_s
    #define strcasecmp _stricmp 
#endif

#include "KNLMeansCL.h"

//////////////////////////////////////////
// AviSynthEquals
#ifdef __AVISYNTH_6_H__
inline bool KNLMeansClass::equals(VideoInfo *v, VideoInfo *w) {
    return v->width == w->width && v->height == w->height && v->fps_numerator == w->fps_numerator
        && v->fps_denominator == w->fps_denominator && v->num_frames == w->num_frames;
}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthEquals
#ifdef VAPOURSYNTH_H
inline bool KNLMeansData::equals(const VSVideoInfo *v, const VSVideoInfo *w) {
    return v->width == w->width && v->height == w->height && v->fpsNum == w->fpsNum && 
        v->fpsDen == w->fpsDen && v->numFrames == w->numFrames && v->format == w->format;
}
#endif //__VAPOURSYNTH_H__

//////////////////////////////////////////
// AviSynthInit
#ifdef __AVISYNTH_6_H__
KNLMeansClass::KNLMeansClass(PClip _child, const int _d, const int _a, const int _s, const bool _cmode, const int _wmode, const double _h,
    PClip _baby, const char* _ocl_device, const int _ocl_id, const bool _lsb, const bool _info, IScriptEnvironment* env) :
    GenericVideoFilter(_child), d(_d), a(_a), s(_s), cmode(_cmode), wmode(_wmode), h(_h), baby(_baby), ocl_device(_ocl_device),
    ocl_id(_ocl_id), lsb(_lsb), info(_info) {

    // Checks AviSynth Version.
    env->CheckVersion(5);
    child->SetCacheHints(CACHE_WINDOW, d);

    // Checks source clip and rclip.
    cl_channel_order channel_order = 0;
    cl_channel_type channel_type = 0;
    if (!vi.IsPlanar() && !vi.IsRGB32())
        env->ThrowError("KNLMeansCL: planar YUV or RGB32 data!");
    if (cmode && !vi.IsYV24() && !vi.IsRGB32())
        env->ThrowError("KNLMeansCL: cmode requires 4:4:4 subsampling!");
    if (baby) {
        VideoInfo rvi = baby->GetVideoInfo();
        if (!equals(&vi, &rvi))
            env->ThrowError("KNLMeansCL: rclip do not math source clip!");
        baby->SetCacheHints(CACHE_WINDOW, d);
        clip_t = EXTRA_CLIP;
    } else clip_t = EXTRA_NONE;
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
        env->ThrowError("KNLMeansCL: d must be greater than or equal to 0!");
    if (a < 1)
        env->ThrowError("KNLMeansCL: a must be greater than or equal to 1!");
    if (s < 0 || s > 4)
        env->ThrowError("KNLMeansCL: s must be in range [0, 4]!");
    if (wmode < 0 || wmode > 2)
        env->ThrowError("KNLMeansCL: wmode must be in range [0, 2]!");
    if (h <= 0.0f)
        env->ThrowError("KNLMeansCL: h must be greater than 0!");
    cl_device_type device_type = 0;
    bool device_auto = false;
    if (!strcasecmp(ocl_device, "CPU")) {
        device_type = CL_DEVICE_TYPE_CPU;
    } else if (!strcasecmp(ocl_device, "GPU")) {
        device_type = CL_DEVICE_TYPE_GPU;
    } else if (!strcasecmp(ocl_device, "ACCELERATOR")) {
        device_type = CL_DEVICE_TYPE_ACCELERATOR;
    } else if (!strcasecmp(ocl_device, "ALL")) {
        device_type = CL_DEVICE_TYPE_ALL;
    } else if (!strcasecmp(ocl_device, "DEFAULT")) {
        device_type = CL_DEVICE_TYPE_DEFAULT;
    } else if (!strcasecmp(ocl_device, "AUTO")) {
        device_auto = true;
    } else {
        env->ThrowError("KNLMeansCL: device_type must be cpu, gpu, accelerator, all, default or auto!");
    }
    if (ocl_id < 0)
        env->ThrowError("KNLMeansCL: device_id must be greater than or equal to 0!");
    if (info && vi.IsRGB())
        env->ThrowError("KNLMeansCL: info requires YUV color space!");

    // Gets PlatformID and DeviceID.
    cl_int ret = CL_SUCCESS;
    if (device_auto) {
        device_type = CL_DEVICE_TYPE_GPU;
        ret |= oclGetDevicesList(device_type, NULL, &num_devices);
        if (num_devices == 0) {
            device_type = CL_DEVICE_TYPE_ACCELERATOR;
            ret |= oclGetDevicesList(device_type, NULL, &num_devices);
            if (num_devices == 0) {
                device_type = CL_DEVICE_TYPE_CPU;
                ret |= oclGetDevicesList(device_type, NULL, &num_devices);
                if (num_devices == 0) {
                    device_type = CL_DEVICE_TYPE_DEFAULT;
                    ret |= oclGetDevicesList(device_type, NULL, &num_devices);
                    if (num_devices == 0) {
                        device_type = CL_DEVICE_TYPE_ALL;
                        ret |= oclGetDevicesList(device_type, NULL, &num_devices);
                        if (num_devices == 0) 
                            env->ThrowError("KNLMeansCL: no opencl devices available!");           
                    } else if (ocl_id >= (int) num_devices)
                        env->ThrowError("KNLMeansCL: selected device is not available!");
                } else if (ocl_id >= (int) num_devices)
                    env->ThrowError("KNLMeansCL: selected device is not available!");
            } else if (ocl_id >= (int) num_devices)
                env->ThrowError("KNLMeansCL: selected device is not available!");
        } else if (ocl_id >= (int) num_devices)
            env->ThrowError("KNLMeansCL: selected device is not available!");
        ocl_device_packed *devices = (ocl_device_packed*) malloc(sizeof(ocl_device_packed) * num_devices);
        ret |= oclGetDevicesList(device_type, devices, NULL);
        if (ret != CL_SUCCESS)
            env->ThrowError("KNLMeansCL: AviSynthCreate error (oclGetDevicesList)!");
        platformID = devices[ocl_id].platform;
        deviceID = devices[ocl_id].device;
        free(devices);
    } else {
        cl_int ret = oclGetDevicesList(device_type, NULL, &num_devices);
        if (num_devices == 0)
            env->ThrowError("KNLMeansCL: no opencl devices available!");
        else if (ocl_id >= (int) num_devices)
            env->ThrowError("KNLMeansCL: selected device is not available!");
        ocl_device_packed *devices = (ocl_device_packed*) malloc(sizeof(ocl_device_packed) * num_devices);
        ret |= oclGetDevicesList(device_type, devices, NULL);
        if (ret != CL_SUCCESS)
            env->ThrowError("KNLMeansCL: AviSynthCreate error (oclGetDevicesList)!");
        platformID = devices[ocl_id].platform;
        deviceID = devices[ocl_id].device;
        free(devices);
    }
    
    // Creates an OpenCL context, 2D images and buffers object.
    idmn[0] = vi.width;
    idmn[1] = lsb ? (vi.height / 2) : (vi.height);
    idmn[2] = vi.height;
    context = clCreateContext(NULL, 1, &deviceID, NULL, NULL, NULL);
    const cl_image_format image_format = { channel_order, channel_type };
    const cl_image_desc image_desc = { (cl_mem_object_type) (d ? CL_MEM_OBJECT_IMAGE2D_ARRAY : CL_MEM_OBJECT_IMAGE2D),
        (size_t) idmn[0], (size_t) idmn[1], 1, 2 * (size_t) d + 1, 0, 0, 0, 0, NULL };
    const cl_image_desc image_desc_out = { CL_MEM_OBJECT_IMAGE2D, (size_t) idmn[0], (size_t) idmn[1], 1, 1, 0, 0, 0, 0, NULL };
    if (!(clip_t & COLOR_YUV) && !lsb) {
        mem_in[0] = clCreateImage(context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, &image_format, &image_desc, NULL, NULL);
        mem_in[1] = clCreateImage(context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, &image_format, &image_desc, NULL, NULL);
        mem_out = clCreateImage(context, CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY, &image_format, &image_desc_out, NULL, NULL);
    } else {
        mem_in[0] = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format, &image_desc, NULL, NULL);
        mem_in[1] = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format, &image_desc, NULL, NULL);
        mem_out = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format, &image_desc_out, NULL, NULL);
    }
    const cl_image_format image_format_u = { CL_LUMINANCE, CL_HALF_FLOAT };
    const size_t size_u0 = sizeof(cl_float) * idmn[0] * idmn[1] * ((clip_t & COLOR_GRAY) ? 2 : 4);
    const size_t size_u3 = sizeof(cl_float) * idmn[0] * idmn[1];
    mem_U[0] = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u0, NULL, NULL);
    mem_U[1] = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format_u, &image_desc, NULL, NULL);
    mem_U[2] = clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format_u, &image_desc, NULL, NULL);
    mem_U[3] = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u3, NULL, NULL);
    const cl_image_format image_format_p = { CL_LUMINANCE, CL_UNORM_INT8 };
    const cl_image_desc image_desc_p = { CL_MEM_OBJECT_IMAGE2D, (size_t) idmn[0], (size_t) idmn[2], 1, 1, 0, 0, 0, 0, NULL };
    mem_P[0] = clCreateImage(context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_p, NULL, NULL);
    mem_P[1] = clCreateImage(context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_p, NULL, NULL);
    mem_P[2] = clCreateImage(context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_p, NULL, NULL);
        
    // Creates and Build a program executable from the program source.
    program = clCreateProgramWithSource(context, 1, &kernel_source_code, NULL, NULL);
    char options[2048];
    setlocale(LC_ALL, "C");
    snprintf(options, 2048, "-cl-single-precision-constant -cl-denorms-are-zero -cl-fast-relaxed-math -Werror "
        "-D H_BLOCK_X=%i -D H_BLOCK_Y=%i -D V_BLOCK_X=%i -D V_BLOCK_Y=%i -D NLMK_TCLIP=%u -D NLMK_S=%i "
        "-D NLMK_WMODE=%i -D NLMK_D=%i -D NLMK_H2_INV_NORM=%f -D NLMK_BIT_SHIFT=%u",
        H_BLOCK_X, H_BLOCK_Y, V_BLOCK_X, V_BLOCK_Y, clip_t, s,
        wmode, d, 65025.0 / (3*h*h*(2*s+1)*(2*s+1)), 0u);
    ret = clBuildProgram(program, 1, &deviceID, options, NULL, NULL);
    if (ret != CL_SUCCESS) {
        size_t options_size, log_size;
        clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_OPTIONS, 0, NULL, &options_size);
        clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char *options = (char*) malloc(options_size);
        char *log = (char*) malloc(log_size);
        clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_OPTIONS, options_size, options, NULL);
        clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        std::ofstream outfile("Log-KNLMeansCL.txt", std::ofstream::out);
        outfile << "---------------------------------" << std::endl;
        outfile << "*** Error in OpenCL compiler ***" << std::endl;
        outfile << "---------------------------------" << std::endl;
        outfile << std::endl << "# Build Options" << std::endl;
        outfile << options << std::endl;
        outfile << std::endl << "# Build Log" << std::endl;
        outfile << log << std::endl;
        outfile.close();
        free(log);
        env->ThrowError("KNLMeansCL: AviSynthCreate error (clBuildProgram)!\n Please report Log-KNLMeansCL.txt.");
    }
    setlocale(LC_ALL, "");

    // Creates kernel objects.
    kernel[nlmSpatialDistance] = clCreateKernel(program, "nlmSpatialDistance", NULL);
    kernel[nlmSpatialHorizontal] = clCreateKernel(program, "nlmSpatialHorizontal", NULL);
    kernel[nlmSpatialVertical] = clCreateKernel(program, "nlmSpatialVertical", NULL);
    kernel[nlmSpatialAccumulation] = clCreateKernel(program, "nlmSpatialAccumulation", NULL);
    kernel[nlmSpatialFinish] = clCreateKernel(program, "nlmSpatialFinish", NULL);
    kernel[nlmDistanceLeft] = clCreateKernel(program, "nlmDistanceLeft", NULL);
    kernel[nlmDistanceRight] = clCreateKernel(program, "nlmDistanceRight", NULL);
    kernel[nlmHorizontal] = clCreateKernel(program, "nlmHorizontal", NULL);
    kernel[nlmVertical] = clCreateKernel(program, "nlmVertical", NULL);
    kernel[nlmAccumulation] = clCreateKernel(program, "nlmAccumulation", NULL);
    kernel[nlmFinish] = clCreateKernel(program, "nlmFinish", NULL);
    kernel[nlmSpatialPack] = clCreateKernel(program, "nlmSpatialPack", NULL);
    kernel[nlmPack] = clCreateKernel(program, "nlmPack", NULL);
    kernel[nlmUnpack] = clCreateKernel(program, "nlmUnpack", NULL);

    // Sets kernel arguments.
    if (d) {
        ret = clSetKernelArg(kernel[nlmDistanceLeft], 0, sizeof(cl_mem), &mem_in[(clip_t & EXTRA_NONE) ? 0 : 1]);
        ret |= clSetKernelArg(kernel[nlmDistanceLeft], 1, sizeof(cl_mem), &mem_U[1]);
        ret |= clSetKernelArg(kernel[nlmDistanceLeft], 2, 2 * sizeof(cl_int), &idmn);
        ret |= clSetKernelArg(kernel[nlmDistanceRight], 0, sizeof(cl_mem), &mem_in[(clip_t & EXTRA_NONE) ? 0 : 1]);
        ret |= clSetKernelArg(kernel[nlmDistanceRight], 1, sizeof(cl_mem), &mem_U[1]);
        ret |= clSetKernelArg(kernel[nlmDistanceRight], 2, 2 * sizeof(cl_int), &idmn);
        ret |= clSetKernelArg(kernel[nlmHorizontal], 0, sizeof(cl_mem), &mem_U[1]);
        ret |= clSetKernelArg(kernel[nlmHorizontal], 1, sizeof(cl_mem), &mem_U[2]);
        ret |= clSetKernelArg(kernel[nlmHorizontal], 3, 2 * sizeof(cl_int), &idmn);
        ret |= clSetKernelArg(kernel[nlmVertical], 0, sizeof(cl_mem), &mem_U[2]);
        ret |= clSetKernelArg(kernel[nlmVertical], 1, sizeof(cl_mem), &mem_U[1]);
        ret |= clSetKernelArg(kernel[nlmVertical], 3, 2 * sizeof(cl_int), &idmn);
        ret |= clSetKernelArg(kernel[nlmAccumulation], 0, sizeof(cl_mem), &mem_in[0]);
        ret |= clSetKernelArg(kernel[nlmAccumulation], 1, sizeof(cl_mem), &mem_U[0]);
        ret |= clSetKernelArg(kernel[nlmAccumulation], 2, sizeof(cl_mem), &mem_U[1]);
        ret |= clSetKernelArg(kernel[nlmAccumulation], 3, sizeof(cl_mem), &mem_U[3]);
        ret |= clSetKernelArg(kernel[nlmAccumulation], 4, 2 * sizeof(cl_int), &idmn);
        ret |= clSetKernelArg(kernel[nlmFinish], 0, sizeof(cl_mem), &mem_in[0]);
        ret |= clSetKernelArg(kernel[nlmFinish], 1, sizeof(cl_mem), &mem_out);
        ret |= clSetKernelArg(kernel[nlmFinish], 2, sizeof(cl_mem), &mem_U[0]);
        ret |= clSetKernelArg(kernel[nlmFinish], 3, sizeof(cl_mem), &mem_U[3]);
        ret |= clSetKernelArg(kernel[nlmFinish], 4, 2 * sizeof(cl_int), &idmn);
        ret |= clSetKernelArg(kernel[nlmPack], 0, sizeof(cl_mem), &mem_P[0]);
        ret |= clSetKernelArg(kernel[nlmPack], 1, sizeof(cl_mem), &mem_P[1]);
        ret |= clSetKernelArg(kernel[nlmPack], 2, sizeof(cl_mem), &mem_P[2]);
        ret |= clSetKernelArg(kernel[nlmPack], 5, 2 * sizeof(cl_int), &idmn);
    } else {
        ret = clSetKernelArg(kernel[nlmSpatialDistance], 0, sizeof(cl_mem), &mem_in[(clip_t & EXTRA_NONE) ? 0 : 1]);
        ret |= clSetKernelArg(kernel[nlmSpatialDistance], 1, sizeof(cl_mem), &mem_U[1]);
        ret |= clSetKernelArg(kernel[nlmSpatialDistance], 2, 2 * sizeof(cl_int), &idmn);
        ret |= clSetKernelArg(kernel[nlmSpatialHorizontal], 0, sizeof(cl_mem), &mem_U[1]);
        ret |= clSetKernelArg(kernel[nlmSpatialHorizontal], 1, sizeof(cl_mem), &mem_U[2]);
        ret |= clSetKernelArg(kernel[nlmSpatialHorizontal], 2, 2 * sizeof(cl_int), &idmn);
        ret |= clSetKernelArg(kernel[nlmSpatialVertical], 0, sizeof(cl_mem), &mem_U[2]);
        ret |= clSetKernelArg(kernel[nlmSpatialVertical], 1, sizeof(cl_mem), &mem_U[1]);
        ret |= clSetKernelArg(kernel[nlmSpatialVertical], 2, 2 * sizeof(cl_int), &idmn);
        ret |= clSetKernelArg(kernel[nlmSpatialAccumulation], 0, sizeof(cl_mem), &mem_in[0]);
        ret |= clSetKernelArg(kernel[nlmSpatialAccumulation], 1, sizeof(cl_mem), &mem_U[0]);
        ret |= clSetKernelArg(kernel[nlmSpatialAccumulation], 2, sizeof(cl_mem), &mem_U[1]);
        ret |= clSetKernelArg(kernel[nlmSpatialAccumulation], 3, sizeof(cl_mem), &mem_U[3]);
        ret |= clSetKernelArg(kernel[nlmSpatialAccumulation], 4, 2 * sizeof(cl_int), &idmn);
        ret |= clSetKernelArg(kernel[nlmSpatialFinish], 0, sizeof(cl_mem), &mem_in[0]);
        ret |= clSetKernelArg(kernel[nlmSpatialFinish], 1, sizeof(cl_mem), &mem_out);
        ret |= clSetKernelArg(kernel[nlmSpatialFinish], 2, sizeof(cl_mem), &mem_U[0]);
        ret |= clSetKernelArg(kernel[nlmSpatialFinish], 3, sizeof(cl_mem), &mem_U[3]);
        ret |= clSetKernelArg(kernel[nlmSpatialFinish], 4, 2 * sizeof(cl_int), &idmn);
        ret |= clSetKernelArg(kernel[nlmSpatialPack], 0, sizeof(cl_mem), &mem_P[0]);
        ret |= clSetKernelArg(kernel[nlmSpatialPack], 1, sizeof(cl_mem), &mem_P[1]);
        ret |= clSetKernelArg(kernel[nlmSpatialPack], 2, sizeof(cl_mem), &mem_P[2]);
        ret |= clSetKernelArg(kernel[nlmSpatialPack], 4, 2 * sizeof(cl_int), &idmn);
    }
    ret |= clSetKernelArg(kernel[nlmUnpack], 0, sizeof(cl_mem), &mem_P[0]);
    ret |= clSetKernelArg(kernel[nlmUnpack], 1, sizeof(cl_mem), &mem_P[1]);
    ret |= clSetKernelArg(kernel[nlmUnpack], 2, sizeof(cl_mem), &mem_P[2]);
    ret |= clSetKernelArg(kernel[nlmUnpack], 3, sizeof(cl_mem), &mem_out);
    ret |= clSetKernelArg(kernel[nlmUnpack], 4, 2 * sizeof(cl_int), &idmn);
    if (ret != CL_SUCCESS) 	env->ThrowError("KNLMeansCL: AviSynthCreate error (clSetKernelArg)!");
}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthInit
#ifdef VAPOURSYNTH_H
static void VS_CC knlmeansInit(VSMap *in, VSMap *out, void **instanceData,
    VSNode *node, VSCore *core, const VSAPI *vsapi) {

    KNLMeansData *d = (KNLMeansData*) *instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
}
#endif //__VAPOURSYNTH_H__

//////////////////////////////////////////
// AviSynthGetFrame
#ifdef __AVISYNTH_6_H__
PVideoFrame __stdcall KNLMeansClass::GetFrame(int n, IScriptEnvironment* env) {
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
        mrounds((size_t) idmn[0], fastmax(H_BLOCK_X, V_BLOCK_X)),
        mrounds((size_t) idmn[1], fastmax(H_BLOCK_Y, V_BLOCK_Y)) 
    };
    const size_t local_horiz[2] = { H_BLOCK_X, H_BLOCK_Y };
    const size_t local_vert[2] = { V_BLOCK_X, V_BLOCK_Y };

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
                case (EXTRA_NONE | CLIP_UNORM | COLOR_GRAY) :
                    ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin_in, region,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    break;
                case (EXTRA_CLIP | CLIP_UNORM | COLOR_GRAY) :
                    ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin_in, region,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_in[1], CL_TRUE, origin_in, region,
                        (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    break;
                case (EXTRA_NONE | CLIP_STACKED | COLOR_GRAY) :
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 3, sizeof(cl_mem), &mem_in[0]);
                    ret |= clSetKernelArg(kernel[nlmPack], 4, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (EXTRA_CLIP | CLIP_STACKED | COLOR_GRAY) :
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 3, sizeof(cl_mem), &mem_in[0]);
                    ret |= clSetKernelArg(kernel[nlmPack], 4, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                        (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 3, sizeof(cl_mem), &mem_in[1]);
                    ret |= clSetKernelArg(kernel[nlmPack], 4, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (EXTRA_NONE | CLIP_UNORM | COLOR_YUV) :
                case (EXTRA_NONE | CLIP_STACKED | COLOR_YUV) :
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 3, sizeof(cl_mem), &mem_in[0]);
                    ret |= clSetKernelArg(kernel[nlmPack], 4, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (EXTRA_CLIP | CLIP_UNORM | COLOR_YUV) :
                case (EXTRA_CLIP | CLIP_STACKED | COLOR_YUV) :
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region_p,
                        (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 3, sizeof(cl_mem), &mem_in[0]);
                    ret |= clSetKernelArg(kernel[nlmPack], 4, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                        (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                        (size_t) ref->GetPitch(PLANAR_U), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region_p,
                        (size_t) ref->GetPitch(PLANAR_V), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmPack], 3, sizeof(cl_mem), &mem_in[1]);
                    ret |= clSetKernelArg(kernel[nlmPack], 4, sizeof(cl_int), &t_pk);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (EXTRA_NONE | CLIP_UNORM | COLOR_RGB) :
                    ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin_in, region,
                        (size_t) src->GetPitch(), 0, src->GetReadPtr(), 0, NULL, NULL);
                    break;
                case (EXTRA_CLIP | CLIP_UNORM | COLOR_RGB) :
                    ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin_in, region,
                        (size_t) src->GetPitch(), 0, src->GetReadPtr(), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, mem_in[1], CL_TRUE, origin_in, region,
                        (size_t) ref->GetPitch(), 0, ref->GetReadPtr(), 0, NULL, NULL);
                    break;
                default:
                    env->ThrowError("KNLMeansCL: AviSynthGetFrame error!");
                    break;
            }
        }
        for (int k = -d; k <= 0; k++) {
            for (int j = -a; j <= a; j++) {
                for (int i = -a; i <= a; i++) {
                    if (k * (2 * a + 1) * (2 * a + 1) + j * (2 * a + 1) + i < 0) {
                        const cl_int q[4] = { i, j, k, 0 };
                        ret |= clSetKernelArg(kernel[nlmDistanceLeft], 3, 4 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmDistanceLeft],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        ret |= clSetKernelArg(kernel[nlmHorizontal], 2, sizeof(cl_int), &t);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmHorizontal],
                            2, NULL, global_work, local_horiz, 0, NULL, NULL);
                        ret |= clSetKernelArg(kernel[nlmVertical], 2, sizeof(cl_int), &t);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmVertical],
                            2, NULL, global_work, local_vert, 0, NULL, NULL);
                        if (k) {
                            const cl_int t_mq = t - k;
                            ret |= clSetKernelArg(kernel[nlmDistanceRight], 3, 4 * sizeof(cl_int), &q);
                            ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmDistanceRight],
                                2, NULL, global_work, NULL, 0, NULL, NULL);
                            ret |= clSetKernelArg(kernel[nlmHorizontal], 2, sizeof(cl_int), &t_mq);
                            ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmHorizontal],
                                2, NULL, global_work, local_horiz, 0, NULL, NULL);
                            ret |= clSetKernelArg(kernel[nlmVertical], 2, sizeof(cl_int), &t_mq);
                            ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmVertical],
                                2, NULL, global_work, local_vert, 0, NULL, NULL);
                        }
                        ret |= clSetKernelArg(kernel[nlmAccumulation], 5, 4 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmAccumulation],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                    }
                }
            }
        }
        ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmFinish], 2, NULL, global_work, NULL, 0, NULL, NULL);
    } else {
        switch (clip_t) {
            case (EXTRA_NONE | CLIP_UNORM | COLOR_GRAY) :
                ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                break;
            case (EXTRA_CLIP | CLIP_UNORM | COLOR_GRAY) :
                ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_in[1], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                break;
            case (EXTRA_NONE | CLIP_STACKED | COLOR_GRAY) :
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 3, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (EXTRA_CLIP | CLIP_STACKED | COLOR_GRAY) :
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 3, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                    (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 3, sizeof(cl_mem), &mem_in[1]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (EXTRA_NONE | CLIP_UNORM | COLOR_YUV) :
            case (EXTRA_NONE | CLIP_STACKED | COLOR_YUV) :
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 3, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (EXTRA_CLIP | CLIP_UNORM | COLOR_YUV) :
            case (EXTRA_CLIP | CLIP_STACKED | COLOR_YUV) :
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_Y), 0, src->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_U), 0, src->GetReadPtr(PLANAR_U), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region_p,
                    (size_t) src->GetPitch(PLANAR_V), 0, src->GetReadPtr(PLANAR_V), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 3, sizeof(cl_mem), &mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                    (size_t) ref->GetPitch(PLANAR_Y), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                    (size_t) ref->GetPitch(PLANAR_U), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_P[2], CL_TRUE, origin, region_p,
                    (size_t) ref->GetPitch(PLANAR_V), 0, ref->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                ret |= clSetKernelArg(kernel[nlmSpatialPack], 3, sizeof(cl_mem), &mem_in[1]);
                ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialPack],
                    2, NULL, global_work, NULL, 0, NULL, NULL);
                break;
            case (EXTRA_NONE | CLIP_UNORM | COLOR_RGB) :
                ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(), 0, src->GetReadPtr(), 0, NULL, NULL);
                break;
            case (EXTRA_CLIP | CLIP_UNORM | COLOR_RGB) :
                ret |= clEnqueueWriteImage(command_queue, mem_in[0], CL_TRUE, origin, region,
                    (size_t) src->GetPitch(), 0, src->GetReadPtr(), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, mem_in[1], CL_TRUE, origin, region,
                    (size_t) ref->GetPitch(), 0, ref->GetReadPtr(), 0, NULL, NULL);
                break;
            default:
                env->ThrowError("KNLMeansCL: AviSynthGetFrame error!");
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
                        2, NULL, global_work, local_horiz, 0, NULL, NULL);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialVertical],
                        2, NULL, global_work, local_vert, 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[nlmSpatialAccumulation], 5, 2 * sizeof(cl_int), &q);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialAccumulation],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                }
            }
        }
        ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmSpatialFinish], 2, NULL, global_work, NULL, 0, NULL, NULL);
    }   
    switch (clip_t) {
        case (EXTRA_NONE | CLIP_UNORM | COLOR_GRAY) :
        case (EXTRA_CLIP | CLIP_UNORM | COLOR_GRAY) :
            ret |= clEnqueueReadImage(command_queue, mem_out, CL_TRUE, origin, region,
                (size_t) dst->GetPitch(PLANAR_Y), 0, dst->GetWritePtr(PLANAR_Y), 0, NULL, NULL);            
            break;
        case (EXTRA_NONE | CLIP_STACKED | COLOR_GRAY) :
        case (EXTRA_CLIP | CLIP_STACKED | COLOR_GRAY) :
            ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmUnpack],
                2, NULL, global_work, NULL, 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                (size_t) dst->GetPitch(PLANAR_Y), 0, dst->GetWritePtr(PLANAR_Y), 0, NULL, NULL); 
            break;
        case (EXTRA_NONE | CLIP_UNORM | COLOR_YUV) :
        case (EXTRA_CLIP | CLIP_UNORM | COLOR_YUV) :
        case (EXTRA_NONE | CLIP_STACKED | COLOR_YUV) :
        case (EXTRA_CLIP | CLIP_STACKED | COLOR_YUV) :
            ret |= clEnqueueNDRangeKernel(command_queue, kernel[nlmUnpack], 2,
                NULL, global_work, NULL, 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[0], CL_TRUE, origin, region_p,
                (size_t) dst->GetPitch(PLANAR_Y), 0, dst->GetWritePtr(PLANAR_Y), 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[1], CL_TRUE, origin, region_p,
                (size_t) dst->GetPitch(PLANAR_U), 0, dst->GetWritePtr(PLANAR_U), 0, NULL, NULL);
            ret |= clEnqueueReadImage(command_queue, mem_P[2], CL_TRUE, origin, region_p,
                (size_t) dst->GetPitch(PLANAR_V), 0, dst->GetWritePtr(PLANAR_V), 0, NULL, NULL);
            break;
        case (EXTRA_NONE | CLIP_UNORM | COLOR_RGB) :
        case (EXTRA_CLIP | CLIP_UNORM | COLOR_RGB) :
            ret |= clEnqueueReadImage(command_queue, mem_out, CL_TRUE, origin, region,
                (size_t) dst->GetPitch(), 0, dst->GetWritePtr(), 0, NULL, NULL);
            break;
        default:
            env->ThrowError("KNLMeansCL: AviSynthGetFrame error!");
            break;
    }
    ret |= clFinish(command_queue);
    ret |= clReleaseCommandQueue(command_queue);
    if (ret != CL_SUCCESS) env->ThrowError("KNLMeansCL: AviSynthGetFrame error!");

    // Info.
    if (info) {
        uint8_t y = 0, *frm = dst->GetWritePtr(PLANAR_Y);
        int pitch = dst->GetPitch(PLANAR_Y);
        char buffer[2048], str[2048], str1[2048];
        DrawString(frm, pitch, 0, y++, "KNLMeansCL");
        DrawString(frm, pitch, 0, y++, " Version " VERSION);
        DrawString(frm, pitch, 0, y++, " Copyright(C) Khanattila");
        snprintf(buffer, 2048, " D:%li  A:%lix%li  S:%lix%li", 2 * d + 1, 2 * a + 1, 2 * a + 1, 2 * s + 1, 2 * s + 1);
        DrawString(frm, pitch, 0, y++, buffer);
        snprintf(buffer, 2048, " Num of ref pixels: %li", (2 * d + 1)*(2 * a + 1)*(2 * a + 1) - 1);
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
        if (ret != CL_SUCCESS) env->ThrowError("KNLMeansCL: AviSynthInfo error!");
    }
    return dst;
}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthGetFrame
#ifdef VAPOURSYNTH_H
static const VSFrameRef *VS_CC VapourSynthPluginGetFrame(int n, int activationReason, void **instanceData,
    void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {

    KNLMeansData *d = (KNLMeansData*) *instanceData;
    const int maxframe = d->vi->numFrames - 1;
    if (activationReason == arInitial) {
        for (int k = int64ToIntS(-d->d); k <= d->d; k++) {
            vsapi->requestFrameFilter(clamp(n + k, 0, maxframe), d->node, frameCtx);
            if (d->knot) vsapi->requestFrameFilter(clamp(n + k, 0, maxframe), d->knot, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        // Variables.
        const VSFrameRef *src, *ref;
        const VSFormat *fi = d->vi->format;
        VSFrameRef *dst = vsapi->newVideoFrame(fi, d->idmn[0], d->idmn[1], NULL, core);
        const cl_int t = int64ToIntS(d->d);
        const cl_float pattern_u0 = 0.0f;
        const cl_float pattern_u3 = CL_FLT_EPSILON;
        const size_t size_u0 = sizeof(cl_float) * d->idmn[0] * d->idmn[1] * ((d->clip_t & COLOR_GRAY) ? 2 : 4);
        const size_t size_u3 = sizeof(cl_float) * d->idmn[0] * d->idmn[1];
        const size_t origin[3] = { 0, 0, 0 };
        const size_t region[3] = { (size_t) d->idmn[0], (size_t) d->idmn[1], 1 };
        const size_t global_work[2] = {
            mrounds((size_t) d->idmn[0], fastmax(H_BLOCK_X, V_BLOCK_X)),
            mrounds((size_t) d->idmn[1], fastmax(H_BLOCK_Y, V_BLOCK_Y))
        };
        const size_t local_horiz[2] = { H_BLOCK_X, H_BLOCK_Y };
        const size_t local_vert[2] = { V_BLOCK_X, V_BLOCK_Y };

        //Copy chroma.  
        if (fi->colorFamily == cmYUV && (d->clip_t & COLOR_GRAY)) {
            src = vsapi->getFrameFilter(n, d->node, frameCtx);
            vs_bitblt(vsapi->getWritePtr(dst, 1), vsapi->getStride(dst, 1), vsapi->getReadPtr(src, 1), vsapi->getStride(src, 1),
                vsapi->getFrameWidth(src, 1) * fi->bytesPerSample, vsapi->getFrameHeight(src, 1));
            vs_bitblt(vsapi->getWritePtr(dst, 2), vsapi->getStride(dst, 2), vsapi->getReadPtr(src, 2), vsapi->getStride(src, 2),
                vsapi->getFrameWidth(src, 2) * fi->bytesPerSample, vsapi->getFrameHeight(src, 2));
            vsapi->freeFrame(src);
        }  

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
                    case (EXTRA_NONE | CLIP_UNORM | COLOR_GRAY) :
                        ret |= clEnqueueWriteImage(command_queue, d->mem_in[0], CL_TRUE, origin_in, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        break;
                    case (EXTRA_CLIP | CLIP_UNORM | COLOR_GRAY) :
                        ret |= clEnqueueWriteImage(command_queue, d->mem_in[0], CL_TRUE, origin_in, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_in[1], CL_TRUE, origin_in, region,
                            (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                        break;
                    case (EXTRA_NONE | CLIP_UNSIGNED | COLOR_GRAY) :
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 3, sizeof(cl_mem), &d->mem_in[0]);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 4, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmPack],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        break;
                    case (EXTRA_CLIP | CLIP_UNSIGNED | COLOR_GRAY) :
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 3, sizeof(cl_mem), &d->mem_in[0]);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 4, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmPack],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 3, sizeof(cl_mem), &d->mem_in[1]);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 4, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmPack],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        break;
                    case (EXTRA_NONE | CLIP_UNORM | COLOR_YUV) :
                    case (EXTRA_NONE | CLIP_UNSIGNED | COLOR_YUV) :
                    case (EXTRA_NONE | CLIP_UNORM | COLOR_RGB) :
                    case (EXTRA_NONE | CLIP_UNSIGNED | COLOR_RGB) :
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 3, sizeof(cl_mem), &d->mem_in[0]);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 4, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmPack],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        break;
                    case (EXTRA_CLIP | CLIP_UNORM | COLOR_YUV) :
                    case (EXTRA_CLIP | CLIP_UNSIGNED | COLOR_YUV) :
                    case (EXTRA_CLIP | CLIP_UNORM | COLOR_RGB) :
                    case (EXTRA_CLIP | CLIP_UNSIGNED | COLOR_RGB) :
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 3, sizeof(cl_mem), &d->mem_in[0]);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 4, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmPack],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 1), 0, vsapi->getReadPtr(ref, 1), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 2), 0, vsapi->getReadPtr(ref, 2), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 3, sizeof(cl_mem), &d->mem_in[1]);
                        ret |= clSetKernelArg(d->kernel[nlmPack], 4, sizeof(cl_int), &t_pk);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmPack],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        break;
                    default:
                        vsapi->setFilterError("knlm.KNLMeansCL: knlmeansGetFrame error!", frameCtx);
                        vsapi->freeNode(d->node);
                        vsapi->freeNode(d->knot);
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
                            ret |= clSetKernelArg(d->kernel[nlmDistanceLeft], 3, 4 * sizeof(cl_int), &q);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmDistanceLeft],
                                2, NULL, global_work, NULL, 0, NULL, NULL);
                            ret |= clSetKernelArg(d->kernel[nlmHorizontal], 2, sizeof(cl_int), &t);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmHorizontal],
                                2, NULL, global_work, local_horiz, 0, NULL, NULL);
                            ret |= clSetKernelArg(d->kernel[nlmVertical], 2, sizeof(cl_int), &t);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmVertical],
                                2, NULL, global_work, local_vert, 0, NULL, NULL);
                            if (k) {
                                const cl_int t_mq = t - k;
                                ret |= clSetKernelArg(d->kernel[nlmDistanceRight], 3, 4 * sizeof(cl_int), &q);
                                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmDistanceRight],
                                    2, NULL, global_work, NULL, 0, NULL, NULL);
                                ret |= clSetKernelArg(d->kernel[nlmHorizontal], 2, sizeof(cl_int), &t_mq);
                                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmHorizontal],
                                    2, NULL, global_work, local_horiz, 0, NULL, NULL);
                                ret |= clSetKernelArg(d->kernel[nlmVertical], 2, sizeof(cl_int), &t_mq);
                                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmVertical],
                                    2, NULL, global_work, local_vert, 0, NULL, NULL);
                            }
                            ret |= clSetKernelArg(d->kernel[nlmAccumulation], 5, 4 * sizeof(cl_int), &q);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmAccumulation],
                                2, NULL, global_work, NULL, 0, NULL, NULL);
                        }
                    }
                }
            }
            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmFinish], 2, NULL, global_work, NULL, 0, NULL, NULL);
        } else {
            src = vsapi->getFrameFilter(n, d->node, frameCtx);
            ref = (d->knot) ? vsapi->getFrameFilter(n, d->knot, frameCtx) : nullptr;
            switch (d->clip_t) {
                case (EXTRA_NONE | CLIP_UNORM | COLOR_GRAY) :
                    ret |= clEnqueueWriteImage(command_queue, d->mem_in[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    break;
                case (EXTRA_CLIP | CLIP_UNORM | COLOR_GRAY) :
                    ret |= clEnqueueWriteImage(command_queue, d->mem_in[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_in[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                    break;
                case (EXTRA_NONE | CLIP_UNSIGNED | COLOR_GRAY) :
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmSpatialPack], 3, sizeof(cl_mem), &d->mem_in[0]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (EXTRA_CLIP | CLIP_UNSIGNED | COLOR_GRAY) :
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmSpatialPack], 3, sizeof(cl_mem), &d->mem_in[0]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmSpatialPack], 3, sizeof(cl_mem), &d->mem_in[1]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (EXTRA_NONE | CLIP_UNORM | COLOR_YUV) :
                case (EXTRA_NONE | CLIP_UNSIGNED | COLOR_YUV) :
                case (EXTRA_NONE | CLIP_UNORM | COLOR_RGB) :
                case (EXTRA_NONE | CLIP_UNSIGNED | COLOR_RGB) :
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmSpatialPack], 3, sizeof(cl_mem), &d->mem_in[0]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                case (EXTRA_CLIP | CLIP_UNORM | COLOR_YUV) :
                case (EXTRA_CLIP | CLIP_UNSIGNED | COLOR_YUV) :
                case (EXTRA_CLIP | CLIP_UNORM | COLOR_RGB) :
                case (EXTRA_CLIP | CLIP_UNSIGNED | COLOR_RGB) :
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmSpatialPack], 3, sizeof(cl_mem), &d->mem_in[0]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(ref, 1), 0, vsapi->getReadPtr(ref, 1), 0, NULL, NULL);
                    ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                        (size_t) vsapi->getStride(ref, 2), 0, vsapi->getReadPtr(ref, 2), 0, NULL, NULL);
                    ret |= clSetKernelArg(d->kernel[nlmSpatialPack], 3, sizeof(cl_mem), &d->mem_in[1]);
                    ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialPack],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    break;
                default:
                    vsapi->setFilterError("knlm.KNLMeansCL: knlmeansGetFrame error!", frameCtx);
                    vsapi->freeNode(d->node);
                    vsapi->freeNode(d->knot);
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
                            2, NULL, global_work, local_horiz, 0, NULL, NULL);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialVertical],
                            2, NULL, global_work, local_vert, 0, NULL, NULL);                      
                        ret |= clSetKernelArg(d->kernel[nlmSpatialAccumulation], 5, 2 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialAccumulation],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                    }
                }
            }
            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmSpatialFinish], 2, NULL, global_work, NULL, 0, NULL, NULL);
        }
        switch (d->clip_t) {
            case (EXTRA_NONE | CLIP_UNORM | COLOR_GRAY) :
            case (EXTRA_CLIP | CLIP_UNORM | COLOR_GRAY) :
                ret |= clEnqueueReadImage(command_queue, d->mem_out, CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(dst, 0), 0, vsapi->getWritePtr(dst, 0), 0, NULL, NULL);
                break;
            case (EXTRA_NONE | CLIP_UNSIGNED | COLOR_GRAY) :
            case (EXTRA_CLIP | CLIP_UNSIGNED | COLOR_GRAY) :
                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmUnpack], 2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(dst, 0), 0, vsapi->getWritePtr(dst, 0), 0, NULL, NULL);
                break;  
            case (EXTRA_NONE | CLIP_UNORM | COLOR_YUV) :
            case (EXTRA_CLIP | CLIP_UNORM | COLOR_YUV) :
            case (EXTRA_NONE | CLIP_UNSIGNED | COLOR_YUV) :
            case (EXTRA_CLIP | CLIP_UNSIGNED | COLOR_YUV) :
            case (EXTRA_NONE | CLIP_UNORM | COLOR_RGB) :
            case (EXTRA_CLIP | CLIP_UNORM | COLOR_RGB) :
            case (EXTRA_NONE | CLIP_UNSIGNED | COLOR_RGB) :
            case (EXTRA_CLIP | CLIP_UNSIGNED | COLOR_RGB) :
                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[nlmUnpack], 2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(dst, 0), 0, vsapi->getWritePtr(dst, 0), 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(dst, 1), 0, vsapi->getWritePtr(dst, 1), 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(dst, 2), 0, vsapi->getWritePtr(dst, 2), 0, NULL, NULL);                             
                break;
            default:
                vsapi->setFilterError("knlm.KNLMeansCL: knlmeansGetFrame error!", frameCtx);
                vsapi->freeNode(d->node);
                vsapi->freeNode(d->knot);
                return 0;
        }     
        ret |= clFinish(command_queue);
        ret |= clReleaseCommandQueue(command_queue);
        if (ret != CL_SUCCESS) {
            vsapi->setFilterError("knlm.KNLMeansCL: knlmeansGetFrame error!", frameCtx);
            vsapi->freeNode(d->node);
            vsapi->freeNode(d->knot);
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
                vsapi->setFilterError("knlm.KNLMeansCL: VapourSynthInfo error!", frameCtx);
                vsapi->freeNode(d->node);
                vsapi->freeNode(d->knot);
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
KNLMeansClass::~KNLMeansClass() {
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
    clReleaseKernel(kernel[nlmUnpack]);
    clReleaseKernel(kernel[nlmPack]);
    clReleaseKernel(kernel[nlmSpatialPack]);
    clReleaseKernel(kernel[nlmFinish]);
    clReleaseKernel(kernel[nlmAccumulation]);
    clReleaseKernel(kernel[nlmVertical]);
    clReleaseKernel(kernel[nlmHorizontal]);
    clReleaseKernel(kernel[nlmDistanceRight]);
    clReleaseKernel(kernel[nlmDistanceLeft]);
    clReleaseKernel(kernel[nlmSpatialFinish]);
    clReleaseKernel(kernel[nlmSpatialAccumulation]);
    clReleaseKernel(kernel[nlmSpatialVertical]);
    clReleaseKernel(kernel[nlmSpatialHorizontal]);
    clReleaseKernel(kernel[nlmSpatialDistance]);
    clReleaseProgram(program);
    clReleaseContext(context);
}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthFree
#ifdef VAPOURSYNTH_H
static void VS_CC VapourSynthPluginFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    KNLMeansData *d = (KNLMeansData*) instanceData;
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
    clReleaseKernel(d->kernel[nlmUnpack]);
    clReleaseKernel(d->kernel[nlmPack]);
    clReleaseKernel(d->kernel[nlmSpatialPack]);
    clReleaseKernel(d->kernel[nlmFinish]);
    clReleaseKernel(d->kernel[nlmAccumulation]);
    clReleaseKernel(d->kernel[nlmVertical]);
    clReleaseKernel(d->kernel[nlmHorizontal]);
    clReleaseKernel(d->kernel[nlmDistanceRight]);
    clReleaseKernel(d->kernel[nlmDistanceLeft]);
    clReleaseKernel(d->kernel[nlmSpatialFinish]);
    clReleaseKernel(d->kernel[nlmSpatialAccumulation]);
    clReleaseKernel(d->kernel[nlmSpatialVertical]);
    clReleaseKernel(d->kernel[nlmSpatialHorizontal]);
    clReleaseKernel(d->kernel[nlmSpatialDistance]);
    clReleaseProgram(d->program);
    clReleaseContext(d->context);
    free(d);
}
#endif //__VAPOURSYNTH_H__

//////////////////////////////////////////
// AviSynthCreate
#ifdef __AVISYNTH_6_H__
AVSValue __cdecl AviSynthPluginCreate(AVSValue args, void* user_data, IScriptEnvironment* env) {
    return new KNLMeansClass(args[0].AsClip(), args[1].AsInt(DFT_D), args[2].AsInt(DFT_A), args[3].AsInt(DFT_S), args[4].AsBool(DFT_cmode),
        args[5].AsInt(DFT_wmode), args[6].AsFloat(DFT_h), args[7].Defined() ? args[7].AsClip() : nullptr, args[8].AsString(DFT_ocl_device),
        args[9].AsInt(DFT_ocl_id), args[10].AsBool(DFT_lsb), args[11].AsBool(DFT_info), env);
}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthCreate
#ifdef VAPOURSYNTH_H
static void VS_CC VapourSynthPluginCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {

    // Checks source clip and rclip.
    KNLMeansData d;
    cl_channel_order channel_order = 0;
    cl_channel_type channel_type = 0;
    int err;
    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.knot = vsapi->propGetNode(in, "rclip", 0, &err);
    if (err) {
        d.knot = nullptr;
        d.clip_t = EXTRA_NONE;
    } else d.clip_t = EXTRA_CLIP;
    d.vi = vsapi->getVideoInfo(d.node);
    d.bit_shift = d.vi->format->bytesPerSample * 8u - d.vi->format->bitsPerSample;
    if (isConstantFormat(d.vi)) {
        d.cmode = vsapi->propGetInt(in, "cmode", 0, &err);
        if (err) d.cmode = DFT_cmode;
        if (d.cmode && (d.vi->format->subSamplingW != 0) && (d.vi->format->subSamplingH != 0)) {
            vsapi->setError(out, "knlm.KNLMeansCL: cmode requires 4:4:4 subsampling!");
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
        vsapi->setError(out, "knlm.KNLMeansCL: rclip do not math source clip!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }

    // Sets default value.
    d.d = vsapi->propGetInt(in, "d", 0, &err);
    if (err) d.d = DFT_D;
    d.a = vsapi->propGetInt(in, "a", 0, &err);
    if (err) d.a = DFT_A;
    d.s = vsapi->propGetInt(in, "s", 0, &err);
    if (err) d.s = DFT_S;
    d.wmode = vsapi->propGetInt(in, "wmode", 0, &err);
    if (err) d.wmode = DFT_wmode;
    d.h = vsapi->propGetFloat(in, "h", 0, &err);
    if (err) d.h = DFT_h;
    d.ocl_device = vsapi->propGetData(in, "device_type", 0, &err);
    if (err) d.ocl_device = DFT_ocl_device;
    d.ocl_id = vsapi->propGetInt(in, "device_id", 0, &err);
    if (err) d.ocl_id = DFT_ocl_id;
    d.info = vsapi->propGetInt(in, "info", 0, &err);
    if (err) d.info = DFT_info;

    // Checks user value.
    if (d.info && d.vi->format->bitsPerSample != 8) {
        vsapi->setError(out, "knlm.KNLMeansCL: info requires Gray8 or YUVP8 color space!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (d.d < 0) {
        vsapi->setError(out, "knlm.KNLMeansCL: d must be greater than or equal to 0!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (d.a < 1) {
        vsapi->setError(out, "knlm.KNLMeansCL: a must be greater than or equal to 1!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (d.s < 0) {
        vsapi->setError(out, "knlm.KNLMeansCL: s must be greater than or equal to 0!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (d.wmode < 0 || d.wmode > 2) {
        vsapi->setError(out, "knlm.KNLMeansCL: wmode must be in range [0, 2]!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (d.h <= 0.0) {
        vsapi->setError(out, "knlm.KNLMeansCL: h must be greater than 0!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    cl_device_type device_type = 0;
    bool device_auto = false;
    if (!strcasecmp(d.ocl_device, "CPU")) {
        device_type = CL_DEVICE_TYPE_CPU;
    } else if (!strcasecmp(d.ocl_device, "GPU")) {
        device_type = CL_DEVICE_TYPE_GPU;
    } else if (!strcasecmp(d.ocl_device, "ACCELERATOR")) {
        device_type = CL_DEVICE_TYPE_ACCELERATOR;
    } else if (!strcasecmp(d.ocl_device, "ALL")) {
        device_type = CL_DEVICE_TYPE_ALL;
    } else if (!strcasecmp(d.ocl_device, "DEFAULT")) {
        device_type = CL_DEVICE_TYPE_DEFAULT;
    } else if (!strcasecmp(d.ocl_device, "AUTO")) {
        device_auto = true;
    } else {
        vsapi->setError(out, "knlm.KNLMeansCL: device_type must be cpu, gpu, accelerator, all, default or auto!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (d.ocl_id < 0) {
        vsapi->setError(out, "knlm.KNLMeansCL: device_id must be greater than or equal to 0!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }

    // Gets PlatformID and DeviceID.
    cl_int ret = CL_SUCCESS;
    if (device_auto) {
        device_type = CL_DEVICE_TYPE_GPU;
        ret |= oclGetDevicesList(device_type, NULL, &d.num_devices);
        if (d.num_devices == 0) {
            device_type = CL_DEVICE_TYPE_ACCELERATOR;
            ret |= oclGetDevicesList(device_type, NULL, &d.num_devices);
            if (d.num_devices == 0) {
                device_type = CL_DEVICE_TYPE_CPU;
                ret |= oclGetDevicesList(device_type, NULL, &d.num_devices);
                if (d.num_devices == 0) {
                    device_type = CL_DEVICE_TYPE_DEFAULT;
                    ret |= oclGetDevicesList(device_type, NULL, &d.num_devices);
                    if (d.num_devices == 0) {
                        device_type = CL_DEVICE_TYPE_ALL;
                        ret |= oclGetDevicesList(device_type, NULL, &d.num_devices);
                        if (d.num_devices == 0) {
                            vsapi->setError(out, "knlm.KNLMeansCL: no opencl platforms available!");
                            vsapi->freeNode(d.node);
                            vsapi->freeNode(d.knot);
                            return;
                        } else if (d.ocl_id >= (int) d.num_devices) {
                            vsapi->setError(out, "knlm.KNLMeansCL: selected device is not available!");
                            vsapi->freeNode(d.node);
                            vsapi->freeNode(d.knot);
                            return;
                        }
                    } else if (d.ocl_id >= (int) d.num_devices) {
                        vsapi->setError(out, "knlm.KNLMeansCL: selected device is not available!");
                        vsapi->freeNode(d.node);
                        vsapi->freeNode(d.knot);
                        return;
                    }
                } else if (d.ocl_id >= (int) d.num_devices) {
                    vsapi->setError(out, "knlm.KNLMeansCL: selected device is not available!");
                    vsapi->freeNode(d.node);
                    vsapi->freeNode(d.knot);
                    return;
                }
            } else if (d.ocl_id >= (int) d.num_devices) {
                vsapi->setError(out, "knlm.KNLMeansCL: selected device is not available!");
                vsapi->freeNode(d.node);
                vsapi->freeNode(d.knot);
                return;
            }
        } else if (d.ocl_id >= (int) d.num_devices) {
            vsapi->setError(out, "knlm.KNLMeansCL: selected device is not available!");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.knot);
            return;
        }
        ocl_device_packed *devices = (ocl_device_packed*) malloc(sizeof(ocl_device_packed) * d.num_devices);
        ret |= oclGetDevicesList(device_type, devices, NULL);
        if (ret != CL_SUCCESS) {
            vsapi->setError(out, "knlm.KNLMeansCL: VapourSynthCreate error (clGetPlatformID)!");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.knot);
            return;
        }
        d.platformID = devices[d.ocl_id].platform;
        d.deviceID = devices[d.ocl_id].device;
        free(devices);
    } else {
        cl_int ret = oclGetDevicesList(device_type, NULL, &d.num_devices);
        if (d.num_devices == 0) {
            vsapi->setError(out, "knlm.KNLMeansCL: no opencl platforms available!");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.knot);
            return;
        } else if (d.ocl_id >= (int) d.num_devices) {
            vsapi->setError(out, "knlm.KNLMeansCL: selected device is not available!");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.knot);
            return;
        }
        ocl_device_packed *devices = (ocl_device_packed*) malloc(sizeof(ocl_device_packed) * d.num_devices);
        ret |= oclGetDevicesList(device_type, devices, NULL);
        if (ret != CL_SUCCESS) {
            vsapi->setError(out, "knlm.KNLMeansCL: VapourSynthCreate error (oclGetDevicesList)!");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.knot);
            return;
        }
        d.platformID = devices[d.ocl_id].platform;
        d.deviceID = devices[d.ocl_id].device;
        free(devices);
    }

    // Creates an OpenCL context, 2D images and buffers object.
    d.idmn[0] = d.vi->width;
    d.idmn[1] = d.vi->height;
    d.context = clCreateContext(NULL, 1, &d.deviceID, NULL, NULL, NULL);
    const cl_image_format image_format = { channel_order, channel_type };
    const cl_image_desc image_desc = { (cl_mem_object_type) (d.d ? CL_MEM_OBJECT_IMAGE2D_ARRAY : CL_MEM_OBJECT_IMAGE2D),
        (size_t) d.idmn[0], (size_t) d.idmn[1], 1, 2 * (size_t) d.d + 1, 0, 0, 0, 0, NULL };
    const cl_image_desc image_desc_out = { CL_MEM_OBJECT_IMAGE2D, (size_t) d.idmn[0], (size_t) d.idmn[1], 1, 1, 0, 0, 0, 0, NULL };
    if ((d.clip_t & COLOR_GRAY) && !d.bit_shift) {
        d.mem_in[0] = clCreateImage(d.context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, &image_format, &image_desc, NULL, NULL);
        d.mem_in[1] = clCreateImage(d.context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, &image_format, &image_desc, NULL, NULL);       
        d.mem_out = clCreateImage(d.context, CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY, &image_format, &image_desc_out, NULL, NULL);
    } else {
        d.mem_in[0] = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format, &image_desc, NULL, NULL);
        d.mem_in[1] = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format, &image_desc, NULL, NULL);       
        d.mem_out = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format, &image_desc_out, NULL, NULL);
    }
    const cl_image_format image_format_u = { CL_LUMINANCE, CL_HALF_FLOAT };
    const size_t size_u0 = sizeof(cl_float) * d.idmn[0] * d.idmn[1] * ((d.clip_t & COLOR_GRAY) ? 2 : 4);
    const size_t size_u3 = sizeof(cl_float) * d.idmn[0] * d.idmn[1];
    d.mem_U[0] = clCreateBuffer(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u0, NULL, NULL);
    d.mem_U[1] = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format_u, &image_desc, NULL, NULL);
    d.mem_U[2] = clCreateImage(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, &image_format_u, &image_desc, NULL, NULL);
    d.mem_U[3] = clCreateBuffer(d.context, CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY, size_u3, NULL, NULL);
    if (d.bit_shift) {
        const cl_image_format image_format_p = { CL_R, CL_UNSIGNED_INT16 };
        d.mem_P[0] = clCreateImage(d.context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_out, NULL, NULL);
        d.mem_P[1] = clCreateImage(d.context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_out, NULL, NULL);
        d.mem_P[2] = clCreateImage(d.context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_out, NULL, NULL);
    } else {
        const cl_image_format image_format_p = { CL_LUMINANCE, channel_type };
        d.mem_P[0] = clCreateImage(d.context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_out, NULL, NULL);
        d.mem_P[1] = clCreateImage(d.context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_out, NULL, NULL);
        d.mem_P[2] = clCreateImage(d.context, CL_MEM_READ_WRITE, &image_format_p, &image_desc_out, NULL, NULL);
    }

    // Creates and Build a program executable from the program source.
    d.program = clCreateProgramWithSource(d.context, 1, &kernel_source_code, NULL, NULL);
    char options[2048];
    setlocale(LC_ALL, "C");  
    snprintf(options, 2048, "-cl-single-precision-constant -cl-denorms-are-zero -cl-fast-relaxed-math -Werror "
        "-D H_BLOCK_X=%i -D H_BLOCK_Y=%i -D V_BLOCK_X=%i -D V_BLOCK_Y=%i -D NLMK_TCLIP=%u -D NLMK_S=%i " 
        "-D NLMK_WMODE=%i -D NLMK_D=%i -D NLMK_H2_INV_NORM=%f -D NLMK_BIT_SHIFT=%u",
        H_BLOCK_X, H_BLOCK_Y, V_BLOCK_X, V_BLOCK_Y, d.clip_t, int64ToIntS(d.s),
        int64ToIntS(d.wmode), int64ToIntS(d.d), 65025.0 / (3*d.h*d.h*(2*d.s+1)*(2*d.s+1)), d.bit_shift); 
    ret = clBuildProgram(d.program, 1, &d.deviceID, options, NULL, NULL);
    if (ret != CL_SUCCESS) {
        size_t options_size, log_size;
        clGetProgramBuildInfo(d.program, d.deviceID, CL_PROGRAM_BUILD_OPTIONS, 0, NULL, &options_size);
        clGetProgramBuildInfo(d.program, d.deviceID, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char *options = (char*) malloc(options_size);
        char *log = (char*) malloc(log_size);
        clGetProgramBuildInfo(d.program, d.deviceID, CL_PROGRAM_BUILD_OPTIONS, options_size, options, NULL);
        clGetProgramBuildInfo(d.program, d.deviceID, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        std::ofstream outfile("Log-KNLMeansCL.txt", std::ofstream::out);
        outfile << "---------------------------------" << std::endl;
        outfile << "*** Error in OpenCL compiler ***" << std::endl;
        outfile << "---------------------------------" << std::endl;
        outfile << std::endl << "# Build Options" << std::endl;
        outfile << options << std::endl;
        outfile << std::endl << "# Build Log" << std::endl;
        outfile << log << std::endl;
        outfile.close();
        free(log);
        vsapi->setError(out, "knlm.KNLMeansCL: VapourSynthCreate error (clBuildProgram)!\n Please report Log-KNLMeansCL.txt.");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    setlocale(LC_ALL, "");

    // Creates kernel objects.
    d.kernel[nlmSpatialDistance] = clCreateKernel(d.program, "nlmSpatialDistance", NULL);
    d.kernel[nlmSpatialHorizontal] = clCreateKernel(d.program, "nlmSpatialHorizontal", NULL);
    d.kernel[nlmSpatialVertical] = clCreateKernel(d.program, "nlmSpatialVertical", NULL);
    d.kernel[nlmSpatialAccumulation] = clCreateKernel(d.program, "nlmSpatialAccumulation", NULL);
    d.kernel[nlmSpatialFinish] = clCreateKernel(d.program, "nlmSpatialFinish", NULL);
    d.kernel[nlmDistanceLeft] = clCreateKernel(d.program, "nlmDistanceLeft", NULL);
    d.kernel[nlmDistanceRight] = clCreateKernel(d.program, "nlmDistanceRight", NULL);
    d.kernel[nlmHorizontal] = clCreateKernel(d.program, "nlmHorizontal", NULL);
    d.kernel[nlmVertical] = clCreateKernel(d.program, "nlmVertical", NULL);
    d.kernel[nlmAccumulation] = clCreateKernel(d.program, "nlmAccumulation", NULL);
    d.kernel[nlmFinish] = clCreateKernel(d.program, "nlmFinish", NULL);
    d.kernel[nlmSpatialPack] = clCreateKernel(d.program, "nlmSpatialPack", NULL);
    d.kernel[nlmPack] = clCreateKernel(d.program, "nlmPack", NULL);
    d.kernel[nlmUnpack] = clCreateKernel(d.program, "nlmUnpack", NULL);

    // Sets kernel arguments.
    if (d.d) {
        ret = clSetKernelArg(d.kernel[nlmDistanceLeft], 0, sizeof(cl_mem), &d.mem_in[(d.clip_t & EXTRA_NONE) ? 0 : 1]);
        ret |= clSetKernelArg(d.kernel[nlmDistanceLeft], 1, sizeof(cl_mem), &d.mem_U[1]);
        ret |= clSetKernelArg(d.kernel[nlmDistanceLeft], 2, 2 * sizeof(cl_int), &d.idmn);
        ret |= clSetKernelArg(d.kernel[nlmDistanceRight], 0, sizeof(cl_mem), &d.mem_in[(d.clip_t & EXTRA_NONE) ? 0 : 1]);
        ret |= clSetKernelArg(d.kernel[nlmDistanceRight], 1, sizeof(cl_mem), &d.mem_U[1]);
        ret |= clSetKernelArg(d.kernel[nlmDistanceRight], 2, 2 * sizeof(cl_int), &d.idmn);
        ret |= clSetKernelArg(d.kernel[nlmHorizontal], 0, sizeof(cl_mem), &d.mem_U[1]);
        ret |= clSetKernelArg(d.kernel[nlmHorizontal], 1, sizeof(cl_mem), &d.mem_U[2]);
        ret |= clSetKernelArg(d.kernel[nlmHorizontal], 3, 2 * sizeof(cl_int), &d.idmn);
        ret |= clSetKernelArg(d.kernel[nlmVertical], 0, sizeof(cl_mem), &d.mem_U[2]);
        ret |= clSetKernelArg(d.kernel[nlmVertical], 1, sizeof(cl_mem), &d.mem_U[1]);
        ret |= clSetKernelArg(d.kernel[nlmVertical], 3, 2 * sizeof(cl_int), &d.idmn);
        ret |= clSetKernelArg(d.kernel[nlmAccumulation], 0, sizeof(cl_mem), &d.mem_in[0]);
        ret |= clSetKernelArg(d.kernel[nlmAccumulation], 1, sizeof(cl_mem), &d.mem_U[0]);
        ret |= clSetKernelArg(d.kernel[nlmAccumulation], 2, sizeof(cl_mem), &d.mem_U[1]);
        ret |= clSetKernelArg(d.kernel[nlmAccumulation], 3, sizeof(cl_mem), &d.mem_U[3]);
        ret |= clSetKernelArg(d.kernel[nlmAccumulation], 4, 2 * sizeof(cl_int), &d.idmn);
        ret |= clSetKernelArg(d.kernel[nlmFinish], 0, sizeof(cl_mem), &d.mem_in[0]);
        ret |= clSetKernelArg(d.kernel[nlmFinish], 1, sizeof(cl_mem), &d.mem_out);
        ret |= clSetKernelArg(d.kernel[nlmFinish], 2, sizeof(cl_mem), &d.mem_U[0]);
        ret |= clSetKernelArg(d.kernel[nlmFinish], 3, sizeof(cl_mem), &d.mem_U[3]);
        ret |= clSetKernelArg(d.kernel[nlmFinish], 4, 2 * sizeof(cl_int), &d.idmn);
        ret |= clSetKernelArg(d.kernel[nlmPack], 0, sizeof(cl_mem), &d.mem_P[0]);
        ret |= clSetKernelArg(d.kernel[nlmPack], 1, sizeof(cl_mem), &d.mem_P[1]);
        ret |= clSetKernelArg(d.kernel[nlmPack], 2, sizeof(cl_mem), &d.mem_P[2]);
        ret |= clSetKernelArg(d.kernel[nlmPack], 5, 2 * sizeof(cl_int), &d.idmn);    
    } else {
        ret = clSetKernelArg(d.kernel[nlmSpatialDistance], 0, sizeof(cl_mem), &d.mem_in[(d.clip_t & EXTRA_NONE) ? 0 : 1]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialDistance], 1, sizeof(cl_mem), &d.mem_U[1]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialDistance], 2, 2 * sizeof(cl_int), &d.idmn);
        ret |= clSetKernelArg(d.kernel[nlmSpatialHorizontal], 0, sizeof(cl_mem), &d.mem_U[1]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialHorizontal], 1, sizeof(cl_mem), &d.mem_U[2]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialHorizontal], 2, 2 * sizeof(cl_int), &d.idmn);
        ret |= clSetKernelArg(d.kernel[nlmSpatialVertical], 0, sizeof(cl_mem), &d.mem_U[2]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialVertical], 1, sizeof(cl_mem), &d.mem_U[1]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialVertical], 2, 2 * sizeof(cl_int), &d.idmn);
        ret |= clSetKernelArg(d.kernel[nlmSpatialAccumulation], 0, sizeof(cl_mem), &d.mem_in[0]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialAccumulation], 1, sizeof(cl_mem), &d.mem_U[0]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialAccumulation], 2, sizeof(cl_mem), &d.mem_U[1]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialAccumulation], 3, sizeof(cl_mem), &d.mem_U[3]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialAccumulation], 4, 2 * sizeof(cl_int), &d.idmn);
        ret |= clSetKernelArg(d.kernel[nlmSpatialFinish], 0, sizeof(cl_mem), &d.mem_in[0]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialFinish], 1, sizeof(cl_mem), &d.mem_out);
        ret |= clSetKernelArg(d.kernel[nlmSpatialFinish], 2, sizeof(cl_mem), &d.mem_U[0]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialFinish], 3, sizeof(cl_mem), &d.mem_U[3]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialFinish], 4, 2 * sizeof(cl_int), &d.idmn);
        ret |= clSetKernelArg(d.kernel[nlmSpatialPack], 0, sizeof(cl_mem), &d.mem_P[0]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialPack], 1, sizeof(cl_mem), &d.mem_P[1]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialPack], 2, sizeof(cl_mem), &d.mem_P[2]);
        ret |= clSetKernelArg(d.kernel[nlmSpatialPack], 4, 2 * sizeof(cl_int), &d.idmn);
    }
    ret |= clSetKernelArg(d.kernel[nlmUnpack], 0, sizeof(cl_mem), &d.mem_P[0]);
    ret |= clSetKernelArg(d.kernel[nlmUnpack], 1, sizeof(cl_mem), &d.mem_P[1]);
    ret |= clSetKernelArg(d.kernel[nlmUnpack], 2, sizeof(cl_mem), &d.mem_P[2]);
    ret |= clSetKernelArg(d.kernel[nlmUnpack], 3, sizeof(cl_mem), &d.mem_out);
    ret |= clSetKernelArg(d.kernel[nlmUnpack], 4, 2 * sizeof(cl_int), &d.idmn);
    if (ret != CL_SUCCESS) {
        vsapi->setError(out, "knlm.KNLMeansCL: VapourSynthCreate error (clSetKernelArg)!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }

    // Creates a new filter and returns a reference to it.
    KNLMeansData *data = (KNLMeansData*) malloc(sizeof(d));
    if(data) *data = d;
    else {
        vsapi->setError(out, "knlm.KNLMeansCL: VapourSynthCreate error (malloc fail)!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    vsapi->createFilter(in, out, "KNLMeansCL", knlmeansInit, VapourSynthPluginGetFrame,
        VapourSynthPluginFree, fmParallelRequests, 0, data, core);
}
#endif //__VAPOURSYNTH_H__

//////////////////////////////////////////
// AviSynthPluginInit
#ifdef __AVISYNTH_6_H__
const AVS_Linkage *AVS_linkage = 0;
extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors) {

    AVS_linkage = vectors;
    env->AddFunction("KNLMeansCL", "c[d]i[a]i[s]i[cmode]b[wmode]i[h]f[rclip]c[device_type]s[device_id]i[lsb_inout]b[info]b",
        AviSynthPluginCreate, 0);
    return "KNLMeansCL for AviSynth";
}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthPluginInit
#ifdef VAPOURSYNTH_H
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {

    configFunc("com.Khanattila.KNLMeansCL", "knlm", "KNLMeansCL for VapourSynth", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("KNLMeansCL", "clip:clip;d:int:opt;a:int:opt;s:int:opt;cmode:int:opt;wmode:int:opt;h:float:opt;rclip:clip:opt;\
device_type:data:opt;device_id:int:opt;info:int:opt", VapourSynthPluginCreate, nullptr, plugin);
}
#endif //__VAPOURSYNTH_H__