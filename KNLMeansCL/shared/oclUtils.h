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

//////////////////////////////////////////
// Definition
typedef cl_bitfield                          ocl_utils_device_type;

#define OCL_UTILS_DEVICE_TYPE_CPU            (1 << 0)
#define OCL_UTILS_DEVICE_TYPE_GPU            (1 << 1)
#define OCL_UTILS_DEVICE_TYPE_ACCELERATOR    (1 << 2)
#define OCL_UTILS_DEVICE_TYPE_AUTO           (1 << 3)

#define OCL_UTILS_MALLOC_ERROR                1
#define OCL_UTILS_NO_DEVICE_AVAILABLE         2
#define OCL_UTILS_INVALID_DEVICE_TYPE         3
#define OCL_UTILS_INVALID_VALUE               4

#define OCL_UTILS_OPENCL_1_0                  10
#define OCL_UTILS_OPENCL_1_1                  11
#define OCL_UTILS_OPENCL_1_2                  12
#define OCL_UTILS_STRING_SIZE                 512


//////////////////////////////////////////
// Common
inline size_t mrounds(const size_t num, const size_t mul) {
    return (size_t) ceil((double) num / (double) mul) * mul;
}

template <typename T>T inline fastmax(const T& left, const T& right) {
    return left > right ? left : right;
}

template <typename T>T inline clamp(const T& value, const T& low, const T& high) {
    return value < low ? low : (value > high ? high : value);
}

//////////////////////////////////////////
// Functions
#define STR(code) case code: return #code
const char* oclUtilsErrorToString(cl_int err) {
    switch (err) {
        STR(OCL_UTILS_INVALID_VALUE);
        STR(OCL_UTILS_INVALID_DEVICE_TYPE);
        STR(OCL_UTILS_NO_DEVICE_AVAILABLE);
        STR(OCL_UTILS_MALLOC_ERROR);
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

cl_int oclUtilsGetIDs(cl_uint ver_opencl, cl_device_type device_type, cl_uint shf_device, cl_platform_id *platform, cl_device_id *device) {
    
    cl_uint num_platforms;
    cl_int ret = clGetPlatformIDs(0, NULL, &num_platforms);
    if (ret != CL_SUCCESS) return ret;
    else if (num_platforms == 0) return CL_INVALID_VALUE;
    cl_platform_id *platforms = (cl_platform_id*) malloc(sizeof(cl_platform_id) * num_platforms);
    if (platforms == NULL) return OCL_UTILS_MALLOC_ERROR;
    ret = clGetPlatformIDs(num_platforms, platforms, NULL);
    if (ret != CL_SUCCESS) return ret;
    cl_uint index = 0;
    for (cl_uint p = 0; p < num_platforms; p++) {
        char str[OCL_UTILS_STRING_SIZE];
        ret = clGetPlatformInfo(platforms[p], CL_PLATFORM_VERSION, sizeof(char) * OCL_UTILS_STRING_SIZE, str, NULL);
        if (ret != CL_SUCCESS) return ret;
        cl_uint ver_platform = 10u * (str[7] - '0') + (str[9] - '0');
        if (ver_platform >= ver_opencl) {
            cl_uint num_devices;
            ret = clGetDeviceIDs(platforms[p], device_type, 0, NULL, &num_devices);
            if (ret != CL_SUCCESS) return ret;
            else if (num_devices > 0) {
                cl_device_id *devices = (cl_device_id*) malloc(sizeof(cl_device_id) * num_devices);
                if (devices == NULL) return OCL_UTILS_MALLOC_ERROR;
                ret = clGetDeviceIDs(platforms[p], device_type, num_devices, devices, NULL);
                if (ret != CL_SUCCESS) return ret;
                for (cl_uint d = 0; d < num_devices; d++) {
                    if (index == shf_device) {
                        *platform = platforms[p];
                        *device = devices[d];
                        free(platforms);
                        free(devices);
                        return CL_SUCCESS;
                    } else {
                        index++;
                    }
                }
            }      
        }
    }
    return OCL_UTILS_NO_DEVICE_AVAILABLE;
}

cl_int oclUtilsGetPlaformDeviceIDs(cl_uint ver_opencl, ocl_utils_device_type device_type, cl_uint shf_device, cl_platform_id *platform,
    cl_device_id *device, cl_device_type *type) {

    if (platform == NULL || device == NULL) {
        return OCL_UTILS_INVALID_VALUE;
    } else switch (device_type) {
        case OCL_UTILS_DEVICE_TYPE_CPU:
            *type = CL_DEVICE_TYPE_CPU;
            return oclUtilsGetIDs(ver_opencl, CL_DEVICE_TYPE_CPU, shf_device, platform, device);
        case OCL_UTILS_DEVICE_TYPE_GPU:
            *type = OCL_UTILS_DEVICE_TYPE_GPU;
            return oclUtilsGetIDs(ver_opencl, CL_DEVICE_TYPE_GPU, shf_device, platform, device);
        case OCL_UTILS_DEVICE_TYPE_ACCELERATOR:
            *type = OCL_UTILS_DEVICE_TYPE_ACCELERATOR;
            return oclUtilsGetIDs(ver_opencl, CL_DEVICE_TYPE_ACCELERATOR, shf_device, platform, device);
        case OCL_UTILS_DEVICE_TYPE_AUTO: {
            *type = OCL_UTILS_DEVICE_TYPE_ACCELERATOR;
            cl_int ret = oclUtilsGetIDs(ver_opencl, CL_DEVICE_TYPE_ACCELERATOR, shf_device, platform, device);
            if (ret == OCL_UTILS_NO_DEVICE_AVAILABLE) {
                *type = OCL_UTILS_DEVICE_TYPE_GPU;
                ret = oclUtilsGetIDs(ver_opencl, CL_DEVICE_TYPE_GPU, shf_device, platform, device);
                if (ret == OCL_UTILS_NO_DEVICE_AVAILABLE) {
                    *type = CL_DEVICE_TYPE_CPU;
                    return oclUtilsGetIDs(ver_opencl, CL_DEVICE_TYPE_CPU, shf_device, platform, device);
                } else if (ret != CL_SUCCESS) break;
            } else if (ret != CL_SUCCESS) break;
        }
        default:
            return OCL_UTILS_INVALID_DEVICE_TYPE;
    }
}