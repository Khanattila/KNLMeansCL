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

#ifndef __PEAK_BENCHMARK_H__
#define __PEAK_BENCHMARK_H__

typedef struct _ocl_device_packed {
    cl_platform_id platform;
    cl_device_id device;
} ocl_device_packed;

//////////////////////////////////////////
// oclGetDevicesList
cl_int oclGetDevicesList(cl_device_type device_type, ocl_device_packed* devices, cl_uint* num_devices) {
    if (devices == NULL && num_devices != NULL) {
        cl_uint tmp_platforms = 0, tmp_devices = 0, prt_devices;
        cl_int ret = clGetPlatformIDs(0, NULL, &tmp_platforms);
        if (ret != CL_SUCCESS) return ret;
        cl_platform_id *temp_platforms = (cl_platform_id*) malloc(sizeof(cl_platform_id) * tmp_platforms);
        ret = clGetPlatformIDs(tmp_platforms, temp_platforms, NULL);
        if (ret != CL_SUCCESS) return ret;
        for (cl_uint i = 0; i < tmp_platforms; i++) {
            prt_devices = 0;
            ret = clGetDeviceIDs(temp_platforms[i], device_type, 0, 0, &prt_devices);
            if (ret != CL_SUCCESS && ret != CL_DEVICE_NOT_FOUND) return ret;
            tmp_devices += prt_devices;
        }
        free(temp_platforms);
        *num_devices = tmp_devices;
        return CL_SUCCESS;
    } else if (devices != NULL && num_devices == NULL) {
        cl_uint tmp_platforms = 0, tmp_devices = 0, index = 0, prt_devices, num_devices_platform;
        cl_device_id tmp_device = 0;
        cl_int ret = clGetPlatformIDs(0, NULL, &tmp_platforms);
        if (ret != CL_SUCCESS) return ret;
        cl_platform_id *temp_platforms = (cl_platform_id*) malloc(sizeof(cl_platform_id) * tmp_platforms);
        ret = clGetPlatformIDs(tmp_platforms, temp_platforms, NULL);
        if (ret != CL_SUCCESS) return ret;
        for (cl_uint i = 0; i < tmp_platforms; i++) {
            prt_devices = 0;
            ret = clGetDeviceIDs(temp_platforms[i], device_type, 0, 0, &prt_devices);
            if (ret != CL_SUCCESS && ret != CL_DEVICE_NOT_FOUND) return ret;
            tmp_devices += prt_devices;
        }
        for (cl_uint i = 0; i < tmp_platforms; i++) {
            num_devices_platform = 0;
            ret = clGetDeviceIDs(temp_platforms[i], device_type, 0, 0, &num_devices_platform);
            if (ret != CL_SUCCESS && ret != CL_DEVICE_NOT_FOUND) return ret;
            for (cl_uint j = 0; j < num_devices_platform; j++) {
                tmp_device = 0;              
                ret = clGetDeviceIDs(temp_platforms[i], device_type, 1, &tmp_device, NULL);
                if (ret != CL_SUCCESS && ret != CL_DEVICE_NOT_FOUND) return ret;                
                devices[index++] = { temp_platforms[i], tmp_device };
            }
        }
        return CL_SUCCESS;
    } else {
        return CL_INVALID_VALUE;
    }
}

#endif //__PEAK_BENCHMARK_H__
