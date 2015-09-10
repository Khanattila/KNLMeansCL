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

#define DFT_D 0
#define DFT_A 2
#define DFT_S 4
#define DFT_cmode false
#define DFT_wmode 1
#define DFT_h 1.2f
#define DFT_ocl_device "DEFAULT"
#define DFT_ocl_id 0
#define DFT_lsb false
#define DFT_info false

#define GET_MSB(val) (val >> 8 & 0xFF)
#define GET_LSB(val) (val & 0xFF)
#define BIT_16(msb, lsb) (msb << 8 | lsb)

#ifdef _MSC_VER
    #define strcasecmp _stricmp
    #define snprintf sprintf_s
#endif

#include "KNLMeansCL.h"

#if defined(_MSC_VER) && defined(CL_VERSION_1_2)
    #pragma warning(disable : 4996)
#elif defined(__GNUC__) && defined(CL_VERSION_1_2)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

//////////////////////////////////////////
// AviSynthEquals
#ifdef __AVISYNTH_6_H__
inline bool KNLMeansClass::avs_equals(VideoInfo *v, VideoInfo *w) {
    return v->width == w->width && v->height == w->height && v->fps_numerator == w->fps_numerator
        && v->fps_denominator == w->fps_denominator && v->num_frames == w->num_frames;
}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthEquals
#ifdef VAPOURSYNTH_H
inline bool vs_equals(const VSVideoInfo *v, const VSVideoInfo *w) {
    return v->width == w->width && v->height == w->height && v->fpsNum == w->fpsNum && 
        v->fpsDen == w->fpsDen && v->numFrames == w->numFrames && v->format == w->format;
}
#endif //__VAPOURSYNTH_H__

//////////////////////////////////////////
// AviSynthMemoryManagement 
#ifdef __AVISYNTH_6_H__
inline cl_int KNLMeansClass::readBufferImage(PVideoFrame &frm, cl_command_queue command_queue,
    cl_mem image, const size_t origin[3], const size_t region[3]) {

    cl_int ret = CL_SUCCESS;
    switch (color) {
        case Gray:
            if (lsb) {
                ret = clEnqueueReadImage(command_queue, image, CL_TRUE, origin, region,
                    idmn[0] * sizeof(uint16_t), 0, hostBuffer, 0, NULL, NULL);
                uint16_t *bufferp = (uint16_t*) hostBuffer;
                int srcY_s = frm->GetPitch(PLANAR_Y);
                uint8_t *dstYmp = frm->GetWritePtr(PLANAR_Y);
                uint8_t *dstYlp = srcY_s * idmn[1] + dstYmp;
                #pragma omp parallel for
                for (int y = 0; y < (int) idmn[1]; y++) {                   
                    for (int x = 0; x < (int) idmn[0]; x++) {
                        dstYmp[y * srcY_s + x] = (uint8_t) GET_MSB(bufferp[y * idmn[0] + x]);
                        dstYlp[y * srcY_s + x] = (uint8_t) GET_LSB(bufferp[y * idmn[0] + x]);
                    }
                }
                break;
            } else {
                ret = clEnqueueReadImage(command_queue, image, CL_TRUE, origin, region,
                    (size_t) frm->GetPitch(PLANAR_Y), 0, frm->GetWritePtr(PLANAR_Y), 0, NULL, NULL);
                break;
            }
        case YUV:
            if (lsb) {
                int buffer_s = 4 * (int) idmn[0];
                ret = clEnqueueReadImage(command_queue, image, CL_TRUE, origin, region,
                    buffer_s * sizeof(uint16_t), 0, hostBuffer, 0, NULL, NULL);
                uint16_t *bufferp = (uint16_t*) hostBuffer;              
                int dstY_s = frm->GetPitch(PLANAR_Y);
                int dstU_s = frm->GetPitch(PLANAR_U);
                int dstV_s = frm->GetPitch(PLANAR_V);
                uint8_t *dstYmp = frm->GetWritePtr(PLANAR_Y);
                uint8_t *dstUmp = frm->GetWritePtr(PLANAR_U);
                uint8_t *dstVmp = frm->GetWritePtr(PLANAR_V);
                uint8_t *dstYlp = dstY_s * idmn[1] + dstYmp;
                uint8_t *dstUlp = dstU_s * idmn[1] + dstUmp;
                uint8_t *dstVlp = dstV_s * idmn[1] + dstVmp;               
                #pragma omp parallel for
                for (int y = 0; y < (int) idmn[1]; y++) {      
                    for (int x = 0; x < (int) idmn[0]; x++) {
                        dstYmp[y * dstY_s + x] = (uint8_t) GET_MSB(bufferp[y * buffer_s + x * 4 + 0]);
                        dstYlp[y * dstY_s + x] = (uint8_t) GET_LSB(bufferp[y * buffer_s + x * 4 + 0]);
                        dstUmp[y * dstU_s + x] = (uint8_t) GET_MSB(bufferp[y * buffer_s + x * 4 + 1]);
                        dstUlp[y * dstU_s + x] = (uint8_t) GET_LSB(bufferp[y * buffer_s + x * 4 + 1]);
                        dstVmp[y * dstV_s + x] = (uint8_t) GET_MSB(bufferp[y * buffer_s + x * 4 + 2]);
                        dstVlp[y * dstV_s + x] = (uint8_t) GET_LSB(bufferp[y * buffer_s + x * 4 + 2]);
                    }
                }
                break;
            } else {
                int buffer_s = 4 * (int) idmn[0];
                ret = clEnqueueReadImage(command_queue, image, CL_TRUE, origin, region,
                    buffer_s * sizeof(uint8_t), 0, hostBuffer, 0, NULL, NULL);
                uint8_t *bufferp = (uint8_t*) hostBuffer;               
                int dstY_s = frm->GetPitch(PLANAR_Y);
                int dstU_s = frm->GetPitch(PLANAR_U);
                int dstV_s = frm->GetPitch(PLANAR_V);
                uint8_t *dstYp = frm->GetWritePtr(PLANAR_Y);
                uint8_t *dstUp = frm->GetWritePtr(PLANAR_U);
                uint8_t *dstVp = frm->GetWritePtr(PLANAR_V);               
                #pragma omp parallel for
                for (int y = 0; y < (int) idmn[1]; y++) {
                    for (int x = 0; x < (int) idmn[0]; x++) {
                        dstYp[y * dstY_s + x] = bufferp[y * buffer_s + x * 4 + 0];
                        dstUp[y * dstU_s + x] = bufferp[y * buffer_s + x * 4 + 1];
                        dstVp[y * dstV_s + x] = bufferp[y * buffer_s + x * 4 + 2];
                    }
                }
                break;
            }
        case RGB:
            ret = clEnqueueReadImage(command_queue, image, CL_TRUE, origin, region,
                (size_t) frm->GetPitch(), 0, frm->GetWritePtr(), 0, NULL, NULL);
            break;
    }
    return ret;
}

inline cl_int KNLMeansClass::writeBufferImage(PVideoFrame &frm, cl_command_queue command_queue,
    cl_mem image, const size_t origin[3], const size_t region[3]) {

    cl_int ret = CL_SUCCESS;
    switch (color) {
        case Gray:
            if (lsb) {
                uint16_t *bufferp = (uint16_t*) hostBuffer;
                int srcY_s = frm->GetPitch(PLANAR_Y);
                const uint8_t *srcYmp = frm->GetReadPtr(PLANAR_Y);
                const uint8_t *srcYlp = srcY_s * idmn[1] + srcYmp;
                #pragma omp parallel for
                for (int y = 0; y < (int) idmn[1]; y++) {
                    for (int x = 0; x < (int) idmn[0]; x++) {
                        bufferp[y * idmn[0] + x] = (uint16_t) BIT_16(srcYmp[y * srcY_s + x], srcYlp[y * srcY_s + x]);
                    }
                }
                ret = clEnqueueWriteImage(command_queue, image, CL_TRUE, origin, region,
                    idmn[0] * sizeof(uint16_t), 0, hostBuffer, 0, NULL, NULL);
                break;
            } else {
                ret = clEnqueueWriteImage(command_queue, image, CL_TRUE, origin, region,
                    (size_t) frm->GetPitch(PLANAR_Y), 0, frm->GetReadPtr(PLANAR_Y), 0, NULL, NULL);
                break;
            }
        case YUV:
            if (lsb) {
                uint16_t *bufferp = (uint16_t*) hostBuffer;
                int buffer_s = 4 * (int) idmn[0];
                int srcY_s = frm->GetPitch(PLANAR_Y);
                int srcU_s = frm->GetPitch(PLANAR_U);
                int srcV_s = frm->GetPitch(PLANAR_V);
                const uint8_t *srcYmp = frm->GetReadPtr(PLANAR_Y);
                const uint8_t *srcUmp = frm->GetReadPtr(PLANAR_U);
                const uint8_t *srcVmp = frm->GetReadPtr(PLANAR_V);
                const uint8_t *srcYlp = srcY_s * idmn[1] + srcYmp;
                const uint8_t *srcUlp = srcU_s * idmn[1] + srcUmp;
                const uint8_t *srcVlp = srcV_s * idmn[1] + srcVmp;         
                #pragma omp parallel for
                for (int y = 0; y < (int) idmn[1]; y++) {                 
                    for (int x = 0; x < (int) idmn[0]; x++) {
                        bufferp[y * buffer_s + x * 4 + 0] = 
                            (uint16_t) BIT_16(srcYmp[y * srcY_s + x], srcYlp[y * srcY_s + x]);
                        bufferp[y * buffer_s + x * 4 + 1] = 
                            (uint16_t) BIT_16(srcUmp[y * srcU_s + x], srcUlp[y * srcU_s + x]);
                        bufferp[y * buffer_s + x * 4 + 2] = 
                            (uint16_t) BIT_16(srcVmp[y * srcV_s + x], srcVlp[y * srcV_s + x]);
                        bufferp[y * buffer_s + x * 4 + 3] = 0;
                    }
                }
                ret = clEnqueueWriteImage(command_queue, image, CL_TRUE, origin, region,
                    buffer_s * sizeof(uint16_t), 0, hostBuffer, 0, NULL, NULL);
                break;
            } else {
                uint8_t *bufferp = (uint8_t*) hostBuffer;
                int buffer_s = 4 * (int) idmn[0];
                int srcY_s = frm->GetPitch(PLANAR_Y);
                int srcU_s = frm->GetPitch(PLANAR_U);
                int srcV_s = frm->GetPitch(PLANAR_V);
                const uint8_t *srcYp = frm->GetReadPtr(PLANAR_Y);
                const uint8_t *srcUp = frm->GetReadPtr(PLANAR_U);
                const uint8_t *srcVp = frm->GetReadPtr(PLANAR_V);    
                #pragma omp parallel for
                for (int y = 0; y < (int) idmn[1]; y++) {
                    for (int x = 0; x < (int) idmn[0]; x++) {
                        bufferp[y * buffer_s + x * 4 + 0] = srcYp[y * srcY_s + x];
                        bufferp[y * buffer_s + x * 4 + 1] = srcUp[y * srcU_s + x];
                        bufferp[y * buffer_s + x * 4 + 2] = srcVp[y * srcV_s + x];
                        bufferp[y * buffer_s + x * 4 + 3] = 0;
                    }
                }
                ret = clEnqueueWriteImage(command_queue, image, CL_TRUE, origin, region,
                    buffer_s * sizeof(uint8_t), 0, hostBuffer, 0, NULL, NULL);
                break;
            }      
        case RGB:
            ret = clEnqueueWriteImage(command_queue, image, CL_TRUE, origin, region,
                (size_t) frm->GetPitch(), 0, frm->GetReadPtr(), 0, NULL, NULL);
            break;
    }
    return ret;
}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// AviSynthInit
#ifdef __AVISYNTH_6_H__
KNLMeansClass::KNLMeansClass(PClip _child, const int _d, const int _a, const int _s, const bool _cmode, 
    const int _wmode, const double _h, PClip _baby, const char* _ocl_device, const int _ocl_id, const bool _lsb, 
    const bool _info, IScriptEnvironment* env) : GenericVideoFilter(_child), d(_d), a(_a), s(_s), cmode(_cmode), 
    wmode(_wmode), h(_h), baby(_baby), ocl_device(_ocl_device), ocl_id(_ocl_id), lsb(_lsb), info(_info) {

    // Checks AviSynth Version.
    env->CheckVersion(6);
    child->SetCacheHints(CACHE_WINDOW, d);
    baby->SetCacheHints(CACHE_WINDOW, d);

    // Checks source clip and rclip.
    cl_channel_order corder = 0;
    cl_channel_type ctype = 0;
    if (cmode && !vi.IsYV24() && !vi.IsRGB32())
        env->ThrowError("KNLMeansCL: cmode requires YV24 image format!");
    if (vi.IsPlanar() && (vi.IsY8() || vi.IsYV411() || vi.IsYV12() || vi.IsYV16())) {
        color = Gray;
        corder = CL_LUMINANCE;
        ctype = (cl_channel_type) (lsb ? CL_UNORM_INT16 : CL_UNORM_INT8);
    } else if (vi.IsPlanar() && vi.IsYV24()) {
        color = cmode ? YUV : Gray;
        corder = (cl_channel_order) (cmode ? CL_RGBA : CL_LUMINANCE);
        ctype = (cl_channel_type) (lsb ? CL_UNORM_INT16 : CL_UNORM_INT8);
    } else if (vi.IsRGB() && vi.IsRGB32()) {
        color = RGB;
        corder = CL_RGBA;
        ctype = CL_UNORM_INT8;
    } else {
        env->ThrowError("KNLMeansCL: planar YUV or RGB32 data!");
    }
    VideoInfo rvi = baby->GetVideoInfo();
    if (!avs_equals(&vi, &rvi))
        env->ThrowError("KNLMeansCL: rclip do not math source clip!");
     
    // Checks user value.
    if (vi.IsRGB() && lsb)
        env->ThrowError("KNLMeansCL: RGB48y is not supported!");
    if (vi.IsRGB() && info)
        env->ThrowError("KNLMeansCL: info requires YUV color space!");
    if (d < 0)
        env->ThrowError("KNLMeansCL: d must be greater than or equal to 0!");
    if (a < 0)
        env->ThrowError("KNLMeansCL: a must be greater than or equal to 0!");
    if (s < 0 || s > 4)
        env->ThrowError("KNLMeansCL: s must be in range [0, 4]!");
    if (wmode < 0 || wmode > 2)
        env->ThrowError("KNLMeansCL: wmode must be in range [0, 2]!");
    if (h <= 0.0f)
        env->ThrowError("KNLMeansCL: h must be greater than 0!");
    cl_device_type device = NULL;
    if (!strcasecmp(ocl_device, "CPU")) {
        device = CL_DEVICE_TYPE_CPU;
    } else if (!strcasecmp(ocl_device, "GPU")) {
        device = CL_DEVICE_TYPE_GPU;
    } else if (!strcasecmp(ocl_device, "ACCELERATOR")) {
        device = CL_DEVICE_TYPE_ACCELERATOR;
    } else if (!strcasecmp(ocl_device, "ALL")) {
        device = CL_DEVICE_TYPE_ALL;
    } else if (!strcasecmp(ocl_device, "DEFAULT")) {
        device = CL_DEVICE_TYPE_DEFAULT;
    } else {
        env->ThrowError("KNLMeansCL: device_type must be cpu, gpu, accelerator or default!");
    }
    if (ocl_id < 0) 
        env->ThrowError("KNLMeansCL: device_id must be greater than or equal to 0!");

    // Gets PlatformID and DeviceID.
    cl_uint num_platforms = 0;
    cl_int ret = clGetPlatformIDs(0, NULL, &num_platforms);
    if (num_platforms == 0) 
        env->ThrowError("KNLMeansCL: no opencl platforms available!");
    cl_platform_id *temp_platforms = (cl_platform_id*) malloc(sizeof(cl_platform_id) * num_platforms);
    ret |= clGetPlatformIDs(num_platforms, temp_platforms, NULL);
    if (ret != CL_SUCCESS) 
        env->ThrowError("KNLMeansCL: AviSynthCreate error (clGetPlatformID)!");
    sum_devices = 0;
    for (cl_uint i = 0; i < num_platforms; i++) {
        cl_uint num_devices = 0;
        clGetDeviceIDs(temp_platforms[i], device, 0, 0, &num_devices);
        sum_devices += num_devices;
    }
    if (sum_devices == 0)   
        env->ThrowError("KNLMeansCL: no opencl device available of type device_type!");
    if ((cl_uint) ocl_id >= sum_devices)
        env->ThrowError("KNLMeansCL: selected device is not available!");
    device_list *list = (device_list*) malloc(sizeof(device_list) * sum_devices);
    if (list == nullptr) 
        env->ThrowError("KNLMeansCL: malloc fails!");
    cl_uint index = 0;
    for (cl_uint i = 0; i < num_platforms; i++) {
        cl_uint num_devices = 0;
        clGetDeviceIDs(temp_platforms[i], device, 0, 0, &num_devices);
        for (cl_uint j = 0; j < num_devices; j++) {
            cl_device_id device_id = 0;
            clGetDeviceIDs(temp_platforms[i], device, 1, &device_id, NULL);
            if (index < sum_devices)
                list[index++] = { temp_platforms[i], device_id };
            else
                env->ThrowError("KNLMeansCL: AviSynthCreate error (device_list)!");
        }
    }
    cl_bool img_support = CL_FALSE;
    ret |= clGetDeviceInfo(list[ocl_id].device, CL_DEVICE_IMAGE_SUPPORT, sizeof(cl_bool), &img_support, NULL);
    if (ret != CL_SUCCESS)
        env->ThrowError("KNLMeansCL: AviSynthCreate error (clGetDeviceInfo)!");
    else if (img_support == CL_FALSE)
        env->ThrowError("KNLMeansCL: selected device does not support image!");
    platformID = list[ocl_id].platform;
    deviceID = list[ocl_id].device;
    free(temp_platforms);
    free(list);
    
    // Creates an OpenCL context, 2D images and buffers object.
    context = clCreateContext(NULL, 1, &deviceID, NULL, NULL, NULL);
    const cl_image_format image_format = { corder, ctype };
    idmn[0] = (cl_uint) vi.width;
    idmn[1] = (cl_uint) (lsb ? (vi.height / 2) : (vi.height));
    const size_t size = sizeof(float) * idmn[0] * idmn[1];
    mem_in[0] = clCreateImage2D(context, CL_MEM_READ_ONLY, &image_format, idmn[0], idmn[1], 0, NULL, &ret);
    if (ret == CL_IMAGE_FORMAT_NOT_SUPPORTED) 
        env->ThrowError("KNLMeansCL: this image format is not supported by your device!");
    mem_in[2] = clCreateImage2D(context, CL_MEM_READ_ONLY, &image_format, idmn[0], idmn[1], 0, NULL, NULL);
    if (d) {
        mem_in[1] = clCreateImage2D(context, CL_MEM_READ_ONLY, &image_format, idmn[0], idmn[1], 0, NULL, NULL);
        mem_in[3] = clCreateImage2D(context, CL_MEM_READ_ONLY, &image_format, idmn[0], idmn[1], 0, NULL, NULL);
    }
    mem_out = clCreateImage2D(context, CL_MEM_WRITE_ONLY, &image_format, idmn[0], idmn[1], 0, NULL, NULL);
    mem_U[0] = clCreateBuffer(context, CL_MEM_READ_WRITE, color ? 4 * size : 2 * size, NULL, NULL);
    mem_U[1] = clCreateBuffer(context, CL_MEM_READ_WRITE, size, NULL, NULL);
    mem_U[2] = clCreateBuffer(context, CL_MEM_READ_WRITE, size, NULL, NULL);
    mem_U[3] = clCreateBuffer(context, CL_MEM_READ_WRITE, size, NULL, NULL);
 
    // Host buffer.
    if (lsb) {
        if (color == Gray) hostBuffer = malloc(idmn[0] * idmn[1] * sizeof(uint16_t));
        else if (color == YUV) hostBuffer = malloc(idmn[0] * idmn[1] * 4 * sizeof(uint16_t));
        else hostBuffer = nullptr;
    } else {
        if (color == YUV) hostBuffer = malloc(idmn[0] * idmn[1] * 4 * sizeof(uint8_t));
        else hostBuffer = nullptr;
    }

    // Creates and Build a program executable from the program source.
    program = clCreateProgramWithSource(context, 1, &source_code, NULL, NULL);
    char options[2048];
    setlocale(LC_ALL, "C");
    snprintf(options, 2048, "-cl-single-precision-constant -cl-denorms-are-zero -cl-fast-relaxed-math -Werror \
        -D H_BLOCK_X=%i -D H_BLOCK_Y=%i -D V_BLOCK_X=%i -D V_BLOCK_Y=%i \
        -D NLMK_TCOLOR=%i -D NLMK_S=%i -D NLMK_WMODE=%i -D NLMK_TEMPORAL=%i \
        -D NLMK_H2_INV_NORM=%f -D NLMK_PACK=%f -D NLMK_UNPACK=%f",
        H_BLOCK_X, H_BLOCK_Y, V_BLOCK_X, V_BLOCK_Y,
        color, s, wmode, d, 
        65025.0 / (3*h*h*(2 * s + 1) * (2 * s + 1)), 1.0, 1.0);
    setlocale(LC_ALL, "");
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

    // Creates kernel objects.
    kernel[0] = clCreateKernel(program, "NLM_init", NULL);
    kernel[1] = clCreateKernel(program, "NLM_dist", NULL);
    kernel[2] = clCreateKernel(program, "NLM_horiz", NULL);
    kernel[3] = clCreateKernel(program, "NLM_vert", NULL);
    kernel[4] = clCreateKernel(program, "NLM_accu", NULL);
    kernel[5] = clCreateKernel(program, "NLM_finish", NULL);

    // Sets kernel arguments.
    ret = clSetKernelArg(kernel[0], 0, sizeof(cl_mem), &mem_U[0]);
    ret |= clSetKernelArg(kernel[0], 1, sizeof(cl_mem), &mem_U[3]);
    ret |= clSetKernelArg(kernel[0], 2, 2 * sizeof(cl_uint), &idmn);
    ret |= clSetKernelArg(kernel[1], 0, sizeof(cl_mem), &mem_in[2]);
    ret |= clSetKernelArg(kernel[1], 1, sizeof(cl_mem), &mem_in[d ? 3 : 2]);
    ret |= clSetKernelArg(kernel[1], 2, sizeof(cl_mem), &mem_U[1]);
    ret |= clSetKernelArg(kernel[1], 3, 2 * sizeof(cl_uint), &idmn);
    ret |= clSetKernelArg(kernel[2], 0, sizeof(cl_mem), &mem_U[1]);
    ret |= clSetKernelArg(kernel[2], 1, sizeof(cl_mem), &mem_U[2]);
    ret |= clSetKernelArg(kernel[2], 2, 2 * sizeof(cl_uint), &idmn);
    ret |= clSetKernelArg(kernel[3], 0, sizeof(cl_mem), &mem_U[2]);
    ret |= clSetKernelArg(kernel[3], 1, sizeof(cl_mem), &mem_U[1]);
    ret |= clSetKernelArg(kernel[3], 2, 2 * sizeof(cl_uint), &idmn);
    ret |= clSetKernelArg(kernel[4], 0, sizeof(cl_mem), &mem_in[d ? 1 : 0]);
    ret |= clSetKernelArg(kernel[4], 1, sizeof(cl_mem), &mem_U[0]);
    ret |= clSetKernelArg(kernel[4], 2, sizeof(cl_mem), &mem_U[1]);
    ret |= clSetKernelArg(kernel[4], 3, sizeof(cl_mem), &mem_U[3]);
    ret |= clSetKernelArg(kernel[4], 4, 2 * sizeof(cl_uint), &idmn);
    ret |= clSetKernelArg(kernel[5], 0, sizeof(cl_mem), &mem_in[0]);
    ret |= clSetKernelArg(kernel[5], 1, sizeof(cl_mem), &mem_out);
    ret |= clSetKernelArg(kernel[5], 2, sizeof(cl_mem), &mem_U[0]);
    ret |= clSetKernelArg(kernel[5], 3, sizeof(cl_mem), &mem_U[3]);
    ret |= clSetKernelArg(kernel[5], 4, 2 * sizeof(cl_uint), &idmn);
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
    PVideoFrame ref = baby->GetFrame(n, env);
    const size_t origin[3] = { 0, 0, 0 };
    const size_t region[3] = { idmn[0], idmn[1], 1 };
    const size_t global_work[2] = {
        mrounds(idmn[0], fastmax(H_BLOCK_X, V_BLOCK_X)),
        mrounds(idmn[1], fastmax(H_BLOCK_Y, V_BLOCK_Y))
    };
    const size_t local_horiz[2] = { H_BLOCK_X, H_BLOCK_Y };
    const size_t local_vert[2] = { V_BLOCK_X, V_BLOCK_Y };

    //Copy chroma.
    PVideoFrame dst = env->NewVideoFrame(vi);
    if (!vi.IsY8() && color == Gray) {
        env->BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), src->GetReadPtr(PLANAR_U),
            src->GetPitch(PLANAR_U), src->GetRowSize(PLANAR_U), src->GetHeight(PLANAR_U));
        env->BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), src->GetReadPtr(PLANAR_V),
            src->GetPitch(PLANAR_V), src->GetRowSize(PLANAR_V), src->GetHeight(PLANAR_V));
    } 

    // Processing.
    cl_int ret = CL_SUCCESS;
    cl_command_queue command_queue = clCreateCommandQueue(context, deviceID, 0, NULL);
    ret |= writeBufferImage(src, command_queue, mem_in[0], origin, region);
    ret |= writeBufferImage(ref, command_queue, mem_in[2], origin, region);
    ret |= clEnqueueNDRangeKernel(command_queue, kernel[0], 2, NULL, global_work, NULL, 0, NULL, NULL);
    if (d) {
        // Temporal.
        const int maxframe = vi.num_frames - 1;
        for (int k = -d; k <= d; k++) {
            src = child->GetFrame(clamp(n + k, 0, maxframe), env);
            ref = baby->GetFrame(clamp(n + k, 0, maxframe), env);
            ret |= writeBufferImage(src, command_queue, mem_in[1], origin, region);
            ret |= writeBufferImage(ref, command_queue, mem_in[3], origin, region);
            for (int j = -a; j <= a; j++) {
                for (int i = -a; i <= a; i++) {
                    if (k || j || i) {
                        const cl_int q[2] = { i, j };
                        ret |= clSetKernelArg(kernel[1], 4, 2 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[1],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[2],
                            2, NULL, global_work, local_horiz, 0, NULL, NULL);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[3],
                            2, NULL, global_work, local_vert, 0, NULL, NULL);
                        ret |= clSetKernelArg(kernel[4], 5, 2 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, kernel[4],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                    }
                }
            }
        }
    } else {
        // Spatial.
        for (int j = -a; j <= 0; j++) {
            for (int i = -a; i <= a; i++) {
                if (j * (2 * a + 1) + i < 0) {
                    const cl_int q[2] = { i, j };
                    ret |= clSetKernelArg(kernel[1], 4, 2 * sizeof(cl_int), &q);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[1],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[2],
                        2, NULL, global_work, local_horiz, 0, NULL, NULL);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[3],
                        2, NULL, global_work, local_vert, 0, NULL, NULL);
                    ret |= clSetKernelArg(kernel[4], 5, 2 * sizeof(cl_int), &q);
                    ret |= clEnqueueNDRangeKernel(command_queue, kernel[4],
                        2, NULL, global_work, NULL, 0, NULL, NULL);
                }
            }
        }
    }
    ret |= clEnqueueNDRangeKernel(command_queue, kernel[5], 2, NULL, global_work, NULL, 0, NULL, NULL);
    ret |= readBufferImage(dst, command_queue, mem_out, origin, region);
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
        snprintf(buffer, 2048, " Iterations: %li", ((2 * d + 1)*(2 * a + 1)*(2 * a + 1) - 1) / (d ? 1 : 2));
        DrawString(frm, pitch, 0, y++, buffer);
        snprintf(buffer, 2048, " Global work size: %lux%lu",
            (unsigned long) global_work[0], (unsigned long) global_work[1]);
        DrawString(frm, pitch, 0, y++, buffer);
        snprintf(buffer, 2048, " Number of devices: %u", sum_devices);
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
            vsapi->requestFrameFilter(clamp(n + k, 0, maxframe), d->knot, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        // Variables.
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFrameRef *ref = vsapi->getFrameFilter(n, d->knot, frameCtx);
        const VSFormat *fi = d->vi->format;
        VSFrameRef *dst = vsapi->newVideoFrame(fi, (int) d->idmn[0], (int) d->idmn[1], NULL, core);
        const size_t origin[3] = { 0, 0, 0 };
        const size_t region[3] = { d->idmn[0], d->idmn[1], 1 };
        const size_t global_work[2] = {
            mrounds(d->idmn[0], fastmax(H_BLOCK_X, V_BLOCK_X)),
            mrounds(d->idmn[1], fastmax(H_BLOCK_Y, V_BLOCK_Y))
        };
        const size_t local_horiz[2] = { H_BLOCK_X, H_BLOCK_Y };
        const size_t local_vert[2] = { V_BLOCK_X, V_BLOCK_Y };

        //Copy chroma.  
        if (fi->colorFamily == cmYUV && d->color == Gray) {
            vs_bitblt(vsapi->getWritePtr(dst, 1), vsapi->getStride(dst, 1), vsapi->getReadPtr(src, 1),
                vsapi->getStride(src, 1), vsapi->getFrameWidth(src, 1) * fi->bytesPerSample,
                vsapi->getFrameHeight(src, 1));
            vs_bitblt(vsapi->getWritePtr(dst, 2), vsapi->getStride(dst, 2), vsapi->getReadPtr(src, 2),
                vsapi->getStride(src, 2), vsapi->getFrameWidth(src, 2) * fi->bytesPerSample,
                vsapi->getFrameHeight(src, 2));
        }       

        // Processing.
        cl_int ret = CL_SUCCESS;
        cl_command_queue command_queue = clCreateCommandQueue(d->context, d->deviceID, 0, NULL);
        switch (d->color) {
            case Gray:
                ret |= clEnqueueWriteImage(command_queue, d->mem_in[0], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, d->mem_in[2], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                break;
            case YUV:
            case RGB:
                ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                ret |= clSetKernelArg(d->kernel[6], 3, sizeof(cl_mem), &d->mem_in[0]);
                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[6], 2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(ref, 1), 0, vsapi->getReadPtr(ref, 1), 0, NULL, NULL);
                ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(ref, 2), 0, vsapi->getReadPtr(ref, 2), 0, NULL, NULL);
                ret |= clSetKernelArg(d->kernel[6], 3, sizeof(cl_mem), &d->mem_in[2]);
                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[6], 2, NULL, global_work, NULL, 0, NULL, NULL);
                break;                         
        }
        vsapi->freeFrame(src);
        vsapi->freeFrame(ref);
        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[0], 2, NULL, global_work, NULL, 0, NULL, NULL);
        if (d->d) {
            // Temporal.
            for (int k = int64ToIntS(-d->d); k <= d->d; k++) {
                src = vsapi->getFrameFilter(clamp(n + k, 0, maxframe), d->node, frameCtx);
                ref = vsapi->getFrameFilter(clamp(n + k, 0, maxframe), d->knot, frameCtx);
                switch (d->color) {
                    case Gray:
                        ret |= clEnqueueWriteImage(command_queue, d->mem_in[1], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_in[3], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                        break;
                    case YUV:
                    case RGB:
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 0), 0, vsapi->getReadPtr(src, 0), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 1), 0, vsapi->getReadPtr(src, 1), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(src, 2), 0, vsapi->getReadPtr(src, 2), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[6], 3, sizeof(cl_mem), &d->mem_in[1]);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[6], 2, NULL, global_work,
                            NULL, 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 0), 0, vsapi->getReadPtr(ref, 0), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 1), 0, vsapi->getReadPtr(ref, 1), 0, NULL, NULL);
                        ret |= clEnqueueWriteImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                            (size_t) vsapi->getStride(ref, 2), 0, vsapi->getReadPtr(ref, 2), 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[6], 3, sizeof(cl_mem), &d->mem_in[3]);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[6], 2, NULL, global_work,
                            NULL, 0, NULL, NULL);
                        break;
                }
                vsapi->freeFrame(src);
                vsapi->freeFrame(ref);
                for (int j = int64ToIntS(-d->a); j <= d->a; j++) {
                    for (int i = int64ToIntS(-d->a); i <= d->a; i++) {
                        if (k || j || i) {
                            const cl_int q[2] = { i, j };
                            ret |= clSetKernelArg(d->kernel[1], 4, 2 * sizeof(cl_int), &q);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[1],
                                2, NULL, global_work, NULL, 0, NULL, NULL);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[2],
                                2, NULL, global_work, local_horiz, 0, NULL, NULL);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[3],
                                2, NULL, global_work, local_vert, 0, NULL, NULL);
                            ret |= clSetKernelArg(d->kernel[4], 5, 2 * sizeof(cl_int), &q);
                            ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[4],
                                2, NULL, global_work, NULL, 0, NULL, NULL);
                        }
                    }
                }
            }
        } else {
            // Spatial.
            for (int j = int64ToIntS(-d->a); j <= 0; j++) {
                for (int i = int64ToIntS(-d->a); i <= d->a; i++) {
                    if (j * (2 * d->a + 1) + i < 0) {
                        const cl_int q[2] = { i, j };
                        ret |= clSetKernelArg(d->kernel[1], 4, 2 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[1],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[2],
                            2, NULL, global_work, local_horiz, 0, NULL, NULL);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[3],
                            2, NULL, global_work, local_vert, 0, NULL, NULL);
                        ret |= clSetKernelArg(d->kernel[4], 5, 2 * sizeof(cl_int), &q);
                        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[4],
                            2, NULL, global_work, NULL, 0, NULL, NULL);
                    }
                }
            }
        }
        ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[5], 2, NULL, global_work, NULL, 0, NULL, NULL);
        switch (d->color) {
            case Gray:
                ret |= clEnqueueReadImage(command_queue, d->mem_out, CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(dst, 0), 0, vsapi->getWritePtr(dst, 0), 0, NULL, NULL);            
                break;
            case YUV:
            case RGB:
                ret |= clEnqueueNDRangeKernel(command_queue, d->kernel[7], 2, NULL, global_work, NULL, 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, d->mem_P[0], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(dst, 0), 0, vsapi->getWritePtr(dst, 0), 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, d->mem_P[1], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(dst, 1), 0, vsapi->getWritePtr(dst, 1), 0, NULL, NULL);
                ret |= clEnqueueReadImage(command_queue, d->mem_P[2], CL_TRUE, origin, region,
                    (size_t) vsapi->getStride(dst, 2), 0, vsapi->getWritePtr(dst, 2), 0, NULL, NULL);                             
                break;
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
            snprintf(buffer, 2048, " D:%i  A:%ix%i  S:%ix%i", 2 * int64ToIntS(d->d) + 1,
                2 * int64ToIntS(d->a) + 1, 2 * int64ToIntS(d->a) + 1, 2 * int64ToIntS(d->s) + 1, 
                2 * int64ToIntS(d->s) + 1);
            DrawString(frm, pitch, 0, y++, buffer);
            snprintf(buffer, 2048, " Iterations: %i", ((2 * int64ToIntS(d->d) + 1)*(2 * int64ToIntS(d->a) + 1)*
                (2 * int64ToIntS(d->a) + 1) - 1) / (int64ToIntS(d->d) ? 1 : 2));
            DrawString(frm, pitch, 0, y++, buffer);
            snprintf(buffer, 2048, " Global work size: %lux%lu",
                (unsigned long) global_work[0], (unsigned long) global_work[1]);
            DrawString(frm, pitch, 0, y++, buffer);
            snprintf(buffer, 2048, " Number of devices: %u", d->sum_devices);
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
    clReleaseMemObject(mem_U[3]);
    clReleaseMemObject(mem_U[2]);
    clReleaseMemObject(mem_U[1]);
    clReleaseMemObject(mem_U[0]);
    clReleaseMemObject(mem_out);
    if (d) {
        clReleaseMemObject(mem_in[3]);
        clReleaseMemObject(mem_in[1]);
    }
    clReleaseMemObject(mem_in[2]);
    clReleaseMemObject(mem_in[0]);
    clReleaseKernel(kernel[5]);
    clReleaseKernel(kernel[4]);
    clReleaseKernel(kernel[3]);
    clReleaseKernel(kernel[2]);
    clReleaseKernel(kernel[1]);
    clReleaseKernel(kernel[0]);
    clReleaseProgram(program);
    clReleaseContext(context);
    free(hostBuffer);
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
    clReleaseMemObject(d->mem_in[3]);
    clReleaseMemObject(d->mem_in[1]);
    clReleaseMemObject(d->mem_in[2]);
    clReleaseMemObject(d->mem_in[0]);
    clReleaseKernel(d->kernel[5]);
    clReleaseKernel(d->kernel[4]);
    clReleaseKernel(d->kernel[3]);
    clReleaseKernel(d->kernel[2]);
    clReleaseKernel(d->kernel[1]);
    clReleaseKernel(d->kernel[0]);
    clReleaseProgram(d->program);
    clReleaseContext(d->context);
    free(d);
}
#endif //__VAPOURSYNTH_H__

//////////////////////////////////////////
// AviSynthCreate
#ifdef __AVISYNTH_6_H__
AVSValue __cdecl AviSynthPluginCreate(AVSValue args, void* user_data, IScriptEnvironment* env) {
    return new KNLMeansClass(args[0].AsClip(), args[1].AsInt(DFT_D), args[2].AsInt(DFT_A), args[3].AsInt(DFT_S),
        args[4].AsBool(DFT_cmode), args[5].AsInt(DFT_wmode), args[6].AsFloat(DFT_h), args[7].Defined() ? 
        args[7].AsClip() : args[0].AsClip(), args[8].AsString(DFT_ocl_device), args[9].AsInt(DFT_ocl_id), 
        args[10].AsBool(DFT_lsb), args[11].AsBool(DFT_info), env);
}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthCreate
#ifdef VAPOURSYNTH_H
static void VS_CC VapourSynthPluginCreate(const VSMap *in, VSMap *out,
    void *userData, VSCore *core, const VSAPI *vsapi) {

    // Checks source clip and rclip.
    KNLMeansData d;
    cl_channel_order channel_order = 0;
    cl_channel_type channel_type = 0;
    double pack = 1.0, unpack = 1.0;
    int err;
    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.knot = vsapi->propGetNode(in, "rclip", 0, &err);
    if (err) d.knot = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = vsapi->getVideoInfo(d.node);
    if (isConstantFormat(d.vi)) {
        d.cmode = vsapi->propGetInt(in, "cmode", 0, &err);
        if (err) d.cmode = DFT_cmode;
        if (d.cmode && (d.vi->format->subSamplingW != 0) && (d.vi->format->subSamplingH != 0)) {
            vsapi->setError(out, "knlm.KNLMeansCL: cmode requires 4:4:4 subsampling!");
            vsapi->freeNode(d.node);
            vsapi->freeNode(d.knot);
            return;
        }
        switch (d.vi->format->bitsPerSample) {
            case 9:
                pack = 65535.0 / 511.0;
                unpack = 511.0 / 65535.0;
                break;
            case 10:
                pack = 65535.0 / 1023.0;
                unpack = 1023.0 / 65535.0;
                break;
        }
        switch (d.vi->format->id) {
            case VSPresetFormat::pfGray8:
            case VSPresetFormat::pfYUV420P8:
            case VSPresetFormat::pfYUV422P8:
            case VSPresetFormat::pfYUV410P8:
            case VSPresetFormat::pfYUV411P8:
            case VSPresetFormat::pfYUV440P8:
                d.color = Gray;
                channel_order = CL_LUMINANCE;
                channel_type = CL_UNORM_INT8; 
                break;
            case VSPresetFormat::pfGray16:
            case VSPresetFormat::pfYUV420P9:
            case VSPresetFormat::pfYUV422P9:
            case VSPresetFormat::pfYUV420P10:
            case VSPresetFormat::pfYUV422P10:
            case VSPresetFormat::pfYUV420P16:
            case VSPresetFormat::pfYUV422P16:
                d.color = Gray;
                channel_order = CL_LUMINANCE;
                channel_type = CL_UNORM_INT16;               
                break;
            case VSPresetFormat::pfGrayH:
                d.color = Gray;
                channel_order = CL_LUMINANCE;
                channel_type = CL_HALF_FLOAT;
                break;
            case VSPresetFormat::pfGrayS:
                d.color = Gray;
                channel_order = CL_LUMINANCE;
                channel_type = CL_FLOAT;
                break;
            case VSPresetFormat::pfYUV444P8:
                d.color = d.cmode ? YUV : Gray;
                channel_order = (cl_channel_order) (d.cmode ? CL_RGBA : CL_LUMINANCE);
                channel_type = CL_UNORM_INT8;
                break;
            case VSPresetFormat::pfYUV444P9:
            case VSPresetFormat::pfYUV444P10:
            case VSPresetFormat::pfYUV444P16:
                d.color = d.cmode ? YUV : Gray;
                channel_order = (cl_channel_order) (d.cmode ? CL_RGBA : CL_LUMINANCE);
                channel_type = CL_UNORM_INT16;
                break;
            case VSPresetFormat::pfYUV444PH:
                d.color = d.cmode ? YUV : Gray;
                channel_order = (cl_channel_order) (d.cmode ? CL_RGBA : CL_LUMINANCE);
                channel_type = CL_HALF_FLOAT;
                break;
            case VSPresetFormat::pfYUV444PS:
                d.color = d.cmode ? YUV : Gray;
                channel_order = (cl_channel_order) (d.cmode ? CL_RGBA : CL_LUMINANCE);
                channel_type = CL_FLOAT;
                break;
            case VSPresetFormat::pfRGB24:
                d.color = RGB;
                channel_order = CL_RGBA;
                channel_type = CL_UNORM_INT8;
                break;
            case VSPresetFormat::pfRGB27:
            case VSPresetFormat::pfRGB30:
            case VSPresetFormat::pfRGB48:
                d.color = RGB;
                channel_order = CL_RGBA;
                channel_type = CL_UNORM_INT16;
                break;
            case VSPresetFormat::pfRGBH:
                d.color = RGB;
                channel_order = CL_RGBA;
                channel_type = CL_HALF_FLOAT;
                break;
            case VSPresetFormat::pfRGBS:
                d.color = RGB;
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
    if (!vs_equals(d.vi, vsapi->getVideoInfo(d.knot))) {
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
    if (d.a < 0) {
        vsapi->setError(out, "knlm.KNLMeansCL: a must be greater than or equal to 0!");
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
    cl_device_type device;
    if (!strcasecmp(d.ocl_device, "CPU")) {
        device = CL_DEVICE_TYPE_CPU;
    } else if (!strcasecmp(d.ocl_device, "GPU")) {
        device = CL_DEVICE_TYPE_GPU;
    } else if (!strcasecmp(d.ocl_device, "ACCELERATOR")) {
        device = CL_DEVICE_TYPE_ACCELERATOR;
    } else if (!strcasecmp(d.ocl_device, "ALL")) {
        device = CL_DEVICE_TYPE_ALL;
    } else if (!strcasecmp(d.ocl_device, "DEFAULT")) {
        device = CL_DEVICE_TYPE_DEFAULT;
    } else {
        vsapi->setError(out, "knlm.KNLMeansCL: device_type must be cpu, gpu, accelerator or default!");
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
    cl_uint num_platforms = 0;
    cl_int ret = clGetPlatformIDs(0, NULL, &num_platforms);
    if (num_platforms == 0) {
        vsapi->setError(out, "knlm.KNLMeansCL: no opencl platforms available!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    cl_platform_id *temp_platforms = (cl_platform_id*) malloc(sizeof(cl_platform_id) * num_platforms);
    ret |= clGetPlatformIDs(num_platforms, temp_platforms, NULL);
    if (ret != CL_SUCCESS) {
        vsapi->setError(out, "knlm.KNLMeansCL: VapourSynthCreate error (clGetPlatformID)!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    d.sum_devices = 0;
    for (cl_uint i = 0; i < num_platforms; i++) {
        cl_uint num_devices = 0;
        clGetDeviceIDs(temp_platforms[i], device, 0, 0, &num_devices);
        d.sum_devices += num_devices;
    }
    if (d.sum_devices == 0) {
        vsapi->setError(out, "knlm.KNLMeansCL: no opencl device available of type device_type!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    if (d.ocl_id >= d.sum_devices) {
        vsapi->setError(out, "knlm.KNLMeansCL: selected device is not available!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    device_list *list = (device_list*) malloc(sizeof(device_list) * d.sum_devices);
    if (list == nullptr) {
        vsapi->setError(out, "knlm.KNLMeansCL: malloc fails!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    cl_uint index = 0;
    for (cl_uint i = 0; i < num_platforms; i++) {
        cl_uint num_devices = 0;
        clGetDeviceIDs(temp_platforms[i], device, 0, 0, &num_devices);
        for (cl_uint j = 0; j < num_devices; j++) {
            cl_device_id device_id = 0;
            clGetDeviceIDs(temp_platforms[i], device, 1, &device_id, NULL);
            if (index < d.sum_devices)
                list[index++] = { temp_platforms[i], device_id };
            else {
                vsapi->setError(out, "knlm.KNLMeansCL: AviSynthCreate error (device_list)!");
                vsapi->freeNode(d.node);
                vsapi->freeNode(d.knot);
                return;
            }
        }
    }
    cl_bool img_support = CL_FALSE;
    ret |= clGetDeviceInfo(list[d.ocl_id].device, CL_DEVICE_IMAGE_SUPPORT, sizeof(cl_bool), &img_support, NULL);
    if (ret != CL_SUCCESS) {
        vsapi->setError(out, "knlm.KNLMeansCL: VapourSynthCreate error (clGetDeviceInfo)!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    } else if (img_support == CL_FALSE) {
        vsapi->setError(out, "knlm.KNLMeansCL: selected device does not support image!");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }
    d.platformID = list[d.ocl_id].platform;
    d.deviceID = list[d.ocl_id].device;
    free(temp_platforms);
    free(list);

    // Creates an OpenCL context, 2D images and buffers object.
    d.context = clCreateContext(NULL, 1, &d.deviceID, NULL, NULL, NULL);
    const cl_image_format image_format = { channel_order, channel_type };
    const cl_image_format image_formatp = { CL_LUMINANCE, channel_type };
    d.idmn[0] = (cl_uint) d.vi->width;
    d.idmn[1] = (cl_uint) d.vi->height;
    const size_t size = sizeof(float) * d.idmn[0] * d.idmn[1];
    d.mem_in[0] = clCreateImage2D(d.context, CL_MEM_READ_WRITE, &image_format, d.idmn[0], d.idmn[1], 0, NULL, NULL);
    d.mem_in[1] = clCreateImage2D(d.context, CL_MEM_READ_WRITE, &image_format, d.idmn[0], d.idmn[1], 0, NULL, NULL);
    d.mem_in[2] = clCreateImage2D(d.context, CL_MEM_READ_WRITE, &image_format, d.idmn[0], d.idmn[1], 0, NULL, NULL);     
    d.mem_in[3] = clCreateImage2D(d.context, CL_MEM_READ_WRITE, &image_format, d.idmn[0], d.idmn[1], 0, NULL, NULL); 
    d.mem_out = clCreateImage2D(d.context, CL_MEM_READ_WRITE, &image_format, d.idmn[0], d.idmn[1], 0, NULL, NULL);
    d.mem_U[0] = clCreateBuffer(d.context, CL_MEM_READ_WRITE, d.color ? 4 * size : 2 * size, NULL, NULL);
    d.mem_U[1] = clCreateBuffer(d.context, CL_MEM_READ_WRITE, size, NULL, NULL);
    d.mem_U[2] = clCreateBuffer(d.context, CL_MEM_READ_WRITE, size, NULL, NULL);
    d.mem_U[3] = clCreateBuffer(d.context, CL_MEM_READ_WRITE, size, NULL, NULL);
    d.mem_P[0] = clCreateImage2D(d.context, CL_MEM_READ_WRITE, &image_formatp, d.idmn[0], d.idmn[1], 0, NULL, NULL);
    d.mem_P[1] = clCreateImage2D(d.context, CL_MEM_READ_WRITE, &image_formatp, d.idmn[0], d.idmn[1], 0, NULL, NULL);
    d.mem_P[2] = clCreateImage2D(d.context, CL_MEM_READ_WRITE, &image_formatp, d.idmn[0], d.idmn[1], 0, NULL, NULL);

    // Creates and Build a program executable from the program source.
    d.program = clCreateProgramWithSource(d.context, 1, &source_code, NULL, NULL);
    char options[2048];
    setlocale(LC_ALL, "C");
    if (channel_type == CL_FLOAT) {
        snprintf(options, 2048, "-cl-single-precision-constant -Werror \
            -D H_BLOCK_X=%i -D H_BLOCK_Y=%i -D V_BLOCK_X=%i -D V_BLOCK_Y=%i \
            -D NLMK_TCOLOR=%i -D NLMK_S=%i -D NLMK_WMODE=%i -D NLMK_TEMPORAL=%i \
            -D NLMK_H2_INV_NORM=%f -D NLMK_PACK=%f -D NLMK_UNPACK=%f",
            H_BLOCK_X, H_BLOCK_Y, V_BLOCK_X, V_BLOCK_Y,
            d.color, int64ToIntS(d.s), int64ToIntS(d.wmode), int64ToIntS(d.d), 
            65025.0 / (3 * d.h*d.h*(2 * d.s + 1) * (2 * d.s + 1)), pack, unpack);
    } else {
        snprintf(options, 2048, "-cl-single-precision-constant -cl-denorms-are-zero -cl-fast-relaxed-math -Werror \
           -D H_BLOCK_X=%i -D H_BLOCK_Y=%i -D V_BLOCK_X=%i -D V_BLOCK_Y=%i \
           -D NLMK_TCOLOR=%i -D NLMK_S=%i -D NLMK_WMODE=%i -D NLMK_TEMPORAL=%i \
           -D NLMK_H2_INV_NORM=%f -D NLMK_PACK=%f -D NLMK_UNPACK=%f",
           H_BLOCK_X, H_BLOCK_Y, V_BLOCK_X, V_BLOCK_Y,
           d.color, int64ToIntS(d.s), int64ToIntS(d.wmode), int64ToIntS(d.d),
           65025.0 / (3 * d.h*d.h*(2 * d.s + 1) * (2 * d.s + 1)), pack, unpack);
    }
    setlocale(LC_ALL, "");
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
        vsapi->setError(out, "knlm.KNLMeansCL: VapourSynthCreate error (clBuildProgram)!\n" 
            "Please report Log-KNLMeansCL.txt.");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.knot);
        return;
    }

    // Creates kernel objects.
    d.kernel[0] = clCreateKernel(d.program, "NLM_init", NULL);
    d.kernel[1] = clCreateKernel(d.program, "NLM_dist", NULL);
    d.kernel[2] = clCreateKernel(d.program, "NLM_horiz", NULL);
    d.kernel[3] = clCreateKernel(d.program, "NLM_vert", NULL);
    d.kernel[4] = clCreateKernel(d.program, "NLM_accu", NULL);
    d.kernel[5] = clCreateKernel(d.program, "NLM_finish", NULL);
    d.kernel[6] = clCreateKernel(d.program, "NLM_pack", NULL);
    d.kernel[7] = clCreateKernel(d.program, "NLM_unpack", NULL);

    // Sets kernel arguments.
    ret = clSetKernelArg(d.kernel[0], 0, sizeof(cl_mem), &d.mem_U[0]);
    ret |= clSetKernelArg(d.kernel[0], 1, sizeof(cl_mem), &d.mem_U[3]);
    ret |= clSetKernelArg(d.kernel[0], 2, 2 * sizeof(cl_uint), &d.idmn);
    ret |= clSetKernelArg(d.kernel[1], 0, sizeof(cl_mem), &d.mem_in[2]);
    ret |= clSetKernelArg(d.kernel[1], 1, sizeof(cl_mem), &d.mem_in[d.d ? 3 : 2]);
    ret |= clSetKernelArg(d.kernel[1], 2, sizeof(cl_mem), &d.mem_U[1]);
    ret |= clSetKernelArg(d.kernel[1], 3, 2 * sizeof(cl_uint), &d.idmn);
    ret |= clSetKernelArg(d.kernel[2], 0, sizeof(cl_mem), &d.mem_U[1]);
    ret |= clSetKernelArg(d.kernel[2], 1, sizeof(cl_mem), &d.mem_U[2]);
    ret |= clSetKernelArg(d.kernel[2], 2, 2 * sizeof(cl_uint), &d.idmn);
    ret |= clSetKernelArg(d.kernel[3], 0, sizeof(cl_mem), &d.mem_U[2]);
    ret |= clSetKernelArg(d.kernel[3], 1, sizeof(cl_mem), &d.mem_U[1]);
    ret |= clSetKernelArg(d.kernel[3], 2, 2 * sizeof(cl_uint), &d.idmn);
    ret |= clSetKernelArg(d.kernel[4], 0, sizeof(cl_mem), &d.mem_in[d.d ? 1 : 0]);
    ret |= clSetKernelArg(d.kernel[4], 1, sizeof(cl_mem), &d.mem_U[0]);
    ret |= clSetKernelArg(d.kernel[4], 2, sizeof(cl_mem), &d.mem_U[1]);
    ret |= clSetKernelArg(d.kernel[4], 3, sizeof(cl_mem), &d.mem_U[3]);
    ret |= clSetKernelArg(d.kernel[4], 4, 2 * sizeof(cl_uint), &d.idmn);
    ret |= clSetKernelArg(d.kernel[5], 0, sizeof(cl_mem), &d.mem_in[0]);
    ret |= clSetKernelArg(d.kernel[5], 1, sizeof(cl_mem), &d.mem_out);
    ret |= clSetKernelArg(d.kernel[5], 2, sizeof(cl_mem), &d.mem_U[0]);
    ret |= clSetKernelArg(d.kernel[5], 3, sizeof(cl_mem), &d.mem_U[3]);
    ret |= clSetKernelArg(d.kernel[5], 4, 2 * sizeof(cl_uint), &d.idmn);
    ret |= clSetKernelArg(d.kernel[6], 0, sizeof(cl_mem), &d.mem_P[0]);
    ret |= clSetKernelArg(d.kernel[6], 1, sizeof(cl_mem), &d.mem_P[1]);
    ret |= clSetKernelArg(d.kernel[6], 2, sizeof(cl_mem), &d.mem_P[2]);
    ret |= clSetKernelArg(d.kernel[6], 4, 2 * sizeof(cl_uint), &d.idmn);
    ret |= clSetKernelArg(d.kernel[7], 0, sizeof(cl_mem), &d.mem_P[0]);
    ret |= clSetKernelArg(d.kernel[7], 1, sizeof(cl_mem), &d.mem_P[1]);
    ret |= clSetKernelArg(d.kernel[7], 2, sizeof(cl_mem), &d.mem_P[2]);
    ret |= clSetKernelArg(d.kernel[7], 3, sizeof(cl_mem), &d.mem_out);
    ret |= clSetKernelArg(d.kernel[7], 4, 2 * sizeof(cl_uint), &d.idmn);  
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
extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(
    IScriptEnvironment* env, const AVS_Linkage* const vectors) {

    AVS_linkage = vectors;
    env->AddFunction("KNLMeansCL", "c[d]i[a]i[s]i[cmode]b[wmode]i[h]f[rclip]c[device_type]s[device_id]i[lsb_inout]b\
[info]b",
        AviSynthPluginCreate, 0);
    return "KNLMeansCL for AviSynth";
}
#endif //__AVISYNTH_6_H__

//////////////////////////////////////////
// VapourSynthPluginInit
#ifdef VAPOURSYNTH_H
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc,
    VSRegisterFunction registerFunc, VSPlugin *plugin) {

    configFunc("com.Khanattila.KNLMeansCL", "knlm", "KNLMeansCL for VapourSynth", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("KNLMeansCL", "clip:clip;d:int:opt;a:int:opt;s:int:opt;cmode:int:opt;wmode:int:opt;h:float:opt;\
rclip:clip:opt;device_type:data:opt;device_id:int:opt;info:int:opt", VapourSynthPluginCreate, nullptr, plugin);
}
#endif //__VAPOURSYNTH_H__