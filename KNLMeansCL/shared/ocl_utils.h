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

#ifndef __OCL_UTILS_H
#define __OCL_UTILS_H

#include <cstring>

#ifdef __APPLE__
#    include <OpenCL/cl.h>
#else
#    include <CL/cl.h>
#endif

//////////////////////////////////////////
// Type Definition
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

//////////////////////////////////////////
// Functions
const char* oclUtilsErrorToString(cl_int err);
cl_int oclUtilsCheckPlatform(cl_platform_id platofrm, bool *compatible);
cl_int oclUtilsCheckDevice(cl_device_id device, bool *compatible);
cl_int oclUtilsGetIDs(cl_device_type device_type, cl_uint shf_device, cl_platform_id *platform, cl_device_id *device);
cl_int oclUtilsGetPlaformDeviceIDs(cl_uint device_type, cl_uint shf_device, cl_platform_id *platform, cl_device_id *device);
void oclUtilsDebugInfo(cl_platform_id platform, cl_device_id device, cl_program program, cl_int errcode);

#endif //__OCLUTILS_H__
