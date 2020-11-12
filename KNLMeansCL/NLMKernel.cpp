/*
*    This file is part of KNLMeansCL,
*    Copyright(C) 2015-2020  Edoardo Brunetti.
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

#include "NLMKernel.h"

#define STR(code) case code: return #code

//////////////////////////////////////////
// Kernel function
const char* nlmClipTypeToString(unsigned int clip)
{
    if (clip & NLM_CLIP_TYPE_UNORM)
        return "NLM_CLIP_TYPE_UNORM";
    else if (clip & NLM_CLIP_TYPE_UNSIGNED)
      return "NLM_CLIP_TYPE_UNSIGNED";
    else if (clip & NLM_CLIP_TYPE_UNORM_IN_UNSIGNED_OUT)
        return "NLM_CLIP_TYPE_UNORM_IN_UNSIGNED_OUT";
    else if (clip & NLM_CLIP_TYPE_UNSIGNED_101010)
      return "NLM_CLIP_TYPE_UNSIGNED_101010";
    else if (clip & NLM_CLIP_TYPE_STACKED)
        return "NLM_CLIP_TYPE_STACKED";
    else
        return "OCL_UTILS_CLIP_TYPE_ERROR";
}

const char* nlmClipRefToString(unsigned int clip)
{
    if (clip & NLM_CLIP_REF_LUMA)
        return "NLM_CLIP_REF_LUMA";
    else if (clip & NLM_CLIP_REF_CHROMA)
        return "NLM_CLIP_REF_CHROMA";
    else if (clip & NLM_CLIP_REF_YUV)
        return "NLM_CLIP_REF_YUV";
    else if ((clip & NLM_CLIP_REF_RGB) || (clip & NLM_CLIP_REF_PACKEDRGB))
        return "NLM_CLIP_REF_RGB";
    else
        return "OCL_UTILS_CLIP_REF_ERROR";
}

const char* nlmWmodeToString(unsigned int wmode)
{
    switch (wmode) {
        STR(NLM_WMODE_WELSCH);
        STR(NLM_WMODE_BISQUARE_A);
        STR(NLM_WMODE_BISQUARE_B);
        STR(NLM_WMODE_BISQUARE_C);
        default: return "OCL_UTILS_WMODE_ERROR";
    }
}

//////////////////////////////////////////
// Kernel Definition

// UNORM_IN: 8 bits and 9-16 bits => normalized internal range
//           8 and 16 bits are natively normalized by 255 and 65535; 32 bit float => 32 bit float directly
//           9-15 bits: correct the normalization of 65535 by a manual step
// UNSIGNED_IN: 8 to 16 bits => normalized internal range, with bit-depth aware range conversion, division done manually
// UNORM_OUT: internal normalized range => 8 or 16 bits (w/ automatic saturation) or to 32 bit float output range
// UNSIGNED_OUT: internal normalized range => 8 to 16 bits, with manually done saturation, useful for 9 to 15 bits

// NLM_CLIP_TYPE_UNSIGNED_101010 is special, used when supported

const char* kernel_source_code =
"#ifndef cl_amd_media_ops2                                                                                        \n" \
"#  define amd_max3(a, b, c)   fmax(a, fmax(b, c))                                                                \n" \
"#else                                                                                                            \n" \
"#  pragma OPENCL EXTENSION cl_amd_media_ops2 : enable                                                            \n" \
"#endif                                                                                                           \n" \
"                                                                                                                 \n" \
"#if defined(NLM_CLIP_TYPE_UNORM_IN_UNSIGNED_OUT)                                                                 \n" \
"#  define NLM_CLIP_TYPE_UNORM_IN                                                                                 \n" \
"#  define NLM_CLIP_TYPE_UNSIGNED_OUT                                                                             \n" \
"#endif                                                                                                           \n" \
"#if defined(NLM_CLIP_TYPE_UNORM)                                                                                 \n" \
"#  define NLM_CLIP_TYPE_UNORM_IN                                                                                 \n" \
"#  define NLM_CLIP_TYPE_UNORM_OUT                                                                                \n" \
"#endif                                                                                                           \n" \
"#if defined(NLM_CLIP_TYPE_UNSIGNED)                                                                              \n" \
"#  define NLM_CLIP_TYPE_UNSIGNED_IN                                                                              \n" \
"#  define NLM_CLIP_TYPE_UNSIGNED_OUT                                                                             \n" \
"#endif                                                                                                           \n" \
"                                                                                                                 \n" \
"#define NLM_MAXPIXELVALUE        ((1 << BITDEPTH) - 1)                                                           \n" \
"#define NLM_MAXPIXELVALUE_RECP   (1.0f / NLM_MAXPIXELVALUE)                                                      \n" \
"                                                                                                                 \n" \
"#if defined(NLM_CLIP_TYPE_UNORM_IN) && (BITDEPTH >= 9) && (BITDEPTH <= 16)                                       \n" \
"#  define NLM_SCALE_UNORM16_TO_X       (65535.0f / NLM_MAXPIXELVALUE)                                            \n" \
"#  define NLM_SCALE_UNORM16_TO_X_RECP  (NLM_MAXPIXELVALUE / 65535.0f)                                            \n" \
"#elif defined(NLM_CLIP_TYPE_UNSIGNED_IN) && defined(NLM_CLIP_TYPE_UNSIGNED_OUT)                                  \n" \
"#  define NLM_SCALE_UNORM16_TO_X       1.0f                                                                      \n" \
"#  define NLM_SCALE_UNORM16_TO_X_RECP  1.0f                                                                      \n" \
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
"   int x = (int) get_global_id(0);                                                                               \n" \
"   int y = (int) get_global_id(1);                                                                               \n" \
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
// stacked planes, combine upper and lower 8 bits before normalization
"#if   defined(NLM_CLIP_TYPE_STACKED)  && (NLM_CHANNELS == 1)                                                     \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r_msb = convert_float(read_imageui(R,     nne, s).x);                                                   \n" \
"   float r_lsb = convert_float(read_imageui(R_lsb, nne, s).x);                                                   \n" \
"   float r     = NLM_16BIT_MSB * r_msb + NLM_16BIT_LSB * r_lsb;                                                  \n" \
"   write_imagef(U1, p, (float4) (r, 0.0f, 0.0f, 0.0f));                                                          \n" \
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
// UNORM_IN: 9-15 bit: normalization assumes full 16 bits input, correct the rest
"#elif defined(NLM_CLIP_TYPE_UNORM_IN)    && (NLM_CHANNELS == 1) && BITDEPTH >= 9 && BITDEPTH <= 15               \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r     = read_imagef(R, nne, s).x * NLM_SCALE_UNORM16_TO_X;                                              \n" \
"   write_imagef(U1, p, (float4) (r, 0.0f, 0.0f, 0.0f));                                                          \n" \
// UNORM_IN: no pack occurs on a single channel UNORM input, normalization is automatic assuming full 8 or 16 bits input
"#elif defined(NLM_CLIP_TYPE_UNORM_IN)    && (NLM_CHANNELS == 1)                                                  \n" \
"#elif defined(NLM_CLIP_TYPE_UNORM_IN)    && (NLM_CHANNELS == 2) && BITDEPTH >= 9 && BITDEPTH <= 15               \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r     = read_imagef(R, nne, s).x * NLM_SCALE_UNORM16_TO_X;                                              \n" \
"   float g     = read_imagef(G, nne, s).x * NLM_SCALE_UNORM16_TO_X;                                              \n" \
"   write_imagef(U1, p, (float4) (r, g, 0.0f, 0.0f));                                                             \n" \
"#elif defined(NLM_CLIP_TYPE_UNORM_IN)    && (NLM_CHANNELS == 2)                                                  \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r     = read_imagef(R, nne, s).x;                                                                       \n" \
"   float g     = read_imagef(G, nne, s).x;                                                                       \n" \
"   write_imagef(U1, p, (float4) (r, g, 0.0f, 0.0f));                                                             \n" \
"#elif defined(NLM_CLIP_TYPE_UNORM_IN)    && (NLM_CHANNELS == 3) && BITDEPTH >= 9 && BITDEPTH <= 15               \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r     = read_imagef(R, nne, s).x * NLM_SCALE_UNORM16_TO_X;                                              \n" \
"   float g     = read_imagef(G, nne, s).x * NLM_SCALE_UNORM16_TO_X;                                              \n" \
"   float b     = read_imagef(B, nne, s).x * NLM_SCALE_UNORM16_TO_X;                                              \n" \
"   write_imagef(U1, p, (float4) (r, g, b, 0.0f));                                                                \n" \
"#elif defined(NLM_CLIP_TYPE_UNORM_IN)    && (NLM_CHANNELS == 3)                                                  \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r     = read_imagef(R, nne, s).x;                                                                       \n" \
"   float g     = read_imagef(G, nne, s).x;                                                                       \n" \
"   float b     = read_imagef(B, nne, s).x;                                                                       \n" \
"   write_imagef(U1, p, (float4) (r, g, b, 0.0f));                                                                \n" \
// UNSIGNED_IN: normalize the inputs manually depending on bit depth
"#elif defined(NLM_CLIP_TYPE_UNSIGNED_IN) && (NLM_CHANNELS == 1)                                                  \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r     = convert_float(read_imageui(R, nne, s).x) * NLM_MAXPIXELVALUE_RECP;                              \n" \
"   write_imagef(U1, p, (float4) (r, 0.0f, 0.0f, 0.0f));                                                          \n" \
"#elif defined(NLM_CLIP_TYPE_UNSIGNED_IN) && (NLM_CHANNELS == 2)                                                  \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r     = convert_float(read_imageui(R, nne, s).x) * NLM_MAXPIXELVALUE_RECP;                              \n" \
"   float g     = convert_float(read_imageui(G, nne, s).x) * NLM_MAXPIXELVALUE_RECP;                              \n" \
"   write_imagef(U1, p, (float4) (r, g, 0.0f, 0.0f));                                                             \n" \
"#elif defined(NLM_CLIP_TYPE_UNSIGNED_IN) && (NLM_CHANNELS == 3)                                                  \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r     = convert_float(read_imageui(R, nne, s).x) * NLM_MAXPIXELVALUE_RECP;                              \n" \
"   float g     = convert_float(read_imageui(G, nne, s).x) * NLM_MAXPIXELVALUE_RECP;                              \n" \
"   float b     = convert_float(read_imageui(B, nne, s).x) * NLM_MAXPIXELVALUE_RECP;                              \n" \
"   write_imagef(U1, p, (float4) (r, g, b, 0.0f));                                                                \n" \
// Special 10 bit 444 or RGB
"#elif defined(NLM_CLIP_TYPE_UNSIGNED_101010) && (NLM_CHANNELS == 3)                                              \n" \
"   int4  p     = (int4) (x, y, t, 0);                                                                            \n" \
"   int2  s     = (int2) (x, y);                                                                                  \n" \
"   float r     = convert_float(read_imageui(R, nne, s).x) / 1023.0f;                                             \n" \
"   float g     = convert_float(read_imageui(G, nne, s).x) / 1023.0f;                                             \n" \
"   float b     = convert_float(read_imageui(B, nne, s).x) / 1023.0f;                                             \n" \
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
// stacked unpack
"#if   defined(NLM_CLIP_TYPE_STACKED)  && (NLM_CHANNELS == 1)                                                     \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float  val = read_imagef(U1, nne, s).x;                                                                       \n" \
"   ushort r   = convert_ushort_sat_rte(val * 65535.0f);                                                          \n" \
"   write_imageui(R,     s, (uint4)  (r >> CHAR_BIT, 0u, 0u, 0u));                                                \n" \
"   write_imageui(R_lsb, s, (uint4)  (r &  0xFF,     0u, 0u, 0u));                                                \n" \
"#elif defined(NLM_CLIP_TYPE_STACKED)  && (NLM_CHANNELS == 2)                                                     \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float2 val = read_imagef(U1, nne, s).xy;                                                                      \n" \
"   ushort r   = convert_ushort_sat_rte(val.x * 65535.0f);                                                        \n" \
"   ushort g   = convert_ushort_sat_rte(val.y * 65535.0f);                                                        \n" \
"   write_imageui(R,     s, (uint4)  (r >> CHAR_BIT, 0u, 0u, 0u));                                                \n" \
"   write_imageui(G,     s, (uint4)  (g >> CHAR_BIT, 0u, 0u, 0u));                                                \n" \
"   write_imageui(R_lsb, s, (uint4)  (r &  0xFF,     0u, 0u, 0u));                                                \n" \
"   write_imageui(G_lsb, s, (uint4)  (g &  0xFF,     0u, 0u, 0u));                                                \n" \
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
// UNORM_OUT 9-15 bits: clamping is done in float, may not be 100% correct. Implemented but better use UNSIGNED_OUT instead.
"#elif defined(NLM_CLIP_TYPE_UNORM_OUT) && (NLM_CHANNELS == 1) && BITDEPTH >= 9 && BITDEPTH <= 15                 \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float val = min(read_imagef(U1, nne, s).x, 65535.0f);                                                         \n" \
"   write_imagef(R,      s, (float4) (val * NLM_SCALE_UNORM16_TO_X_RECP, 0.0f, 0.0f, 0.0f));                      \n" \
"#elif defined(NLM_CLIP_TYPE_UNORM_OUT) && (NLM_CHANNELS == 2) && BITDEPTH>=9 && BITDEPTH <= 15                   \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float2 val = min(read_imagef(U1, nne, s).xy, 65535.0f);                                                       \n" \
"   write_imagef(R,      s, (float4) (val.x * NLM_SCALE_UNORM16_TO_X_RECP, 0.0f, 0.0f, 0.0f));                    \n" \
"   write_imagef(G,      s, (float4) (val.y * NLM_SCALE_UNORM16_TO_X_RECP, 0.0f, 0.0f, 0.0f));                    \n" \
"#elif defined(NLM_CLIP_TYPE_UNORM_OUT) && (NLM_CHANNELS == 3) && BITDEPTH>=9 && BITDEPTH<=15                     \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float3 val = min(read_imagef(U1, nne, s).xyz, 65535.0f);                                                      \n" \
"   write_imagef(R,      s, (float4) (val.x * NLM_SCALE_UNORM16_TO_X_RECP, 0.0f, 0.0f, 0.0f));                    \n" \
"   write_imagef(G,      s, (float4) (val.y * NLM_SCALE_UNORM16_TO_X_RECP, 0.0f, 0.0f, 0.0f));                    \n" \
"   write_imagef(B,      s, (float4) (val.z * NLM_SCALE_UNORM16_TO_X_RECP, 0.0f, 0.0f, 0.0f));                    \n" \
// UNORM_OUT native 8 or 16 bits: clamping to valid range (saturation) is done natively. 32 bit float: as-is
// no channel separation -> no unpack for single plane
"#elif defined(NLM_CLIP_TYPE_UNORM_OUT)    && (NLM_CHANNELS == 1)                                                 \n" \
// channel separation -> unpack for 2 planes (UV)
"#elif defined(NLM_CLIP_TYPE_UNORM_OUT)    && (NLM_CHANNELS == 2)                                                 \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float2 val = read_imagef(U1, nne, s).xy;                                                                      \n" \
"   write_imagef(R,      s, (float4) (val.x, 0.0f, 0.0f, 0.0f));                                                  \n" \
"   write_imagef(G,      s, (float4) (val.y, 0.0f, 0.0f, 0.0f));                                                  \n" \
// channel separation -> unpack for 3 planes (444 or RGB)
"#elif defined(NLM_CLIP_TYPE_UNORM_OUT)    && (NLM_CHANNELS == 3)                                                 \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float3 val = read_imagef(U1, nne, s).xyz;                                                                     \n" \
"   write_imagef(R,      s, (float4) (val.x, 0.0f, 0.0f, 0.0f));                                                  \n" \
"   write_imagef(G,      s, (float4) (val.y, 0.0f, 0.0f, 0.0f));                                                  \n" \
"   write_imagef(B,      s, (float4) (val.z, 0.0f, 0.0f, 0.0f));                                                  \n" \
// UNSIGNED_OUT: clamp to valid range in integer domain, used for 9-15 bits
"#elif defined(NLM_CLIP_TYPE_UNSIGNED_OUT) && (NLM_CHANNELS == 1)                                                 \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float  val = read_imagef(U1, nne, s).x;                                                                       \n" \
"   ushort r   = min(convert_ushort_sat_rte(val * NLM_MAXPIXELVALUE), (ushort) NLM_MAXPIXELVALUE);                \n" \
"   write_imageui(R,     s, (uint4)  (r, 0u, 0u, 0u));                                                            \n" \
"#elif defined(NLM_CLIP_TYPE_UNSIGNED_OUT) && (NLM_CHANNELS == 2)                                                 \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float2 val = read_imagef(U1, nne, s).xy;                                                                      \n" \
"   ushort r   = min(convert_ushort_sat_rte(val.x * NLM_MAXPIXELVALUE), (ushort) NLM_MAXPIXELVALUE);              \n" \
"   ushort g   = min(convert_ushort_sat_rte(val.y * NLM_MAXPIXELVALUE), (ushort) NLM_MAXPIXELVALUE);              \n" \
"   write_imageui(R,     s, (uint4)  (r, 0u, 0u, 0u));                                                            \n" \
"   write_imageui(G,     s, (uint4)  (g, 0u, 0u, 0u));                                                            \n" \
"#elif defined(NLM_CLIP_TYPE_UNSIGNED_OUT) && (NLM_CHANNELS == 3)                                                 \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float3 val = read_imagef(U1, nne, s).xyz;                                                                     \n" \
"   ushort r   = min(convert_ushort_sat_rte(val.x * NLM_MAXPIXELVALUE), (ushort) NLM_MAXPIXELVALUE);              \n" \
"   ushort g   = min(convert_ushort_sat_rte(val.y * NLM_MAXPIXELVALUE), (ushort) NLM_MAXPIXELVALUE);              \n" \
"   ushort b   = min(convert_ushort_sat_rte(val.z * NLM_MAXPIXELVALUE), (ushort) NLM_MAXPIXELVALUE);              \n" \
"   write_imageui(R,     s, (uint4)  (r, 0u, 0u, 0u));                                                            \n" \
"   write_imageui(G,     s, (uint4)  (g, 0u, 0u, 0u));                                                            \n" \
"   write_imageui(B,     s, (uint4)  (b, 0u, 0u, 0u));                                                            \n" \
// NLM_CLIP_TYPE_UNSIGNED_101010: clamp to valid range in integer domain, 3 planes only, special 444 or RGB 10 bits
"#elif defined(NLM_CLIP_TYPE_UNSIGNED_101010) && (NLM_CHANNELS == 3)                                              \n" \
"   int2   s   = (int2) (x, y);                                                                                   \n" \
"   float3 val = read_imagef(U1, nne, s).xyz;                                                                     \n" \
"   ushort r   = min(convert_ushort_sat_rte(val.x * 1023.0f), (ushort) 0x3FF);                                    \n" \
"   ushort g   = min(convert_ushort_sat_rte(val.y * 1023.0f), (ushort) 0x3FF);                                    \n" \
"   ushort b   = min(convert_ushort_sat_rte(val.z * 1023.0f), (ushort) 0x3FF);                                    \n" \
"   write_imageui(R,     s, (uint4)  (r, 0u, 0u, 0u));                                                            \n" \
"   write_imageui(G,     s, (uint4)  (g, 0u, 0u, 0u));                                                            \n" \
"   write_imageui(B,     s, (uint4)  (b, 0u, 0u, 0u));                                                            \n" \
"#endif                                                                                                           \n" \
"                                                                                                                 \n" \
"}                                                                                                                  ";
