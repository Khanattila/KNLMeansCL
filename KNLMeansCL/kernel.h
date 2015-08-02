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

#ifndef __KERNEL_H__
#define __KERNEL_H__

//////////////////////////////////////////
// OpenCL
static const cl_uint H_BLOCK_X = 32, H_BLOCK_Y = 4, V_BLOCK_X = 32, V_BLOCK_Y = 4;
static const char* source_code =
"																												  \n" \
"__kernel																										  \n" \
"void NLM_init(__global void* U2, __global float* M, const int2 dim) {											  \n" \
"																												  \n" \
"	const int x = get_global_id(0);																				  \n" \
"	const int y = get_global_id(1);																				  \n" \
"	if(x >= dim.x || y >= dim.y) return;																		  \n" \
"																												  \n" \
"	const int gidx = mad24(y, dim.x, x);																		  \n" \
"																												  \n" \
"	if(NLMK_RGB) {																								  \n" \
"		__global float4* U2c = (__global float4*) U2;															  \n" \
"		U2c[gidx] = (float4) 0.0f;																				  \n" \
"	} else {																									  \n" \
"		__global float2* U2c = (__global float2*) U2;															  \n" \
"		U2c[gidx] = (float2) 0.0f;																				  \n" \
"	}																											  \n" \
"	M[gidx] = FLT_EPSILON;																						  \n" \
"}																												  \n" \
"																												  \n" \
"__kernel																										  \n" \
"void NLM_dist(__read_only image2d_t U1, __read_only image2d_t U1_pq, __global float* U4, const int2 dim,		  \n" \
"const int2 q) {																								  \n" \
"																												  \n" \
"	const int x = get_global_id(0);																				  \n" \
"	const int y = get_global_id(1);																				  \n" \
"	if(x >= dim.x || y >= dim.y) return;																		  \n" \
"																												  \n" \
"	const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;			  \n" \
"	const int gidx = mad24(y, dim.x, x);																		  \n" \
"																												  \n" \
"	if (NLMK_RGB) {																								  \n" \
"		const float4 u1 = read_imagef(U1, smp, (int2) (x, y));													  \n" \
"		const float4 u1_pq = read_imagef(U1_pq, smp, (int2) (x, y) + q);										  \n" \
"		const float r = native_divide(u1.x + u1_pq.x, 6.0f);													  \n" \
"		const float4 wgh = (float4) (2.0f/3.0f + r, 4.0f/3.0f, 1.0f - r, 0.0f);									  \n" \
"		const float4 tmp = wgh * ((u1 - u1_pq) * (u1 - u1_pq));												      \n" \
"		U4[gidx] = tmp.x + tmp.y + tmp.z + tmp.w;																  \n" \
"	} else {																									  \n" \
"		const float u1 = read_imagef(U1, smp, (int2) (x, y)).x;													  \n" \
"		const float u1_pq = read_imagef(U1_pq, smp, (int2) (x, y) + q).x;										  \n" \
"		U4[gidx] = (u1 - u1_pq) * (u1 - u1_pq);																	  \n" \
"	}																											  \n" \
"}																												  \n" \
"																												  \n" \
"__kernel __attribute__((reqd_work_group_size(H_BLOCK_X, H_BLOCK_Y, 1)))										  \n" \
"void NLM_horiz(__global float* U4_in, __global float* U4_out,	const int2 dim) {								  \n" \
"																												  \n" \
"	__local float buffer[H_BLOCK_Y][3*H_BLOCK_X];																  \n" \
"	const int x = get_global_id(0);																				  \n" \
"	const int y = get_global_id(1);																				  \n" \
"	const int lx = get_local_id(0);																				  \n" \
"	const int ly = get_local_id(1);																				  \n" \
"	const int mdata = mad24(clamp(y, 0, dim.y - 1), dim.x, clamp(x,             0, dim.x - 1));					  \n" \
"	const int lhalo = mad24(clamp(y, 0, dim.y - 1), dim.x, clamp(x - H_BLOCK_X, 0, dim.x - 1));					  \n" \
"	const int rhalo = mad24(clamp(y, 0, dim.y - 1), dim.x, clamp(x + H_BLOCK_X, 0, dim.x - 1));					  \n" \
"																												  \n" \
"	buffer[ly][lx + H_BLOCK_X]	 = U4_in[mdata];																  \n" \
"	buffer[ly][lx]		         = U4_in[lhalo];																  \n" \
"	buffer[ly][lx + 2*H_BLOCK_X] = U4_in[rhalo];																  \n" \
"	barrier(CLK_LOCAL_MEM_FENCE);																				  \n" \
"																												  \n" \
"	if(x >= dim.x || y >= dim.y) return;																		  \n" \
"	float sum = 0.0f;																							  \n" \
"	for(int i = -NLMK_S; i <= NLMK_S; i++)																		  \n" \
"		sum += buffer[ly][lx + H_BLOCK_X + i];																	  \n" \
"	U4_out[mdata] = sum;																						  \n" \
"}																												  \n" \
"																												  \n" \
"__kernel __attribute__((reqd_work_group_size(V_BLOCK_X, V_BLOCK_Y, 1)))										  \n" \
"void NLM_vert(__global float* U4_in, __global float* U4_out, const int2 dim) {									  \n" \
"																												  \n" \
"	__local float buffer[3*V_BLOCK_Y][V_BLOCK_X];																  \n" \
"	const int x = get_global_id(0);																				  \n" \
"	const int y = get_global_id(1);																				  \n" \
"	const int lx = get_local_id(0);																				  \n" \
"	const int ly = get_local_id(1);																				  \n" \
"	const int mdata = mad24(clamp(y,             0, dim.y - 1), dim.x, clamp(x, 0, dim.x - 1));					  \n" \
"	const int uhalo = mad24(clamp(y - V_BLOCK_Y, 0, dim.y - 1), dim.x, clamp(x, 0, dim.x - 1));					  \n" \
"	const int lhalo = mad24(clamp(y + V_BLOCK_Y, 0, dim.y - 1), dim.x, clamp(x, 0, dim.x - 1));					  \n" \
"																												  \n" \
"	buffer[ly + V_BLOCK_Y][lx]	 = U4_in[mdata];																  \n" \
"	buffer[ly][lx]		         = U4_in[uhalo];																  \n" \
"	buffer[ly + 2*V_BLOCK_Y][lx] = U4_in[lhalo];																  \n" \
"	barrier(CLK_LOCAL_MEM_FENCE);																				  \n" \
"																												  \n" \
"	if(x >= dim.x || y >= dim.y) return;																		  \n" \
"	float sum = 0.0f;																							  \n" \
"	for(int j = -NLMK_S; j <= NLMK_S; j++)																		  \n" \
"		sum += buffer[ly + V_BLOCK_Y + j][lx];																	  \n" \
"	U4_out[mdata] =																								  \n" \
"		(NLMK_WMODE == 2) ? fmax(1.0f -sum * NLMK_H2_INV_NORM, 0.0f) * fmax(1.0f -sum * NLMK_H2_INV_NORM, 0.0f) : \n" \
"		(NLMK_WMODE == 1) ? native_exp(-sum * NLMK_H2_INV_NORM) :												  \n" \
"		native_recip(1.0f + sum * NLMK_H2_INV_NORM);															  \n" \
"}																												  \n" \
"																												  \n" \
"__kernel																										  \n" \
"void NLM_accu(__read_only image2d_t U1, __global void* U2, __global float* U4, __global float* M,				  \n" \
"const int2 dim, const int2 q) {																				  \n" \
"																												  \n" \
"	const int x = get_global_id(0);																				  \n" \
"	const int y = get_global_id(1);																				  \n" \
"	if(x >= dim.x || y >= dim.y) return;																		  \n" \
"																												  \n" \
"	const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;			  \n" \
"	const int gidx = mad24(y, dim.x, x);																		  \n" \
"	const int qidx = mad24(clamp(y - q.y, 0, dim.y - 1), dim.x, clamp(x - q.x, 0, dim.x - 1));					  \n" \
"	const float u4 = U4[gidx];																					  \n" \
"	const float u4_mq = U4[qidx];																				  \n" \
"	M[gidx] = fmax(M[gidx], fmax(u4, u4_mq));																	  \n" \
"																												  \n" \
"	if (NLMK_RGB) {																								  \n" \
"		__global float4* U2c = (__global float4*) U2;															  \n" \
"		const float4 u1_pq = read_imagef(U1, smp, (int2) (x, y) + q);											  \n" \
"		const float4 u1_mq = read_imagef(U1, smp, (int2) (x, y) - q);											  \n" \
"		float4 accu = NLMK_TEMPORAL ? (u4 * u1_pq) : (u4 * u1_pq) + (u4_mq * u1_mq);							  \n" \
"		accu.w = NLMK_TEMPORAL ? (u4) : (u4 + u4_mq);															  \n" \
"		U2c[gidx] += accu;																						  \n" \
"	} else {																									  \n" \
"		__global float2* U2c = (__global float2*) U2;															  \n" \
"		const float u1_pq = read_imagef(U1, smp, (int2) (x, y) + q).x;											  \n" \
"		const float u1_mq = read_imagef(U1, smp, (int2) (x, y) - q).x;											  \n" \
"		U2c[gidx].x += NLMK_TEMPORAL ? (u4 * u1_pq) : (u4 * u1_pq) + (u4_mq * u1_mq);							  \n" \
"		U2c[gidx].y += NLMK_TEMPORAL ? (u4) : (u4 + u4_mq);														  \n" \
"	}																											  \n" \
"}																												  \n" \
"																												  \n" \
"__kernel																										  \n" \
"void NLM_finish(__read_only image2d_t U1_in, __write_only image2d_t U1_out, __global void* U2,					  \n" \
"__global float* M, const int2 dim) {																			  \n" \
"																												  \n" \
"	const int x = get_global_id(0);																				  \n" \
"	const int y = get_global_id(1);																				  \n" \
"	if(x >= dim.x || y >= dim.y) return;																		  \n" \
"																												  \n" \
"	const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;			  \n" \
"	const int gidx = mad24(y, dim.x, x);																		  \n" \
"																												  \n" \
"	if (NLMK_RGB) {																								  \n" \
"		__global float4* U2c = (__global float4*) U2;															  \n" \
"		const float4 u1 = read_imagef(U1_in, smp, (int2) (x, y));												  \n" \
"		const float4 num = mad((float4) M[gidx], u1, U2c[gidx]);												  \n" \
"		const float4 den = (float4) (U2c[gidx].w + M[gidx]);													  \n" \
"		float4 val = native_divide(num, den); val.w = u1.w;														  \n" \
"		write_imagef(U1_out, (int2) (x, y), val);																  \n" \
"	} else {																									  \n" \
"		__global float2* U2c = (__global float2*) U2;															  \n" \
"		const float u1 = read_imagef(U1_in, smp, (int2) (x, y)).x;												  \n" \
"		const float num = mad(M[gidx], u1, U2c[gidx].x);														  \n" \
"		const float den = U2c[gidx].y + M[gidx];																  \n" \
"		const float val = native_divide(num, den);																  \n" \
"		write_imagef(U1_out, (int2) (x, y), (float4) (val, val, val, 1.0f));									  \n" \
"	}																											  \n" \
"}																												  ";

#endif //__KERNEL_H__
