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
*
*    To speed up processing I use an algorithm proposed by B. Goossens,
*    H.Q. Luong, J. Aelterman, A. Pizurica,  and W. Philips, "A GPU-Accelerated
*    Real-Time NLMeans Algorithm for Denoising Color Video Sequences",
*    in Proc. ACIVS (2), 2010, pp.46-57.
*/

#ifndef __NLM_KERNEL_H
#define __NLM_KERNEL_H

#define CL_USE_DEPRECATED_OPENCL_2_0_APIS

#ifdef __APPLE__
#    include <OpenCL/cl.h>
#else
#    include <CL/cl.h>
#endif

//////////////////////////////////////////
// Type Definition
#define nlmDistance                0x0
#define nlmHorizontal              0x1
#define nlmVertical                0x2
#define nlmAccumulation            0x3
#define nlmFinish                  0x4
#define nlmPack                    0x5
#define nlmUnpack                  0x6
#define NLM_KERNEL                 0x7

#define memU1a                     0x0
#define memU1b                     0x1
#define memU1z                     0x2
#define memU2                      0x3
#define memU4a                     0x4
#define memU4b                     0x5
#define memU5                      0x6
#define NLM_MEMORY                 0x7

#define NLM_CLIP_EXTRA_FALSE      (1 << 0)
#define NLM_CLIP_EXTRA_TRUE       (1 << 1)
#define NLM_CLIP_TYPE_UNORM       (1 << 2)
#define NLM_CLIP_TYPE_UNSIGNED    (1 << 3)
#define NLM_CLIP_TYPE_STACKED     (1 << 4)
#define NLM_CLIP_REF_LUMA         (1 << 5)
#define NLM_CLIP_REF_CHROMA       (1 << 6)
#define NLM_CLIP_REF_YUV          (1 << 7)
#define NLM_CLIP_REF_RGB          (1 << 8)

#define NLM_WMODE_WELSCH           0x0
#define NLM_WMODE_BISQUARE_A       0x1
#define NLM_WMODE_BISQUARE_B       0x2
#define NLM_WMODE_BISQUARE_C       0x3

#define STR(code) case code: return #code

//////////////////////////////////////////
// Kernel function
const char* nlmClipTypeToString(cl_uint clip);
const char* nlmClipRefToString(cl_uint clip);
const char* nlmWmodeToString(cl_int wmode);

//////////////////////////////////////////
// Kernel Definition
static const char* kernel_source_code =
"#ifndef cl_amd_media_ops2                                                                                        \n" \
"#  define amd_max3(a, b, c)   fmax(a, fmax(b, c))                                                                \n" \
"#else                                                                                                            \n" \
"#  pragma OPENCL EXTENSION cl_amd_media_ops2 : enable                                                            \n" \
"#endif                                                                                                           \n" \
"                                                                                                                 \n" \
"#define NLM_NORM            ( 255.0f * 255.0f )                                                                  \n" \
"#define NLM_LEGACY          ( 3.0f )                                                                             \n" \
"#define NLM_S_SIZE          ( (2 * NLM_S + 1) * (2 * NLM_S + 1) )                                                \n" \
"#define NLM_H2_INV_NORM     ( NLM_NORM / (NLM_LEGACY * NLM_H * NLM_H * NLM_S_SIZE) )                             \n" \
"#define NLM_16BIT_MSB       ( 256.0f / (257.0f * 255.0f) )                                                       \n" \
"#define NLM_16BIT_LSB       (   1.0f / (257.0f * 255.0f) )                                                       \n" \
"                                                                                                                 \n" \
"#if   defined(NLM_CLIP_REF_LUMA)                                                                                 \n" \
"#  define NLM_CHANNELS  1                                                                                        \n" \
"#elif defined(NLM_CLIP_REF_CHROMA)                                                                               \n" \
"#  define NLM_CHANNELS  2                                                                                        \n" \
"#elif defined(NLM_CLIP_REF_YUV) || defined(NLM_CLIP_REF_RGB)                                                     \n" \
"#  define NLM_CHANNELS  3                                                                                        \n" \
"#endif                                                                                                           \n" \
"                                                                                                                 \n" \
"__constant sampler_t nne = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE  | CLK_FILTER_NEAREST;                 \n" \
"__constant sampler_t clm = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                 \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmDistance(__read_only image2d_array_t U1, __write_only image2d_array_t U4, const int t, const int4 q) {   \n" \
"                                                                                                                 \n" \
"   int x = (int) get_global_id(0);                                                                              \n" \
"   int y = (int) get_global_id(1);                                                                              \n" \
"   if (x >= VI_DIM_X || y >= VI_DIM_Y) return;                                                                   \n" \
"                                                                                                                 \n" \
"   int4 p = (int4) (x, y, t, 0);                                                                                 \n" \
"                                                                                                                 \n" \
"#if   defined(NLM_CLIP_REF_LUMA)                                                                                 \n" \
"   float  u1    = read_imagef(U1, nne, p    ).x;                                                                 \n" \
"   float  u1_pq = read_imagef(U1, clm, p + q).x;                                                                 \n" \
"   float  val   = 3.0f * pown(u1 - u1_pq, 2);                                                                    \n" \
"#elif defined(NLM_CLIP_REF_CHROMA)                                                                               \n" \
"   float2 u1    = read_imagef(U1, nne, p    ).xy;                                                                \n" \
"   float2 u1_pq = read_imagef(U1, clm, p + q).xy;                                                                \n" \
"   float  dst_u = pown(u1.x - u1_pq.x, 2);                                                                       \n" \
"   float  dst_v = pown(u1.y - u1_pq.y, 2);                                                                       \n" \
"   float  val   = 1.5f * (dst_u + dst_v);                                                                        \n" \
"#elif defined(NLM_CLIP_REF_YUV)                                                                                  \n" \
"   float3 u1    = read_imagef(U1, nne, p    ).xyz;                                                               \n" \
"   float3 u1_pq = read_imagef(U1, clm, p + q).xyz;                                                               \n" \
"   float  dst_y = pown(u1.x - u1_pq.x, 2);                                                                       \n" \
"   float  dst_u = pown(u1.y - u1_pq.y, 2);                                                                       \n" \
"   float  dst_v = pown(u1.z - u1_pq.z, 2);                                                                       \n" \
"   float  val   = dst_y + dst_u + dst_v;                                                                         \n" \
"#elif defined(NLM_CLIP_REF_RGB)                                                                                  \n" \
"   float3 u1    = read_imagef(U1, nne, p    ).xyz;                                                               \n" \
"   float3 u1_pq = read_imagef(U1, clm, p + q).xyz;                                                               \n" \
"   float  m_red = native_divide(u1.x + u1_pq.x, 6.0f);                                                           \n" \
"   float  dst_r = (2.0f/3.0f + m_red) * pown(u1.x - u1_pq.x, 2);                                                 \n" \
"   float  dst_g = (4.0f/3.0f        ) * pown(u1.y - u1_pq.y, 2);                                                 \n" \
"   float  dst_b = (     1.0f - m_red) * pown(u1.z - u1_pq.z, 2);                                                 \n" \
"   float  val   = dst_r + dst_g + dst_b;                                                                         \n" \
"#endif                                                                                                           \n" \
"                                                                                                                 \n" \
"   write_imagef(U4, p, (float4) val);                                                                            \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel __attribute__((reqd_work_group_size(HRZ_BLOCK_X, HRZ_BLOCK_Y, 1)))                                      \n" \
"void nlmHorizontal(__read_only image2d_array_t U4_in, __write_only image2d_array_t U4_out, const int t) {        \n" \
"                                                                                                                 \n" \
"   __local float buffer[HRZ_BLOCK_Y][(HRZ_RESULT + 2) * HRZ_BLOCK_X];                                            \n" \
"   int x = (get_group_id(0) * HRZ_RESULT - 1) * HRZ_BLOCK_X + (int) get_local_id(0);                             \n" \
"   int y = (int) get_global_id(1);                                                                               \n" \
"                                                                                                                 \n" \
"   for (int i = 1; i < 1 + HRZ_RESULT; i++)                                                                      \n" \
"       buffer[get_local_id(1)][get_local_id(0) + i * HRZ_BLOCK_X] =                                              \n" \
"           read_imagef(U4_in, clm, (int4) (x + i * HRZ_BLOCK_X, y, t, 0)).x;                                     \n" \
"                                                                                                                 \n" \
"   buffer[get_local_id(1)][get_local_id(0)] =                                                                    \n" \
"       read_imagef(U4_in, clm, (int4) (x, y, t, 0)).x;                                                           \n" \
"                                                                                                                 \n" \
"   buffer[get_local_id(1)][get_local_id(0) + (1 + HRZ_RESULT) * HRZ_BLOCK_X] =                                   \n" \
"       read_imagef(U4_in, clm, (int4) (x + (1 + HRZ_RESULT) * HRZ_BLOCK_X, y, t, 0)).x;                          \n" \
"                                                                                                                 \n" \
"   barrier(CLK_LOCAL_MEM_FENCE);                                                                                 \n" \
"                                                                                                                 \n" \
"   for (int i = 1; i < 1 + HRZ_RESULT; i++) {                                                                    \n" \
"       if ((x + i * HRZ_BLOCK_X) >= VI_DIM_X || y >= VI_DIM_Y) return;                                           \n" \
"       float sum = 0.0f;                                                                                         \n" \
"       for (int j = -NLM_S; j <= NLM_S; j++)                                                                     \n" \
"           sum += buffer[get_local_id(1)][get_local_id(0) + i * HRZ_BLOCK_X + j];                                \n" \
"       write_imagef(U4_out, (int4) (x + i * HRZ_BLOCK_X, y, t, 0), (float4) sum);                                \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel __attribute__((reqd_work_group_size(VRT_BLOCK_X, VRT_BLOCK_Y, 1)))                                      \n" \
"void nlmVertical(__read_only image2d_array_t U4_in, __write_only image2d_array_t U4_out, const int t) {          \n" \
"                                                                                                                 \n" \
"   __local float buffer[VRT_BLOCK_X][(VRT_RESULT + 2) * VRT_BLOCK_Y + 1];                                        \n" \
"   int x = (int) get_global_id(0);                                                                               \n" \
"   int y = (get_group_id(1) * VRT_RESULT - 1) * VRT_BLOCK_Y + (int) get_local_id(1);                             \n" \
"                                                                                                                 \n" \
"   for (int i = 1; i < 1 + VRT_RESULT; i++)                                                                      \n" \
"       buffer[get_local_id(0)][get_local_id(1) + i * VRT_BLOCK_Y] =                                              \n" \
"           read_imagef(U4_in, clm, (int4) (x, y + i * VRT_BLOCK_Y, t, 0)).x;                                     \n" \
"                                                                                                                 \n" \
"   buffer[get_local_id(0)][get_local_id(1)] =                                                                    \n" \
"       read_imagef(U4_in, clm, (int4) (x, y, t, 0)).x;                                                           \n" \
"                                                                                                                 \n" \
"   buffer[get_local_id(0)][get_local_id(1) + (1 + VRT_RESULT) * VRT_BLOCK_Y] =                                   \n" \
"       read_imagef(U4_in, clm, (int4) (x, y + (1 + VRT_RESULT) * VRT_BLOCK_Y, t, 0)).x;                          \n" \
"                                                                                                                 \n" \
"   barrier(CLK_LOCAL_MEM_FENCE);                                                                                 \n" \
"                                                                                                                 \n" \
"   for (int i = 1; i < 1 + VRT_RESULT; i++) {                                                                    \n" \
"       if (x >= VI_DIM_X || (y + i * VRT_BLOCK_Y) >= VI_DIM_Y) return;                                           \n" \
"       float sum = 0.0f;                                                                                         \n" \
"       for (int j = -NLM_S; j <= NLM_S; j++)                                                                     \n" \
"           sum += buffer[get_local_id(0)][get_local_id(1) + i * VRT_BLOCK_Y + j];                                \n" \
"                                                                                                                 \n" \
"#if   defined(NLM_WMODE_WELSCH)                                                                                  \n" \
"       float val = native_exp(- sum * NLM_H2_INV_NORM);                                                          \n" \
"#elif defined(NLM_WMODE_BISQUARE_A)                                                                              \n" \
"       float val = fdim(1.0f, sum * NLM_H2_INV_NORM);                                                            \n" \
"#elif defined(NLM_WMODE_BISQUARE_B)                                                                              \n" \
"       float val = pown(fdim(1.0f, sum * NLM_H2_INV_NORM), 2);                                                   \n" \
"#elif defined(NLM_WMODE_BISQUARE_C)                                                                              \n" \
"       float val = pown(fdim(1.0f, sum * NLM_H2_INV_NORM), 8);                                                   \n" \
"#endif                                                                                                           \n" \
"                                                                                                                 \n" \
"       write_imagef(U4_out, (int4) (x, y + i * VRT_BLOCK_Y, t, 0), (float4) val);                                \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmAccumulation(__read_only image2d_array_t U1, __global float* U2, __read_only image2d_array_t U4,         \n" \
"__global float* U5, const int4 q) {                                                                              \n" \
"                                                                                                                 \n" \
"   int x = (int) get_global_id(0);                                                                               \n" \
"   int y = (int) get_global_id(1);                                                                               \n" \
"   if (x >= VI_DIM_X || y >= VI_DIM_Y) return;                                                                   \n" \
"                                                                                                                 \n" \
"   int4 p = (int4) (x, y, NLM_D, 0);                                                                             \n" \
"   int  gidx = y * VI_DIM_X + x;                                                                                 \n" \
"                                                                                                                 \n" \
"   float u4    = read_imagef(U4, nne, p    ).x;                                                                  \n" \
"   float u4_mq = read_imagef(U4, clm, p - q).x;                                                                  \n" \
"   U5[gidx]    = amd_max3(u4, u4_mq, U5[gidx]);                                                                  \n" \
"                                                                                                                 \n" \
"#if   (NLM_CHANNELS == 1)                                                                                        \n" \
"   float  u1_pq = read_imagef(U1, clm, p + q).x;                                                                 \n" \
"   float  u1_mq = read_imagef(U1, clm, p - q).x;                                                                 \n" \
"   float2 u2    = vload2(gidx, U2);                                                                              \n" \
"          u2.x += (u4 * u1_pq) + (u4_mq * u1_mq);                                                                \n" \
"          u2.y += (u4 + u4_mq);                                                                                  \n" \
"   vstore2(u2, gidx, U2);                                                                                        \n" \
"#elif (NLM_CHANNELS == 2)                                                                                        \n" \
"   float2 u1_pq = read_imagef(U1, clm, p + q).xy;                                                                \n" \
"   float2 u1_mq = read_imagef(U1, clm, p - q).xy;                                                                \n" \
"   float3 u2    = vload3(gidx, U2);                                                                              \n" \
"          u2.x += (u4 * u1_pq.x) + (u4_mq * u1_mq.x);                                                            \n" \
"          u2.y += (u4 * u1_pq.y) + (u4_mq * u1_mq.y);                                                            \n" \
"          u2.z += (u4 + u4_mq);                                                                                  \n" \
"   vstore3(u2, gidx, U2);                                                                                        \n" \
"#elif (NLM_CHANNELS == 3)                                                                                        \n" \
"   float3 u1_pq = read_imagef(U1, clm, p + q).xyz;                                                               \n" \
"   float3 u1_mq = read_imagef(U1, clm, p - q).xyz;                                                               \n" \
"   float4 u2    = vload4(gidx, U2);                                                                              \n" \
"          u2.x += (u4 * u1_pq.x) + (u4_mq * u1_mq.x);                                                            \n" \
"          u2.y += (u4 * u1_pq.y) + (u4_mq * u1_mq.y);                                                            \n" \
"          u2.z += (u4 * u1_pq.z) + (u4_mq * u1_mq.z);                                                            \n" \
"          u2.w += (u4 + u4_mq);                                                                                  \n" \
"   vstore4(u2, gidx, U2);                                                                                        \n" \
"#endif                                                                                                           \n" \
"                                                                                                                 \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmFinish(__read_only image2d_array_t U1_in, __write_only image2d_t U1_out, __global float* U2,             \n" \
"__global float* U5) {                                                                                            \n" \
"                                                                                                                 \n" \
"   int x = (int) get_global_id(0);                                                                               \n" \
"   int y = (int) get_global_id(1);                                                                               \n" \
"   if (x >= VI_DIM_X || y >= VI_DIM_Y) return;                                                                   \n" \
"                                                                                                                 \n" \
"   int4  p = (int4) (x, y, NLM_D, 0);                                                                            \n" \
"   int2  s = (int2) (x, y);                                                                                      \n" \
"   int   gidx = y * VI_DIM_X + x;                                                                                \n" \
"   float m = NLM_WREF * U5[gidx];                                                                                \n" \
"                                                                                                                 \n" \
"#if   (NLM_CHANNELS == 1)                                                                                        \n" \
"   float  u1    = read_imagef(U1_in, nne, p).x;                                                                  \n" \
"   float2 u2    = vload2(gidx, U2);                                                                              \n" \
"   float  den   = m + u2.y;                                                                                      \n" \
"   float  val   = (u1 * m + u2.x) / den;                                                                         \n" \
"   write_imagef(U1_out, s, (float4) (val, 0.0f, 0.0f, 0.0f));                                                    \n" \
"#elif (NLM_CHANNELS == 2)                                                                                        \n" \
"   float2 u1    = read_imagef(U1_in, nne, p).xy;                                                                 \n" \
"   float3 u2    = vload3(gidx, U2);                                                                              \n" \
"   float  den   = m + u2.z;                                                                                      \n" \
"   float  val_x = (u1.x * m + u2.x) / den;                                                                       \n" \
"   float  val_y = (u1.y * m + u2.y) / den;                                                                       \n" \
"   write_imagef(U1_out, s,  (float4) (val_x, val_y, 0.0f, 0.0f));                                                \n" \
"#elif (NLM_CHANNELS == 3)                                                                                        \n" \
"   float3 u1    = read_imagef(U1_in, nne, p).xyz;                                                                \n" \
"   float4 u2    = vload4(gidx, U2);                                                                              \n" \
"   float  den   = m + u2.w;                                                                                      \n" \
"   float  val_x = (u1.x * m + u2.x) / den;                                                                       \n" \
"   float  val_y = (u1.y * m + u2.y) / den;                                                                       \n" \
"   float  val_z = (u1.z * m + u2.z) / den;                                                                       \n" \
"   write_imagef(U1_out, s,  (float4) (val_x, val_y, val_z, 0.0f));                                               \n" \
"#endif                                                                                                           \n" \
"                                                                                                                 \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"//////////////////////////////////////////                                                                       \n" \
"// OpenCL Specification                                                                                          \n" \
"// 8.3 Conversion Rules, 2012, pp.336-340                                                                        \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmPack(__read_only image2d_t R, __read_only image2d_t G, __read_only image2d_t B,                          \n" \
"__read_only image2d_t R_lsb, __read_only image2d_t G_lsb, __read_only image2d_t B_lsb,                           \n" \
"__write_only image2d_array_t U1, const int t) {                                                                  \n" \
"                                                                                                                 \n" \
"   int x = (int) get_global_id(0);                                                                               \n" \
"   int y = (int) get_global_id(1);                                                                               \n" \
"   if (x >= VI_DIM_X || y >= VI_DIM_Y) return;                                                                   \n" \
"                                                                                                                 \n" \
"#if   defined(NLM_CLIP_TYPE_STACKED)  && (NLM_CHANNELS == 1)                                                     \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r_msb = convert_float(read_imageui(R,     nne, s).x);                                                   \n" \
"   float r_lsb = convert_float(read_imageui(R_lsb, nne, s).x);                                                   \n" \
"   float r     = NLM_16BIT_MSB * r_msb + NLM_16BIT_LSB * r_lsb;                                                  \n" \
"   write_imagef(U1, p, (float4) (r, 0.0f, 0.0f, 0.0f));                                                          \n" \
"#elif defined(NLM_CLIP_TYPE_UNORM)    && (NLM_CHANNELS == 2)                                                     \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r     = read_imagef(R, nne, s).x;                                                                       \n" \
"   float g     = read_imagef(G, nne, s).x;                                                                       \n" \
"   write_imagef(U1, p, (float4) (r, g, 0.0f, 0.0f));                                                             \n" \
"#elif defined(NLM_CLIP_TYPE_STACKED)  && (NLM_CHANNELS == 2)                                                     \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r_msb = convert_float(read_imageui(R,     nne, s).x);                                                   \n" \
"   float g_msb = convert_float(read_imageui(G,     nne, s).x);                                                   \n" \
"   float r_lsb = convert_float(read_imageui(R_lsb, nne, s).x);                                                   \n" \
"   float g_lsb = convert_float(read_imageui(G_lsb, nne, s).x);                                                   \n" \
"   float r     = NLM_16BIT_MSB * r_msb + NLM_16BIT_LSB * r_lsb;                                                  \n" \
"   float g     = NLM_16BIT_MSB * g_msb + NLM_16BIT_LSB * g_lsb;                                                  \n" \
"   write_imagef(U1, p, (float4) (r, g, 0.0f, 0.0f));                                                             \n" \
"#elif defined(NLM_CLIP_TYPE_UNORM)    && (NLM_CHANNELS == 3)                                                     \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r     = read_imagef(R, nne, s).x;                                                                       \n" \
"   float g     = read_imagef(G, nne, s).x;                                                                       \n" \
"   float b     = read_imagef(B, nne, s).x;                                                                       \n" \
"   write_imagef(U1, p, (float4) (r, g, b, 0.0f));                                                                \n" \
"#elif defined(NLM_CLIP_TYPE_UNSIGNED) && (NLM_CHANNELS == 3)                                                     \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r     = convert_float(read_imageui(R, nne, s).x) / 1023.0f;                                             \n" \
"   float g     = convert_float(read_imageui(G, nne, s).x) / 1023.0f;                                             \n" \
"   float b     = convert_float(read_imageui(B, nne, s).x) / 1023.0f;                                             \n" \
"   write_imagef(U1, p, (float4) (r, g, b, 0.0f));                                                                \n" \
"#elif defined(NLM_CLIP_TYPE_STACKED)  && (NLM_CHANNELS == 3)                                                     \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r_msb = convert_float(read_imageui(R,     nne, s).x);                                                   \n" \
"   float g_msb = convert_float(read_imageui(G,     nne, s).x);                                                   \n" \
"   float b_msb = convert_float(read_imageui(B,     nne, s).x);                                                   \n" \
"   float r_lsb = convert_float(read_imageui(R_lsb, nne, s).x);                                                   \n" \
"   float g_lsb = convert_float(read_imageui(G_lsb, nne, s).x);                                                   \n" \
"   float b_lsb = convert_float(read_imageui(B_lsb, nne, s).x);                                                   \n" \
"   float r     = NLM_16BIT_MSB * r_msb + NLM_16BIT_LSB * r_lsb;                                                  \n" \
"   float g     = NLM_16BIT_MSB * g_msb + NLM_16BIT_LSB * g_lsb;                                                  \n" \
"   float b     = NLM_16BIT_MSB * b_msb + NLM_16BIT_LSB * b_lsb;                                                  \n" \
"   write_imagef(U1, p, (float4) (r, g, b, 0.0f));                                                                \n" \
"#endif                                                                                                           \n" \
"                                                                                                                 \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmUnpack(__write_only image2d_t R, __write_only image2d_t G, __write_only image2d_t B,                     \n" \
"__write_only image2d_t R_lsb, __write_only image2d_t G_lsb, __write_only image2d_t B_lsb,                        \n" \
"__read_only image2d_t U1) {                                                                                      \n" \
"                                                                                                                 \n" \
"   int x = (int) get_global_id(0);                                                                               \n" \
"   int y = (int) get_global_id(1);                                                                               \n" \
"   if (x >= VI_DIM_X || y >= VI_DIM_Y) return;                                                                   \n" \
"                                                                                                                 \n" \
"#if   defined(NLM_CLIP_TYPE_STACKED)  && (NLM_CHANNELS == 1)                                                     \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float  val = read_imagef(U1, nne, s).x;                                                                       \n" \
"   ushort r   = convert_ushort_sat_rte(val * 65535.0f);                                                          \n" \
"   write_imageui(R,     s, (uint4)  (r >> CHAR_BIT, 0u, 0u, 0u));                                                \n" \
"   write_imageui(R_lsb, s, (uint4)  (r &  0xFF,     0u, 0u, 0u));                                                \n" \
"#elif defined(NLM_CLIP_TYPE_UNORM)    && (NLM_CHANNELS == 2)                                                     \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float2 val = read_imagef(U1, nne, s).xy;                                                                      \n" \
"   write_imagef(R,      s, (float4) (val.x, 0.0f, 0.0f, 0.0f));                                                  \n" \
"   write_imagef(G,      s, (float4) (val.y, 0.0f, 0.0f, 0.0f));                                                  \n" \
"#elif defined(NLM_CLIP_TYPE_STACKED)  && (NLM_CHANNELS == 2)                                                     \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float2 val = read_imagef(U1, nne, s).xy;                                                                      \n" \
"   ushort r   = convert_ushort_sat_rte(val.x * 65535.0f);                                                        \n" \
"   ushort g   = convert_ushort_sat_rte(val.y * 65535.0f);                                                        \n" \
"   write_imageui(R,     s, (uint4)  (r >> CHAR_BIT, 0u, 0u, 0u));                                                \n" \
"   write_imageui(G,     s, (uint4)  (g >> CHAR_BIT, 0u, 0u, 0u));                                                \n" \
"   write_imageui(R_lsb, s, (uint4)  (r &  0xFF,     0u, 0u, 0u));                                                \n" \
"   write_imageui(G_lsb, s, (uint4)  (g &  0xFF,     0u, 0u, 0u));                                                \n" \
"#elif defined(NLM_CLIP_TYPE_UNORM)    && (NLM_CHANNELS == 3)                                                     \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float3 val = read_imagef(U1, nne, s).xyz;                                                                     \n" \
"   write_imagef(R,      s, (float4) (val.x, 0.0f, 0.0f, 0.0f));                                                  \n" \
"   write_imagef(G,      s, (float4) (val.y, 0.0f, 0.0f, 0.0f));                                                  \n" \
"   write_imagef(B,      s, (float4) (val.z, 0.0f, 0.0f, 0.0f));                                                  \n" \
"#elif defined(NLM_CLIP_TYPE_UNSIGNED) && (NLM_CHANNELS == 3)                                                     \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float3 val = read_imagef(U1, nne, s).xyz;                                                                     \n" \
"   ushort r   = min(convert_ushort_sat_rte(val.x * 1023.0f), (ushort) 0x3FF);                                    \n" \
"   ushort g   = min(convert_ushort_sat_rte(val.y * 1023.0f), (ushort) 0x3FF);                                    \n" \
"   ushort b   = min(convert_ushort_sat_rte(val.z * 1023.0f), (ushort) 0x3FF);                                    \n" \
"   write_imageui(R,     s, (uint4)  (r, 0u, 0u, 0u));                                                            \n" \
"   write_imageui(G,     s, (uint4)  (g, 0u, 0u, 0u));                                                            \n" \
"   write_imageui(B,     s, (uint4)  (b, 0u, 0u, 0u));                                                            \n" \
"#elif defined(NLM_CLIP_TYPE_STACKED)  && (NLM_CHANNELS == 3)                                                     \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float3 val = read_imagef(U1, nne, s).xyz;                                                                     \n" \
"   ushort r   = convert_ushort_sat_rte(val.x * 65535.0f);                                                        \n" \
"   ushort g   = convert_ushort_sat_rte(val.y * 65535.0f);                                                        \n" \
"   ushort b   = convert_ushort_sat_rte(val.z * 65535.0f);                                                        \n" \
"   write_imageui(R,     s, (uint4)  (r >> CHAR_BIT, 0u, 0u, 0u));                                                \n" \
"   write_imageui(G,     s, (uint4)  (g >> CHAR_BIT, 0u, 0u, 0u));                                                \n" \
"   write_imageui(B,     s, (uint4)  (b >> CHAR_BIT, 0u, 0u, 0u));                                                \n" \
"   write_imageui(R_lsb, s, (uint4)  (r &  0xFF,     0u, 0u, 0u));                                                \n" \
"   write_imageui(G_lsb, s, (uint4)  (g &  0xFF,     0u, 0u, 0u));                                                \n" \
"   write_imageui(B_lsb, s, (uint4)  (b &  0xFF,     0u, 0u, 0u));                                                \n" \
"#endif                                                                                                           \n" \
"                                                                                                                 \n" \
"}                                                                                                                ";

#endif //__NLM_KERNEL_H__