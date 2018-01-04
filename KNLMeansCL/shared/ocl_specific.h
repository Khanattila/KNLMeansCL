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

#ifndef __OCL_SPECIFIC_H
#define __OCL_SPECIFIC_H

#ifdef __APPLE__
#    include <OpenCL/cl.h>
#else
#    include <CL/cl.h>
#endif

//////////////////////////////////////////
// Common
size_t mrounds(const size_t number, const size_t multiple);
int min(const int a, const int b);

//////////////////////////////////////////
// Kernel specific
const char* oclUtilsNlmClipTypeToString(cl_uint clip);
const char* oclUtilsNlmClipRefToString(cl_uint clip);
const char* oclUtilsNlmWmodeToString(cl_int wmode);

#endif //__OCL_SPECIFIC_H__