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

#include "ocl_utils.h"
#include <fstream>
#include <cstring>

#define STR(code) case code: return #code

//////////////////////////////////////////
// Functions
const char* oclUtilsErrorToString(cl_int err) {
    switch (err) {
        STR(OCL_UTILS_INVALID_VALUE);
        STR(OCL_UTILS_INVALID_DEVICE_TYPE);
        STR(OCL_UTILS_NO_DEVICE_AVAILABLE);
        STR(OCL_UTILS_MALLOC_ERROR);
        STR(CL_SUCCESS);
        STR(CL_DEVICE_NOT_FOUND);
        STR(CL_DEVICE_NOT_AVAILABLE);
        STR(CL_COMPILER_NOT_AVAILABLE);
        STR(CL_MEM_OBJECT_ALLOCATION_FAILURE);
        STR(CL_OUT_OF_RESOURCES);
        STR(CL_OUT_OF_HOST_MEMORY);
        STR(CL_PROFILING_INFO_NOT_AVAILABLE);
        STR(CL_MEM_COPY_OVERLAP);
        STR(CL_IMAGE_FORMAT_MISMATCH);
        STR(CL_IMAGE_FORMAT_NOT_SUPPORTED);
        STR(CL_BUILD_PROGRAM_FAILURE);
        STR(CL_MAP_FAILURE);
        STR(CL_MISALIGNED_SUB_BUFFER_OFFSET);
        STR(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST);
        STR(CL_COMPILE_PROGRAM_FAILURE);
        STR(CL_LINKER_NOT_AVAILABLE);
        STR(CL_LINK_PROGRAM_FAILURE);
        STR(CL_DEVICE_PARTITION_FAILED);
        STR(CL_KERNEL_ARG_INFO_NOT_AVAILABLE);
        STR(CL_INVALID_VALUE);
        STR(CL_INVALID_DEVICE_TYPE);
        STR(CL_INVALID_PLATFORM);
        STR(CL_INVALID_DEVICE);
        STR(CL_INVALID_CONTEXT);
        STR(CL_INVALID_QUEUE_PROPERTIES);
        STR(CL_INVALID_COMMAND_QUEUE);
        STR(CL_INVALID_HOST_PTR);
        STR(CL_INVALID_MEM_OBJECT);
        STR(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR);
        STR(CL_INVALID_IMAGE_SIZE);
        STR(CL_INVALID_SAMPLER);
        STR(CL_INVALID_BINARY);
        STR(CL_INVALID_BUILD_OPTIONS);
        STR(CL_INVALID_PROGRAM);
        STR(CL_INVALID_PROGRAM_EXECUTABLE);
        STR(CL_INVALID_KERNEL_NAME);
        STR(CL_INVALID_KERNEL_DEFINITION);
        STR(CL_INVALID_KERNEL);
        STR(CL_INVALID_ARG_INDEX);
        STR(CL_INVALID_ARG_VALUE);
        STR(CL_INVALID_ARG_SIZE);
        STR(CL_INVALID_KERNEL_ARGS);
        STR(CL_INVALID_WORK_DIMENSION);
        STR(CL_INVALID_WORK_GROUP_SIZE);
        STR(CL_INVALID_WORK_ITEM_SIZE);
        STR(CL_INVALID_GLOBAL_OFFSET);
        STR(CL_INVALID_EVENT_WAIT_LIST);
        STR(CL_INVALID_EVENT);
        STR(CL_INVALID_OPERATION);
        STR(CL_INVALID_GL_OBJECT);
        STR(CL_INVALID_BUFFER_SIZE);
        STR(CL_INVALID_MIP_LEVEL);
        STR(CL_INVALID_GLOBAL_WORK_SIZE);
        STR(CL_INVALID_PROPERTY);
        STR(CL_INVALID_IMAGE_DESCRIPTOR);
        STR(CL_INVALID_COMPILER_OPTIONS);
        STR(CL_INVALID_LINKER_OPTIONS);
        STR(CL_INVALID_DEVICE_PARTITION_COUNT);
        default: return "OCL_UTILS_UNKNOWN_ERROR";
    }
}

cl_int oclUtilsCheckPlatform(cl_platform_id platofrm, bool *compatible) {

    /* Platform: Full Profile */
    char plt_profile_txt[64];
    plt_profile_txt[0] = '\0';
    cl_int ret = clGetPlatformInfo(platofrm, CL_PLATFORM_PROFILE, sizeof(char) * 64, plt_profile_txt, NULL);
    if (ret != CL_SUCCESS || !strstr(plt_profile_txt, "FULL_PROFILE")) {
        *compatible = false;
        return ret;
    }

    /* Platform: OpenCL 1.2 */
    char plt_version_txt[64];
    ret = clGetPlatformInfo(platofrm, CL_PLATFORM_VERSION, sizeof(char) * 64, plt_version_txt, NULL);
    if (ret != CL_SUCCESS || !(10 * (plt_version_txt[7] - '0') + (plt_version_txt[9] - '0') >= OCL_UTILS_OPENCL_1_2)) {
        *compatible = false;
        return ret;
    } else {
        *compatible = true;
        return ret;
    }
}

cl_int oclUtilsCheckDevice(cl_device_id device, bool *compatible) {

    /* Device: Full Profile */
    char dvc_profile_txt[64];
    dvc_profile_txt[0] = '\0';
    cl_int ret = clGetDeviceInfo(device, CL_DEVICE_PROFILE, sizeof(char) * 64, dvc_profile_txt, NULL);
    if (ret != CL_SUCCESS || !strstr(dvc_profile_txt, "FULL_PROFILE")) {
        *compatible = false;
        return ret;
    }

    /* Device: OpenCL 1.2 */
    char dvc_version_txt[64];
    ret = clGetDeviceInfo(device, CL_DEVICE_VERSION, sizeof(char) * 64, dvc_version_txt, NULL);
    if (ret != CL_SUCCESS || !(10 * (dvc_version_txt[7] - '0') + (dvc_version_txt[9] - '0') >= OCL_UTILS_OPENCL_1_2)) {
        *compatible = false;
        return ret;
    }

    /* Device: Image Support */
    cl_bool dev_image_support;
    ret = clGetDeviceInfo(device, CL_DEVICE_IMAGE_SUPPORT, sizeof(cl_bool), &dev_image_support, NULL);
    if (ret != CL_SUCCESS || dev_image_support == CL_FALSE) {
        *compatible = false;
        return ret;
    } else {
        *compatible = true;
        return ret;
    }

}

cl_int oclUtilsGetIDs(cl_device_type device_type, cl_uint shf_device, cl_platform_id *platform, cl_device_id *device) {

    /* Get OpenCL platforms */
    cl_uint num_platforms, shf_index = 0;
    cl_int ret = clGetPlatformIDs(0, NULL, &num_platforms);
    if (ret != CL_SUCCESS) return ret;
    else if (num_platforms == 0) return CL_OUT_OF_HOST_MEMORY;
    cl_platform_id *platformIDs = (cl_platform_id*) malloc(sizeof(cl_platform_id) * num_platforms);
    if (platformIDs == NULL) return OCL_UTILS_MALLOC_ERROR;
    ret = clGetPlatformIDs(num_platforms, platformIDs, NULL);
    if (ret != CL_SUCCESS) return ret;
    else if (num_platforms == 0) return CL_OUT_OF_HOST_MEMORY;

    /* Iterate over platformIDs */
    for (cl_uint i = 0; i < num_platforms; i++) {
        bool plt_found;
        ret = oclUtilsCheckPlatform(platformIDs[i], &plt_found);
        if (ret != CL_SUCCESS) return ret;
        if (plt_found) {

            /* Get OpenCl devies */
            cl_uint num_devices;
            ret = clGetDeviceIDs(platformIDs[i], device_type, 0, NULL, &num_devices);
            if (ret == CL_SUCCESS && num_devices > 0) {
                cl_device_id *deviceIDs = (cl_device_id*) malloc(sizeof(cl_device_id) * num_devices);
                if (deviceIDs == NULL) return OCL_UTILS_MALLOC_ERROR;
                ret = clGetDeviceIDs(platformIDs[i], device_type, num_devices, deviceIDs, NULL);
                if (ret == CL_SUCCESS && num_devices > 0) {

                    /* Iterate over deviceIDs */
                    for (cl_uint k = 0; k < num_devices; k++) {
                        bool dvc_found;
                        ret = oclUtilsCheckDevice(deviceIDs[k], &dvc_found);
                        if (ret != CL_SUCCESS) {
                            free(deviceIDs);
                            free(platformIDs);
                            return ret;
                        }
                        if (dvc_found) {
                            if (shf_index == shf_device) {
                                *platform = platformIDs[i];
                                *device = deviceIDs[k];
                                free(deviceIDs);
                                free(platformIDs);
                                return ret;
                            } else if (shf_index < shf_device) shf_index++;
                        }
                    }
                } else if (ret != CL_DEVICE_NOT_FOUND) {
                    free(platformIDs);
                    free(deviceIDs);
                    return ret;
                }
            } else if (ret != CL_DEVICE_NOT_FOUND) {
                free(platformIDs);
                return ret;
            }

        }
    }
    free(platformIDs);
    return OCL_UTILS_NO_DEVICE_AVAILABLE;

}

cl_int oclUtilsGetPlaformDeviceIDs(cl_uint device_type, cl_uint shf_device, cl_platform_id *platform, cl_device_id *device) {

    if (platform == NULL || device == NULL) {
        return OCL_UTILS_INVALID_VALUE;
    } else switch (device_type) {
        case OCL_UTILS_DEVICE_TYPE_CPU:
            return oclUtilsGetIDs(CL_DEVICE_TYPE_CPU, shf_device, platform, device);
        case OCL_UTILS_DEVICE_TYPE_GPU:
            return oclUtilsGetIDs(CL_DEVICE_TYPE_GPU, shf_device, platform, device);
        case OCL_UTILS_DEVICE_TYPE_ACCELERATOR:
            return oclUtilsGetIDs(CL_DEVICE_TYPE_ACCELERATOR, shf_device, platform, device);
        case OCL_UTILS_DEVICE_TYPE_AUTO: {
            cl_int ret = oclUtilsGetIDs(CL_DEVICE_TYPE_ACCELERATOR, shf_device, platform, device);
            if (ret == OCL_UTILS_NO_DEVICE_AVAILABLE) {
                ret = oclUtilsGetIDs(CL_DEVICE_TYPE_GPU, shf_device, platform, device);
                if (ret == OCL_UTILS_NO_DEVICE_AVAILABLE) {
                    return oclUtilsGetIDs(CL_DEVICE_TYPE_CPU, shf_device, platform, device);
                } return ret;
            } return ret;
        }
        default:
            return OCL_UTILS_INVALID_DEVICE_TYPE;
    }
}

void oclUtilsDebugInfo(cl_platform_id platform, cl_device_id device, cl_program program, cl_int errcode) {
    cl_int ret = CL_SUCCESS;
    std::ofstream outfile("Log-KNLMeansCL.txt", std::ofstream::out);

    // clGetPlatformInfo
    outfile << " OpenCL Platform" << std::endl;
    outfile << "------------------------------------------------------------" << std::endl;
    size_t plt_vendor, plt_name, plt_version, plt_profile;
    ret |= clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, 0, NULL, &plt_vendor);
    ret |= clGetPlatformInfo(platform, CL_PLATFORM_NAME, 0, NULL, &plt_name);
    ret |= clGetPlatformInfo(platform, CL_PLATFORM_VERSION, 0, NULL, &plt_version);
    ret |= clGetPlatformInfo(platform, CL_PLATFORM_PROFILE, 0, NULL, &plt_profile);
    char *plt_vendor_txt = (char*) malloc(plt_vendor);
    char *plt_name_txt = (char*) malloc(plt_name);
    char *plt_version_txt = (char*) malloc(plt_version);
    char *plt_profile_txt = (char*) malloc(plt_profile);
    ret |= clGetPlatformInfo(platform, CL_PLATFORM_VENDOR, plt_vendor, plt_vendor_txt, NULL);
    ret |= clGetPlatformInfo(platform, CL_PLATFORM_NAME, plt_name, plt_name_txt, NULL);
    ret |= clGetPlatformInfo(platform, CL_PLATFORM_VERSION, plt_version, plt_version_txt, NULL);
    ret |= clGetPlatformInfo(platform, CL_PLATFORM_PROFILE, plt_profile, plt_profile_txt, NULL);
    outfile << " CL_PLATFORM_VENDOR:                " << plt_vendor_txt << std::endl;
    outfile << " CL_PLATFORM_NAME:                  " << plt_name_txt << std::endl;
    outfile << " CL_PLATFORM_VERSION:               " << plt_version_txt << std::endl;
    outfile << " CL_PLATFORM_PROFILE:               " << plt_profile_txt << std::endl << std::endl;
    free(plt_vendor_txt);
    free(plt_name_txt);
    free(plt_version_txt);
    free(plt_profile_txt);

    // clGetDeviceInfo
    outfile << " OpenCL Device" << std::endl;
    outfile << "------------------------------------------------------------" << std::endl;
    size_t dvc_vendor, dvc_name, drv_version, dvc_version, dvc_profile;
    ret |= clGetDeviceInfo(device, CL_DEVICE_VENDOR, 0, NULL, &dvc_vendor);
    ret |= clGetDeviceInfo(device, CL_DEVICE_NAME, 0, NULL, &dvc_name);
    ret |= clGetDeviceInfo(device, CL_DRIVER_VERSION, 0, NULL, &drv_version);
    ret |= clGetDeviceInfo(device, CL_DEVICE_VERSION, 0, NULL, &dvc_version);
    ret |= clGetDeviceInfo(device, CL_DEVICE_PROFILE, 0, NULL, &dvc_profile);
    char *dvc_vendor_txt = (char*) malloc(dvc_vendor);
    char *dvc_name_txt = (char*) malloc(dvc_name);
    char *drv_version_txt = (char*) malloc(drv_version);
    char *dvc_version_txt = (char*) malloc(dvc_version);
    char *dvc_profile_txt = (char*) malloc(dvc_profile);
    cl_bool img_support;
    size_t max_width, max_height, arr_size;
    ret |= clGetDeviceInfo(device, CL_DEVICE_VENDOR, dvc_vendor, dvc_vendor_txt, NULL);
    ret |= clGetDeviceInfo(device, CL_DEVICE_NAME, dvc_name, dvc_name_txt, NULL);
    ret |= clGetDeviceInfo(device, CL_DRIVER_VERSION, drv_version, drv_version_txt, NULL);
    ret |= clGetDeviceInfo(device, CL_DEVICE_VERSION, dvc_version, dvc_version_txt, NULL);
    ret |= clGetDeviceInfo(device, CL_DEVICE_PROFILE, dvc_profile, dvc_profile_txt, NULL);
    ret |= clGetDeviceInfo(device, CL_DEVICE_IMAGE_SUPPORT, sizeof(cl_bool), &img_support, NULL);
    ret |= clGetDeviceInfo(device, CL_DEVICE_IMAGE2D_MAX_WIDTH, sizeof(size_t), &max_width, NULL);
    ret |= clGetDeviceInfo(device, CL_DEVICE_IMAGE2D_MAX_HEIGHT, sizeof(size_t), &max_height, NULL);
    ret |= clGetDeviceInfo(device, CL_DEVICE_IMAGE_MAX_ARRAY_SIZE, sizeof(size_t), &arr_size, NULL);
    outfile << " CL_DEVICE_VENDOR:                  " << dvc_vendor_txt << std::endl;
    outfile << " CL_DEVICE_NAME:                    " << dvc_name_txt << std::endl;
    outfile << " CL_DRIVER_VERSION:                 " << drv_version_txt << std::endl;
    outfile << " CL_DEVICE_VERSION:                 " << dvc_version_txt << std::endl;
    outfile << " CL_DEVICE_PROFILE:                 " << dvc_profile_txt << std::endl;
    outfile << " CL_DEVICE_IMAGE_SUPPORT:           " << img_support << std::endl;
    outfile << " CL_DEVICE_IMAGE2D_MAX_WIDTH:       " << max_width << std::endl;
    outfile << " CL_DEVICE_IMAGE2D_MAX_HEIGHT:      " << max_height << std::endl;
    outfile << " CL_DEVICE_IMAGE_MAX_ARRAY_SIZE:    " << arr_size << std::endl << std::endl;
    free(dvc_vendor_txt);
    free(dvc_name_txt);
    free(drv_version_txt);
    free(dvc_version_txt);
    free(dvc_profile_txt);

    // clGetProgramBuildInfo
    outfile << " Program Build" << std::endl;
    outfile << "------------------------------------------------------------" << std::endl;
    size_t bld_options, bld_log;
    ret |= clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_OPTIONS, 0, NULL, &bld_options);
    ret |= clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &bld_log);
    char *bld_options_txt = (char*) malloc(bld_options);
    char *bld_log_txt = (char*) malloc(bld_log);
    ret |= clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_OPTIONS, bld_options, bld_options_txt, NULL);
    ret |= clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, bld_log, bld_log_txt, NULL);
    std::string str_options = bld_options_txt;
    std::string search = " -";
    std::string format = "\n                                    -";
    size_t pos = 0;
    while ((pos = str_options.find(search, pos)) != std::string::npos) {
        str_options.replace(pos, search.length(), format);
        pos += format.length();
    }
    outfile << " CL_PROGRAM_BUILD_ERROR:            " << oclUtilsErrorToString(errcode) << std::endl;
    outfile << " CL_PROGRAM_BUILD_OPTIONS:          " << str_options.c_str() << std::endl;
    outfile << " CL_PROGRAM_BUILD_LOG:              " << std::endl << bld_log_txt << std::endl << std::endl;
    free(bld_options_txt);
    free(bld_log_txt);

    // Close
    outfile << " RETURN:                            " << ret << std::endl;
    outfile.close();
}