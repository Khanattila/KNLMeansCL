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
*
*    To speed up processing I use an algorithm proposed by B. Goossens,
*    H.Q. Luong, J. Aelterman, A. Pizurica,  and W. Philips, "A GPU-Accelerated
*    Real-Time NLMeans Algorithm for Denoising Color Video Sequences",
*    in Proc. ACIVS (2), 2010, pp.46-57.
*/

//////////////////////////////////////////
// Type Definition

#define nlmSpatialDistance         0x0
#define nlmSpatialHorizontal       0x1
#define nlmSpatialVertical         0x2
#define nlmSpatialAccumulation     0x3
#define nlmSpatialFinish           0x4
#define nlmSpatialPack             0x5
#define nlmSpatialUnpack           0x6
#define nlmSpatialTotal            0x7

#define nlmDistanceLeft            0x0
#define nlmDistanceRight           0x1
#define nlmHorizontal              0x2
#define nlmVertical                0x3
#define nlmAccumulation            0x4
#define nlmFinish                  0x5
#define nlmPack                    0x6
#define nlmUnpack                  0x7
#define nlmTotal                   0x8

#define NLM_NUMBER_KERNELS        nlmTotal

#define NLM_CLIP_EXTRA_FALSE      (1 << 0)
#define NLM_CLIP_EXTRA_TRUE       (1 << 1)
#define NLM_CLIP_TYPE_UNORM       (1 << 2)
#define NLM_CLIP_TYPE_UNSIGNED    (1 << 3)
#define NLM_CLIP_TYPE_STACKED     (1 << 4)
#define NLM_CLIP_REF_LUMA         (1 << 5)
#define NLM_CLIP_REF_CHROMA       (1 << 6)
#define NLM_CLIP_REF_YUV          (1 << 7)
#define NLM_CLIP_REF_RGB          (1 << 8)

#define NLM_WMODE_CAUCHY           0x0
#define NLM_WMODE_WELSCH           0x1
#define NLM_WMODE_BISQUARE         0x2
#define NLM_WMODE_MOD_BISQUARE     0x3

#define HRZ_BLOCK_X                 32
#define HRZ_BLOCK_Y                  8
#define HRZ_RESULT                   4
#define VRT_BLOCK_X                 32
#define VRT_BLOCK_Y                  8
#define VRT_RESULT                   4

//////////////////////////////////////////
// Kernel Definition
static const char* kernel_source_code_spatial =
"#define NLM_16BIT_MSB    ( 256.0f / 65535.0f )                                                                   \n" \
"#define NLM_16BIT_LSB    (   1.0f / 65535.0f )                                                                   \n" \
"#define CHECK_FLAG(flag) ((NLM_TCLIP & (flag)) == (flag))                                                        \n" \
"                                                                                                                 \n" \
"float   norm   (uint   u);                                                                                       \n" \
"ushort  denorm (float  f);                                                                                       \n" \
"                                                                                                                 \n" \
"float   norm   (uint   u) { return native_divide(convert_float(u), NLM_UNORM_MAX); }                             \n" \
"ushort  denorm (float  f) { return convert_ushort_sat(f *          NLM_UNORM_MAX); }                             \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmSpatialDistance(__read_only image2d_t U1, __write_only image2d_t U4, const int2 dim, const int2 q) {     \n" \
"                                                                                                                 \n" \
"   int x = get_global_id(0);                                                                                     \n" \
"   int y = get_global_id(1);                                                                                     \n" \
"   if (x >= dim.x || y >= dim.y) return;                                                                         \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"   int2 p = (int2) (x, y);                                                                                       \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_CLIP_REF_LUMA)) {                                                                          \n" \
"                                                                                                                 \n" \
"       float  u1    = read_imagef(U1, smp, p    ).x;                                                             \n" \
"       float  u1_pq = read_imagef(U1, smp, p + q).x;                                                             \n" \
"       float  dst   = (u1 - u1_pq) * (u1 - u1_pq);                                                               \n" \
"       float  val   = 3.0f * dst;                                                                                \n" \
"       write_imagef(U4, p, (float4) (val, 0.0f, 0.0f, 0.0f));                                                    \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_CHROMA)) {                                                                 \n" \
"                                                                                                                 \n" \
"       float2 u1    = read_imagef(U1, smp, p    ).xy;                                                            \n" \
"       float2 u1_pq = read_imagef(U1, smp, p + q).xy;                                                            \n" \
"       float  dst_u = (u1.x - u1_pq.x) * (u1.x - u1_pq.x);                                                       \n" \
"       float  dst_v = (u1.y - u1_pq.y) * (u1.y - u1_pq.y);                                                       \n" \
"       float  val   = 1.5f * (dst_u + dst_v);                                                                    \n" \
"       write_imagef(U4, p, (float4) (val, 0.0f, 0.0f, 0.0f));                                                    \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_YUV)) {                                                                    \n" \
"                                                                                                                 \n" \
"       float3 u1    = read_imagef(U1, smp, p    ).xyz;                                                           \n" \
"       float3 u1_pq = read_imagef(U1, smp, p + q).xyz;                                                           \n" \
"       float  dst_y = (u1.x - u1_pq.x) * (u1.x - u1_pq.x);                                                       \n" \
"       float  dst_u = (u1.y - u1_pq.y) * (u1.y - u1_pq.y);                                                       \n" \
"       float  dst_v = (u1.z - u1_pq.z) * (u1.z - u1_pq.z);                                                       \n" \
"       float  val   = dst_y + dst_u + dst_v;                                                                     \n" \
"       write_imagef(U4, p, (float4) (val, 0.0f, 0.0f, 0.0f));                                                    \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_RGB)) {                                                                    \n" \
"                                                                                                                 \n" \
"       float3 u1    = read_imagef(U1, smp, p    ).xyz;                                                           \n" \
"       float3 u1_pq = read_imagef(U1, smp, p + q).xyz;                                                           \n" \
"       float  m_red = native_divide(u1.x + u1_pq.x, 6.0f);                                                       \n" \
"       float  dst_r = (2.0f/3.0f + m_red) * (u1.x - u1_pq.x) * (u1.x - u1_pq.x);                                 \n" \
"       float  dst_g = (4.0f/3.0f        ) * (u1.y - u1_pq.y) * (u1.y - u1_pq.y);                                 \n" \
"       float  dst_b = (     1.0f - m_red) * (u1.z - u1_pq.z) * (u1.z - u1_pq.z);                                 \n" \
"       float  val   = dst_r + dst_g + dst_b;                                                                     \n" \
"       write_imagef(U4, p, (float4) (val, 0.0f, 0.0f, 0.0f));                                                    \n" \
"                                                                                                                 \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel __attribute__((reqd_work_group_size(HRZ_BLOCK_X, HRZ_BLOCK_Y, 1)))                                      \n" \
"void nlmSpatialHorizontal(__read_only image2d_t U4_in, __write_only image2d_t U4_out, const int2 dim) {          \n" \
"                                                                                                                 \n" \
"   __local float buffer[HRZ_BLOCK_Y][(HRZ_RESULT + 2) * HRZ_BLOCK_X];                                            \n" \
"   int x = (get_group_id(0) * HRZ_RESULT - 1) * HRZ_BLOCK_X + get_local_id(0);                                   \n" \
"   int y = get_group_id(1) * HRZ_BLOCK_Y + get_local_id(1);                                                      \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"                                                                                                                 \n" \
"   for (int i = 1; i < 1 + HRZ_RESULT; i++)                                                                      \n" \
"       buffer[get_local_id(1)][get_local_id(0) + i * HRZ_BLOCK_X] =                                              \n" \
"           read_imagef(U4_in, smp, (int2) (x + i * HRZ_BLOCK_X, y)).x;                                           \n" \
"                                                                                                                 \n" \
"   buffer[get_local_id(1)][get_local_id(0)] =                                                                    \n" \
"       read_imagef(U4_in, smp, (int2) (x, y)).x;                                                                 \n" \
"                                                                                                                 \n" \
"   buffer[get_local_id(1)][get_local_id(0) + (1 + HRZ_RESULT) * HRZ_BLOCK_X] =                                   \n" \
"       read_imagef(U4_in, smp, (int2) (x + (1 + HRZ_RESULT) * HRZ_BLOCK_X, y)).x;                                \n" \
"                                                                                                                 \n" \
"   barrier(CLK_LOCAL_MEM_FENCE);                                                                                 \n" \
"                                                                                                                 \n" \
"   for (int i = 1; i < 1 + HRZ_RESULT; i++) {                                                                    \n" \
"       if ((x + i * HRZ_BLOCK_X) >= dim.x || y >= dim.y) return;                                                 \n" \
"       float sum = 0.0f;                                                                                         \n" \
"       for (int j = -NLM_S; j <= NLM_S; j++)                                                                     \n" \
"           sum += buffer[get_local_id(1)][get_local_id(0) + i * HRZ_BLOCK_X + j];                                \n" \
"                                                                                                                 \n" \
"       write_imagef(U4_out, (int2) (x + i * HRZ_BLOCK_X, y), (float4) (sum, 0.0f, 0.0f, 0.0f));                  \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel __attribute__((reqd_work_group_size(VRT_BLOCK_X, VRT_BLOCK_Y, 1)))                                      \n" \
"void nlmSpatialVertical(__read_only image2d_t U4_in, __write_only image2d_t U4_out, const int2 dim) {            \n" \
"                                                                                                                 \n" \
"   __local float buffer[VRT_BLOCK_X][(VRT_RESULT + 2) * VRT_BLOCK_Y + 1];                                        \n" \
"   int x = get_group_id(0) * VRT_BLOCK_X + get_local_id(0);                                                      \n" \
"   int y = (get_group_id(1) * VRT_RESULT - 1) * VRT_BLOCK_Y + get_local_id(1);                                   \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"                                                                                                                 \n" \
"   for (int i = 1; i < 1 + VRT_RESULT; i++)                                                                      \n" \
"       buffer[get_local_id(0)][get_local_id(1) + i * VRT_BLOCK_Y] =                                              \n" \
"           read_imagef(U4_in, smp, (int2) (x, y + i * VRT_BLOCK_Y)).x;                                           \n" \
"                                                                                                                 \n" \
"   buffer[get_local_id(0)][get_local_id(1)] =                                                                    \n" \
"       read_imagef(U4_in, smp, (int2) (x, y)).x;                                                                 \n" \
"                                                                                                                 \n" \
"   buffer[get_local_id(0)][get_local_id(1) + (1 + VRT_RESULT) * VRT_BLOCK_Y] =                                   \n" \
"       read_imagef(U4_in, smp, (int2) (x, y + (1 + VRT_RESULT) * VRT_BLOCK_Y)).x;                                \n" \
"                                                                                                                 \n" \
"   barrier(CLK_LOCAL_MEM_FENCE);                                                                                 \n" \
"                                                                                                                 \n" \
"   for (int i = 1; i < 1 + VRT_RESULT; i++) {                                                                    \n" \
"       if (x >= dim.x || (y + i * VRT_BLOCK_Y) >= dim.y) return;                                                 \n" \
"       float sum = 0.0f;                                                                                         \n" \
"       for (int j = -NLM_S; j <= NLM_S; j++)                                                                     \n" \
"           sum += buffer[get_local_id(0)][get_local_id(1) + i * VRT_BLOCK_Y + j];                                \n" \
"                                                                                                                 \n" \
"       float val = 0.0f;                                                                                         \n" \
"       if (NLM_WMODE == NLM_WMODE_CAUCHY) {                                                                      \n" \
"           val = native_recip(1.0f + sum * NLM_H2_INV_NORM);                                                     \n" \
"       } else if (NLM_WMODE == NLM_WMODE_WELSCH) {                                                               \n" \
"           val = native_exp(- sum * NLM_H2_INV_NORM);                                                            \n" \
"       } else if (NLM_WMODE == NLM_WMODE_BISQUARE) {                                                             \n" \
"           val = pown(fdim(1.0f, sum * NLM_H2_INV_NORM), 2);                                                     \n" \
"       } else if (NLM_WMODE == NLM_WMODE_MOD_BISQUARE) {                                                         \n" \
"           val = pown(fdim(1.0f, sum * NLM_H2_INV_NORM), 8);                                                     \n" \
"       }                                                                                                         \n" \
"       write_imagef(U4_out, (int2) (x, y + i * VRT_BLOCK_Y), (float4) (val, 0.0f, 0.0f, 0.0f));                  \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmSpatialAccumulation(__read_only image2d_t U1, __global void* U2, __read_only image2d_t U4,               \n" \
"__global float* M, const int2 dim, const int2 q) {                                                               \n" \
"                                                                                                                 \n" \
"   int x = get_global_id(0);                                                                                     \n" \
"   int y = get_global_id(1);                                                                                     \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"   int2 p    = (int2) (x, y);                                                                                    \n" \
"   int  gidx = mad24(y, dim.x, x);                                                                               \n" \
"                                                                                                                 \n" \
"   float u4    = read_imagef(U4, smp, p    ).x;                                                                  \n" \
"   float u4_mq = read_imagef(U4, smp, p - q).x;                                                                  \n" \
"   M[gidx] = fmax(M[gidx], fmax(u4, u4_mq));                                                                     \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_CLIP_REF_LUMA)) {                                                                          \n" \
"                                                                                                                 \n" \
"       __global float2* U2c = (__global float2*) U2;                                                             \n" \
"       float  u1_pq = read_imagef(U1, smp, p + q).x;                                                             \n" \
"       float  u1_mq = read_imagef(U1, smp, p - q).x;                                                             \n" \
"       float  acc   = (u4 * u1_pq) + (u4_mq * u1_mq);                                                            \n" \
"       float  sum   = (u4 + u4_mq);                                                                              \n" \
"       U2c[gidx] += (float2) (acc, sum);                                                                         \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_CHROMA)) {                                                                 \n" \
"                                                                                                                 \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"       float2 u1_pq = read_imagef(U1, smp, p + q).xy;                                                            \n" \
"       float2 u1_mq = read_imagef(U1, smp, p - q).xy;                                                            \n" \
"       float  acc_u = (u4 * u1_pq.x) + (u4_mq * u1_mq.x);                                                        \n" \
"       float  acc_v = (u4 * u1_pq.y) + (u4_mq * u1_mq.y);                                                        \n" \
"       float  sum   = (u4 + u4_mq);                                                                              \n" \
"       U2c[gidx] += (float4) (acc_u, acc_v, sum, 0.0f);                                                          \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_YUV)) {                                                                    \n" \
"                                                                                                                 \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"       float3 u1_pq = read_imagef(U1, smp, p + q).xyz;                                                           \n" \
"       float3 u1_mq = read_imagef(U1, smp, p - q).xyz;                                                           \n" \
"       float  acc_y = (u4 * u1_pq.x) + (u4_mq * u1_mq.x);                                                        \n" \
"       float  acc_u = (u4 * u1_pq.y) + (u4_mq * u1_mq.y);                                                        \n" \
"       float  acc_v = (u4 * u1_pq.z) + (u4_mq * u1_mq.z);                                                        \n" \
"       float  sum   = (u4 + u4_mq);                                                                              \n" \
"       U2c[gidx] += (float4) (acc_y, acc_u, acc_v, sum);                                                         \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_RGB)) {                                                                    \n" \
"                                                                                                                 \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"       float3 u1_pq = read_imagef(U1, smp, p + q).xyz;                                                           \n" \
"       float3 u1_mq = read_imagef(U1, smp, p - q).xyz;                                                           \n" \
"       float  acc_r = (u4 * u1_pq.x) + (u4_mq * u1_mq.x);                                                        \n" \
"       float  acc_g = (u4 * u1_pq.y) + (u4_mq * u1_mq.y);                                                        \n" \
"       float  acc_b = (u4 * u1_pq.z) + (u4_mq * u1_mq.z);                                                        \n" \
"       float  sum   = (u4 + u4_mq);                                                                              \n" \
"       U2c[gidx] += (float4) (acc_r, acc_g, acc_b, sum);                                                         \n" \
"                                                                                                                 \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmSpatialFinish(__read_only image2d_t U1_in, __write_only image2d_t U1_out, __global void* U2,             \n" \
"__global float* M, const int2 dim) {                                                                             \n" \
"                                                                                                                 \n" \
"   int x = get_global_id(0);                                                                                     \n" \
"   int y = get_global_id(1);                                                                                     \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;                    \n" \
"   int2  p    = (int2) (x, y);                                                                                   \n" \
"   int   gidx = mad24(y, dim.x, x);                                                                              \n" \
"   float wM   = NLM_WREF * M[gidx];                                                                              \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_CLIP_REF_LUMA)) {                                                                          \n" \
"                                                                                                                 \n" \
"       __global float2* U2c = (__global float2*) U2;                                                             \n" \
"       float  u1    = read_imagef(U1_in, smp, p).x;                                                              \n" \
"       float  num   = U2c[gidx].x + wM * u1;                                                                     \n" \
"       float  den   = U2c[gidx].y + wM;                                                                          \n" \
"       float  val   = native_divide(num, den);                                                                   \n" \
"       write_imagef(U1_out, p, (float4) (val, 0.0f, 0.0f, 0.0f));                                                \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_CHROMA)) {                                                                 \n" \
"                                                                                                                 \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"       float2 u1    = read_imagef(U1_in, smp, p).xy;                                                             \n" \
"       float  num_u = U2c[gidx].x + wM * u1.x;                                                                   \n" \
"       float  num_v = U2c[gidx].y + wM * u1.y;                                                                   \n" \
"       float  den   = U2c[gidx].z + wM;                                                                          \n" \
"       float  val_u = native_divide(num_u, den);                                                                 \n" \
"       float  val_v = native_divide(num_v, den);                                                                 \n" \
"       write_imagef(U1_out, p, (float4) (val_u, val_v, 0.0f, 0.0f));                                             \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_YUV)) {                                                                    \n" \
"                                                                                                                 \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"       float3 u1    = read_imagef(U1_in, smp, p).xyz;                                                            \n" \
"       float  num_y = U2c[gidx].x + wM * u1.x;                                                                   \n" \
"       float  num_u = U2c[gidx].y + wM * u1.y;                                                                   \n" \
"       float  num_v = U2c[gidx].z + wM * u1.z;                                                                   \n" \
"       float  den   = U2c[gidx].w + wM;                                                                          \n" \
"       float  val_y = native_divide(num_y, den);                                                                 \n" \
"       float  val_u = native_divide(num_u, den);                                                                 \n" \
"       float  val_v = native_divide(num_v, den);                                                                 \n" \
"       write_imagef(U1_out, p, (float4) (val_y, val_u, val_v, 0.0f));                                            \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_RGB)) {                                                                    \n" \
"                                                                                                                 \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"       float3 u1    = read_imagef(U1_in, smp, p).xyz;                                                            \n" \
"       float  num_r = U2c[gidx].x + wM * u1.x;                                                                   \n" \
"       float  num_g = U2c[gidx].y + wM * u1.y;                                                                   \n" \
"       float  num_b = U2c[gidx].z + wM * u1.z;                                                                   \n" \
"       float  den   = U2c[gidx].w + wM;                                                                          \n" \
"       float  val_r = native_divide(num_r, den);                                                                 \n" \
"       float  val_g = native_divide(num_g, den);                                                                 \n" \
"       float  val_b = native_divide(num_b, den);                                                                 \n" \
"       write_imagef(U1_out, p, (float4) (val_r, val_g, val_b, 0.0f));                                            \n" \
"                                                                                                                 \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmSpatialPack(__read_only image2d_t R, __read_only image2d_t G, __read_only image2d_t B,                   \n" \
"__read_only image2d_t R_lsb, __read_only image2d_t G_lsb, __read_only image2d_t B_lsb,                           \n" \
"__write_only image2d_t U1, const int2 dim) {                                                                     \n" \
"                                                                                                                 \n" \
"   int x = get_global_id(0);                                                                                     \n" \
"   int y = get_global_id(1);                                                                                     \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;                    \n" \
"   int2 p = (int2) (x, y);                                                                                       \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_LUMA)) {                                                 \n" \
"       float y     = norm(read_imageui(R, smp, p).x);                                                            \n" \
"       write_imagef(U1, p, (float4) (y, 0.0f, 0.0f, 0.0f));                                                      \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_LUMA)) {                                           \n" \
"       float y_msb = convert_float(read_imageui(R,     smp, p).x);                                               \n" \
"       float y_lsb = convert_float(read_imageui(R_lsb, smp, p).x);                                               \n" \
"       float y     = NLM_16BIT_MSB * y_msb + NLM_16BIT_LSB * y_lsb;                                              \n" \
"       write_imagef(U1, p, (float4) (y, 0.0f, 0.0f, 0.0f));                                                      \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA)) {                                           \n" \
"       float u     = read_imagef(R, smp, p).x;                                                                   \n" \
"       float v     = read_imagef(G, smp, p).x;                                                                   \n" \
"       write_imagef(U1, p, (float4) (u, v, 0.0f, 0.0f));                                                         \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_CHROMA)) {                                        \n" \
"       float u     = norm(read_imageui(R, smp, p).x);                                                            \n" \
"       float v     = norm(read_imageui(G, smp, p).x);                                                            \n" \
"       write_imagef(U1, p, (float4) (u, v, 0.0f, 0.0f));                                                         \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_CHROMA)) {                                         \n" \
"       float u_msb = convert_float(read_imageui(R,     smp, p).x);                                               \n" \
"       float v_msb = convert_float(read_imageui(G,     smp, p).x);                                               \n" \
"       float u_lsb = convert_float(read_imageui(R_lsb, smp, p).x);                                               \n" \
"       float v_lsb = convert_float(read_imageui(G_lsb, smp, p).x);                                               \n" \
"       float u     = NLM_16BIT_MSB * u_msb + NLM_16BIT_LSB * u_lsb;                                              \n" \
"       float v     = NLM_16BIT_MSB * v_msb + NLM_16BIT_LSB * v_lsb;                                              \n" \
"       write_imagef(U1, p, (float4) (u, v, 0.0f, 0.0f));                                                         \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV)) {                                              \n" \
"       float y     = read_imagef(R, smp, p).x;                                                                   \n" \
"       float u     = read_imagef(G, smp, p).x;                                                                   \n" \
"       float v     = read_imagef(B, smp, p).x;                                                                   \n" \
"       write_imagef(U1, p, (float4) (y, u, v, 0.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV)) {                                           \n" \
"       float y     = norm(read_imageui(R, smp, p).x);                                                            \n" \
"       float u     = norm(read_imageui(G, smp, p).x);                                                            \n" \
"       float v     = norm(read_imageui(B, smp, p).x);                                                            \n" \
"       write_imagef(U1, p, (float4) (y, u, v, 0.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_YUV)) {                                            \n" \
"       float y_msb = convert_float(read_imageui(R,     smp, p).x);                                               \n" \
"       float u_msb = convert_float(read_imageui(G,     smp, p).x);                                               \n" \
"       float v_msb = convert_float(read_imageui(B,     smp, p).x);                                               \n" \
"       float y_lsb = convert_float(read_imageui(R_lsb, smp, p).x);                                               \n" \
"       float u_lsb = convert_float(read_imageui(G_lsb, smp, p).x);                                               \n" \
"       float v_lsb = convert_float(read_imageui(B_lsb, smp, p).x);                                               \n" \
"       float y     = NLM_16BIT_MSB * y_msb + NLM_16BIT_LSB * y_lsb;                                              \n" \
"       float u     = NLM_16BIT_MSB * u_msb + NLM_16BIT_LSB * u_lsb;                                              \n" \
"       float v     = NLM_16BIT_MSB * v_msb + NLM_16BIT_LSB * v_lsb;                                              \n" \
"       write_imagef(U1, p, (float4) (y, u, v, 0.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB)) {                                              \n" \
"       float r     = read_imagef(R, smp, p).x;                                                                   \n" \
"       float g     = read_imagef(G, smp, p).x;                                                                   \n" \
"       float b     = read_imagef(B, smp, p).x;                                                                   \n" \
"       write_imagef(U1, p, (float4) (r, g, b, 0.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB)) {                                           \n" \
"       float r     = norm(read_imageui(R, smp, p).x);                                                            \n" \
"       float g     = norm(read_imageui(G, smp, p).x);                                                            \n" \
"       float b     = norm(read_imageui(B, smp, p).x);                                                            \n" \
"       write_imagef(U1, p, (float4) (r, g, b, 0.0f));                                                            \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmSpatialUnpack(__write_only image2d_t R, __write_only image2d_t G, __write_only image2d_t B,              \n" \
"__write_only image2d_t R_lsb, __write_only image2d_t G_lsb, __write_only image2d_t B_lsb,                        \n" \
"__read_only image2d_t U1, const int2 dim) {                                                                      \n" \
"                                                                                                                 \n" \
"   int x = get_global_id(0);                                                                                     \n" \
"   int y = get_global_id(1);                                                                                     \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;                    \n" \
"   int2 s = (int2) (x, y);                                                                                       \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_LUMA)) {                                                 \n" \
"       ushort y   = denorm(read_imagef(U1, smp, s).x);                                                           \n" \
"       write_imageui(R,     s, (uint4) y);                                                                       \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_LUMA)) {                                           \n" \
"       float  val = read_imagef(U1, smp, s).x;                                                                   \n" \
"       ushort y   = denorm(val);                                                                                 \n" \
"       write_imageui(R,     s, (uint4)  (y >> CHAR_BIT, 0u, 0u, 0u));                                            \n" \
"       write_imageui(R_lsb, s, (uint4)  (y &  0xFF,     0u, 0u, 0u));                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA)) {                                           \n" \
"       float2 val = read_imagef(U1, smp, s).xy;                                                                  \n" \
"       write_imagef(R,      s, (float4) (val.x, 0.0f, 0.0f, 0.0f));                                              \n" \
"       write_imagef(G,      s, (float4) (val.y, 0.0f, 0.0f, 0.0f));                                              \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_CHROMA)) {                                        \n" \
"       float2 val = read_imagef(U1, smp, s).xy;                                                                  \n" \
"       write_imageui(R,     s, (uint4)  (denorm(val.x), 0u, 0u, 0u));                                            \n" \
"       write_imageui(G,     s, (uint4)  (denorm(val.y), 0u, 0u, 0u));                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_CHROMA)) {                                         \n" \
"       float2 val = read_imagef(U1, smp, s).xy;                                                                  \n" \
"       ushort u   = denorm(val.x);                                                                               \n" \
"       ushort v   = denorm(val.y);                                                                               \n" \
"       write_imageui(R,     s, (uint4)  (u >> CHAR_BIT, 0u, 0u, 0u));                                            \n" \
"       write_imageui(G,     s, (uint4)  (v >> CHAR_BIT, 0u, 0u, 0u));                                            \n" \
"       write_imageui(R_lsb, s, (uint4)  (u &  0xFF,     0u, 0u, 0u));                                            \n" \
"       write_imageui(G_lsb, s, (uint4)  (v &  0xFF,     0u, 0u, 0u));                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV)) {                                              \n" \
"       float3 val = read_imagef(U1, smp, s).xyz;                                                                 \n" \
"       write_imagef(R,      s, (float4) (val.x, 0.0f, 0.0f, 0.0f));                                              \n" \
"       write_imagef(G,      s, (float4) (val.y, 0.0f, 0.0f, 0.0f));                                              \n" \
"       write_imagef(B,      s, (float4) (val.z, 0.0f, 0.0f, 0.0f));                                              \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV)) {                                           \n" \
"       float3 val = read_imagef(U1, smp, s).xyz;                                                                 \n" \
"       write_imageui(R,     s, (uint4)  (denorm(val.x), 0u, 0u, 0u));                                            \n" \
"       write_imageui(G,     s, (uint4)  (denorm(val.y), 0u, 0u, 0u));                                            \n" \
"       write_imageui(B,     s, (uint4)  (denorm(val.z), 0u, 0u, 0u));                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_YUV)) {                                            \n" \
"       float3 val = read_imagef(U1, smp, s).xyz;                                                                 \n" \
"       ushort y   = denorm(val.x);                                                                               \n" \
"       ushort u   = denorm(val.y);                                                                               \n" \
"       ushort v   = denorm(val.z);                                                                               \n" \
"       write_imageui(R,     s, (uint4)  (y >> CHAR_BIT, 0u, 0u, 0u));                                            \n" \
"       write_imageui(G,     s, (uint4)  (u >> CHAR_BIT, 0u, 0u, 0u));                                            \n" \
"       write_imageui(B,     s, (uint4)  (v >> CHAR_BIT, 0u, 0u, 0u));                                            \n" \
"       write_imageui(R_lsb, s, (uint4)  (y &  0xFF,     0u, 0u, 0u));                                            \n" \
"       write_imageui(G_lsb, s, (uint4)  (u &  0xFF,     0u, 0u, 0u));                                            \n" \
"       write_imageui(B_lsb, s, (uint4)  (v &  0xFF,     0u, 0u, 0u));                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB)) {                                              \n" \
"       float3  val    = read_imagef(U1, smp, s).xyz;                                                             \n" \
"       write_imagef(R,      s, (float4) (val.x, 0.0f, 0.0f, 0.0f));                                              \n" \
"       write_imagef(G,      s, (float4) (val.y, 0.0f, 0.0f, 0.0f));                                              \n" \
"       write_imagef(B,      s, (float4) (val.z, 0.0f, 0.0f, 0.0f));                                              \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB)) {                                           \n" \
"       float3  val    = read_imagef(U1, smp, s).xyz;                                                             \n" \
"       write_imageui(R,     s, (uint4)  (denorm(val.x), 0u, 0u, 0u));                                            \n" \
"       write_imageui(G,     s, (uint4)  (denorm(val.y), 0u, 0u, 0u));                                            \n" \
"       write_imageui(B,     s, (uint4)  (denorm(val.z), 0u, 0u, 0u));                                            \n" \
"   }                                                                                                             \n" \
"}                                                                                                                ";

static const char* kernel_source_code =
"#define NLM_16BIT_MSB    ( 256.0f / 65535.0f )                                                                   \n" \
"#define NLM_16BIT_LSB    (   1.0f / 65535.0f )                                                                   \n" \
"#define CHECK_FLAG(flag) ((NLM_TCLIP & (flag)) == (flag))                                                        \n" \
"                                                                                                                 \n" \
"float   norm   (uint   u);                                                                                       \n" \
"ushort  denorm (float  f);                                                                                       \n" \
"                                                                                                                 \n" \
"float   norm   (uint   u) { return native_divide(convert_float(u), NLM_UNORM_MAX); }                             \n" \
"ushort  denorm (float  f) { return convert_ushort_sat(f *          NLM_UNORM_MAX); }                             \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmDistanceLeft(__read_only image2d_array_t U1, __write_only image2d_array_t U4, const int2 dim,            \n" \
"const int4 q) {                                                                                                  \n" \
"                                                                                                                 \n" \
"   int x = get_global_id(0);                                                                                     \n" \
"   int y = get_global_id(1);                                                                                     \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"   int4 p = (int4) (x, y, NLM_D, 0);                                                                             \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_CLIP_REF_LUMA)) {                                                                          \n" \
"                                                                                                                 \n" \
"       float  u1    = read_imagef(U1, smp, p    ).x;                                                             \n" \
"       float  u1_pq = read_imagef(U1, smp, p + q).x;                                                             \n" \
"       float  dst   = (u1 - u1_pq) * (u1 - u1_pq);                                                               \n" \
"       float  val   = 3.0f * dst;                                                                                \n" \
"       write_imagef(U4, p, (float4) (val, 0.0f, 0.0f, 0.0f));                                                    \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_CHROMA)) {                                                                 \n" \
"                                                                                                                 \n" \
"       float2 u1    = read_imagef(U1, smp, p    ).xy;                                                            \n" \
"       float2 u1_pq = read_imagef(U1, smp, p + q).xy;                                                            \n" \
"       float  dst_u = (u1.x - u1_pq.x) * (u1.x - u1_pq.x);                                                       \n" \
"       float  dst_v = (u1.y - u1_pq.y) * (u1.y - u1_pq.y);                                                       \n" \
"       float  val   = 1.5f * (dst_u + dst_v);                                                                    \n" \
"       write_imagef(U4, p, (float4) (val, 0.0f, 0.0f, 0.0f));                                                    \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_YUV)) {                                                                    \n" \
"                                                                                                                 \n" \
"       float3 u1    = read_imagef(U1, smp, p    ).xyz;                                                           \n" \
"       float3 u1_pq = read_imagef(U1, smp, p + q).xyz;                                                           \n" \
"       float  dst_y = (u1.x - u1_pq.x) * (u1.x - u1_pq.x);                                                       \n" \
"       float  dst_u = (u1.y - u1_pq.y) * (u1.y - u1_pq.y);                                                       \n" \
"       float  dst_v = (u1.z - u1_pq.z) * (u1.z - u1_pq.z);                                                       \n" \
"       float  val   = dst_y + dst_u + dst_v;                                                                     \n" \
"       write_imagef(U4, p, (float4) (val, 0.0f, 0.0f, 0.0f));                                                    \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_RGB)) {                                                                    \n" \
"                                                                                                                 \n" \
"       float3 u1    = read_imagef(U1, smp, p    ).xyz;                                                           \n" \
"       float3 u1_pq = read_imagef(U1, smp, p + q).xyz;                                                           \n" \
"       float  m_red = native_divide(u1.x + u1_pq.x, 6.0f);                                                       \n" \
"       float  dst_r = (2.0f/3.0f + m_red) * (u1.x - u1_pq.x) * (u1.x - u1_pq.x);                                 \n" \
"       float  dst_g = (4.0f/3.0f        ) * (u1.y - u1_pq.y) * (u1.y - u1_pq.y);                                 \n" \
"       float  dst_b = (     1.0f - m_red) * (u1.z - u1_pq.z) * (u1.z - u1_pq.z);                                 \n" \
"       float  val   = dst_r + dst_g + dst_b;                                                                     \n" \
"       write_imagef(U4, p, (float4) (val, 0.0f, 0.0f, 0.0f));                                                    \n" \
"                                                                                                                 \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmDistanceRight(__read_only image2d_array_t U1, __write_only image2d_array_t U4, const int2 dim,           \n" \
"const int4 q) {                                                                                                  \n" \
"                                                                                                                 \n" \
"   int x = get_global_id(0);                                                                                     \n" \
"   int y = get_global_id(1);                                                                                     \n" \
"   if((x - q.x) < 0 || (x - q.x) >= dim.x || (y - q.y) < 0 || (y - q.y) >= dim.y) return;                        \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"   int4 p = (int4) (x, y, NLM_D, 0);                                                                             \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_CLIP_REF_LUMA)) {                                                                          \n" \
"                                                                                                                 \n" \
"       float  u1    = read_imagef(U1, smp, p    ).x;                                                             \n" \
"       float  u1_mq = read_imagef(U1, smp, p - q).x;                                                             \n" \
"       float  dst   = (u1 - u1_mq) * (u1 - u1_mq);                                                               \n" \
"       float  val   = 3.0f * dst;                                                                                \n" \
"       write_imagef(U4, p - q, (float4) (val, 0.0f, 0.0f, 0.0f));                                                \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_CHROMA)) {                                                                 \n" \
"                                                                                                                 \n" \
"       float2 u1    = read_imagef(U1, smp, p    ).xy;                                                            \n" \
"       float2 u1_mq = read_imagef(U1, smp, p - q).xy;                                                            \n" \
"       float  dst_u = (u1.x - u1_mq.x) * (u1.x - u1_mq.x);                                                       \n" \
"       float  dst_v = (u1.y - u1_mq.y) * (u1.y - u1_mq.y);                                                       \n" \
"       float  val   = 1.5f * (dst_u + dst_v);                                                                    \n" \
"       write_imagef(U4, p - q, (float4) (val, 0.0f, 0.0f, 0.0f));                                                \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_YUV)) {                                                                    \n" \
"                                                                                                                 \n" \
"       float3 u1    = read_imagef(U1, smp, p    ).xyz;                                                           \n" \
"       float3 u1_mq = read_imagef(U1, smp, p - q).xyz;                                                           \n" \
"       float  dst_y  = (u1.x - u1_mq.x) * (u1.x - u1_mq.x);                                                      \n" \
"       float  dst_u  = (u1.y - u1_mq.y) * (u1.y - u1_mq.y);                                                      \n" \
"       float  dst_v  = (u1.z - u1_mq.z) * (u1.z - u1_mq.z);                                                      \n" \
"       float  val    = dst_y + dst_u + dst_v;                                                                    \n" \
"       write_imagef(U4, p - q, (float4) (val, 0.0f, 0.0f, 0.0f));                                                \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_RGB)) {                                                                    \n" \
"                                                                                                                 \n" \
"       float3 u1    = read_imagef(U1, smp, p    ).xyz;                                                           \n" \
"       float3 u1_mq = read_imagef(U1, smp, p - q).xyz;                                                           \n" \
"       float  m_red = native_divide(u1.x + u1_mq.x, 6.0f);                                                       \n" \
"       float  dst_r = (2.0f/3.0f + m_red) * (u1.x - u1_mq.x) * (u1.x - u1_mq.x);                                 \n" \
"       float  dst_g = (4.0f/3.0f        ) * (u1.y - u1_mq.y) * (u1.y - u1_mq.y);                                 \n" \
"       float  dst_b = (     1.0f - m_red) * (u1.z - u1_mq.z) * (u1.z - u1_mq.z);                                 \n" \
"       float  val    = dst_r + dst_g + dst_b;                                                                    \n" \
"       write_imagef(U4, p - q, (float4) (val, 0.0f, 0.0f, 0.0f));                                                \n" \
"                                                                                                                 \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel __attribute__((reqd_work_group_size(HRZ_BLOCK_X, HRZ_BLOCK_Y, 1)))                                      \n" \
"void nlmHorizontal(__read_only image2d_array_t U4_in, __write_only image2d_array_t U4_out,                       \n" \
"const int t, const int2 dim) {                                                                                   \n" \
"                                                                                                                 \n" \
"   __local float buffer[HRZ_BLOCK_Y][(HRZ_RESULT + 2) * HRZ_BLOCK_X];                                            \n" \
"   int x = (get_group_id(0) * HRZ_RESULT - 1) * HRZ_BLOCK_X + get_local_id(0);                                   \n" \
"   int y = get_group_id(1) * HRZ_BLOCK_Y + get_local_id(1);                                                      \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"                                                                                                                 \n" \
"   for (int i = 1; i < 1 + HRZ_RESULT; i++)                                                                      \n" \
"       buffer[get_local_id(1)][get_local_id(0) + i * HRZ_BLOCK_X] =                                              \n" \
"           read_imagef(U4_in, smp, (int4) (x + i * HRZ_BLOCK_X, y, t, 0)).x;                                     \n" \
"                                                                                                                 \n" \
"   buffer[get_local_id(1)][get_local_id(0)] =                                                                    \n" \
"       read_imagef(U4_in, smp, (int4) (x, y, t, 0)).x;                                                           \n" \
"                                                                                                                 \n" \
"   buffer[get_local_id(1)][get_local_id(0) + (1 + HRZ_RESULT) * HRZ_BLOCK_X] =                                   \n" \
"       read_imagef(U4_in, smp, (int4) (x + (1 + HRZ_RESULT) * HRZ_BLOCK_X, y, t, 0)).x;                          \n" \
"                                                                                                                 \n" \
"   barrier(CLK_LOCAL_MEM_FENCE);                                                                                 \n" \
"                                                                                                                 \n" \
"   for (int i = 1; i < 1 + HRZ_RESULT; i++) {                                                                    \n" \
"       if ((x + i * HRZ_BLOCK_X) >= dim.x || y >= dim.y) return;                                                 \n" \
"       float sum = 0.0f;                                                                                         \n" \
"       for (int j = -NLM_S; j <= NLM_S; j++)                                                                     \n" \
"           sum += buffer[get_local_id(1)][get_local_id(0) + i * HRZ_BLOCK_X + j];                                \n" \
"                                                                                                                 \n" \
"       write_imagef(U4_out, (int4) (x + i * HRZ_BLOCK_X, y, t, 0), (float4) (sum, 0.0f, 0.0f, 0.0f));            \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel __attribute__((reqd_work_group_size(VRT_BLOCK_X, VRT_BLOCK_Y, 1)))                                      \n" \
"void nlmVertical(__read_only image2d_array_t U4_in, __write_only image2d_array_t U4_out,                         \n" \
"const int t, const int2 dim) {                                                                                   \n" \
"                                                                                                                 \n" \
"   __local float buffer[VRT_BLOCK_X][(VRT_RESULT + 2) * VRT_BLOCK_Y + 1];                                        \n" \
"   int x = get_group_id(0) * VRT_BLOCK_X + get_local_id(0);                                                      \n" \
"   int y = (get_group_id(1) * VRT_RESULT - 1) * VRT_BLOCK_Y + get_local_id(1);                                   \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"                                                                                                                 \n" \
"   for (int i = 1; i < 1 + VRT_RESULT; i++)                                                                      \n" \
"       buffer[get_local_id(0)][get_local_id(1) + i * VRT_BLOCK_Y] =                                              \n" \
"           read_imagef(U4_in, smp, (int4) (x, y + i * VRT_BLOCK_Y, t, 0)).x;                                     \n" \
"                                                                                                                 \n" \
"   buffer[get_local_id(0)][get_local_id(1)] =                                                                    \n" \
"       read_imagef(U4_in, smp, (int4) (x, y, t, 0)).x;                                                           \n" \
"                                                                                                                 \n" \
"   buffer[get_local_id(0)][get_local_id(1) + (1 + VRT_RESULT) * VRT_BLOCK_Y] =                                   \n" \
"       read_imagef(U4_in, smp, (int4) (x, y + (1 + VRT_RESULT) * VRT_BLOCK_Y, t, 0)).x;                          \n" \
"                                                                                                                 \n" \
"   barrier(CLK_LOCAL_MEM_FENCE);                                                                                 \n" \
"                                                                                                                 \n" \
"   for (int i = 1; i < 1 + VRT_RESULT; i++) {                                                                    \n" \
"       if (x >= dim.x || (y + i * VRT_BLOCK_Y) >= dim.y) return;                                                 \n" \
"       float sum = 0.0f;                                                                                         \n" \
"       for (int j = -NLM_S; j <= NLM_S; j++)                                                                     \n" \
"           sum += buffer[get_local_id(0)][get_local_id(1) + i * VRT_BLOCK_Y + j];                                \n" \
"                                                                                                                 \n" \
"       float val = 0.0f;                                                                                         \n" \
"       if (NLM_WMODE == NLM_WMODE_CAUCHY) {                                                                      \n" \
"           val = native_recip(1.0f + sum * NLM_H2_INV_NORM);                                                     \n" \
"       } else if (NLM_WMODE == NLM_WMODE_WELSCH) {                                                               \n" \
"           val = native_exp(- sum * NLM_H2_INV_NORM);                                                            \n" \
"       } else if (NLM_WMODE == NLM_WMODE_BISQUARE) {                                                             \n" \
"           val = pown(fdim(1.0f, sum * NLM_H2_INV_NORM), 2);                                                     \n" \
"       } else if (NLM_WMODE == NLM_WMODE_MOD_BISQUARE) {                                                         \n" \
"           val = pown(fdim(1.0f, sum * NLM_H2_INV_NORM), 8);                                                     \n" \
"       }                                                                                                         \n" \
"       write_imagef(U4_out, (int4) (x, y + i * VRT_BLOCK_Y, t, 0), (float4) (val, 0.0f, 0.0f, 0.0f));            \n" \
"   }                                                                                                             \n" \ 
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmAccumulation(__read_only image2d_array_t U1, __global void* U2, __read_only image2d_array_t U4,          \n" \
"__global float* M, const int2 dim, const int4 q) {                                                               \n" \
"                                                                                                                 \n" \
"   int x = get_global_id(0);                                                                                     \n" \
"   int y = get_global_id(1);                                                                                     \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"   int4 p = (int4) (x, y, NLM_D, 0);                                                                             \n" \
"   int gidx = mad24(y, dim.x, x);                                                                                \n" \
"                                                                                                                 \n" \
"   float u4    = read_imagef(U4, smp, p    ).x;                                                                  \n" \
"   float u4_mq = read_imagef(U4, smp, p - q).x;                                                                  \n" \
"   M[gidx] = fmax(M[gidx], fmax(u4, u4_mq));                                                                     \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_CLIP_REF_LUMA)) {                                                                          \n" \
"                                                                                                                 \n" \
"       __global float2* U2c = (__global float2*) U2;                                                             \n" \
"       float  u1_pq = read_imagef(U1, smp, p + q).x;                                                             \n" \
"       float  u1_mq = read_imagef(U1, smp, p - q).x;                                                             \n" \
"       float  acc   = (u4 * u1_pq) + (u4_mq * u1_mq);                                                            \n" \
"       float  sum   = (u4 + u4_mq);                                                                              \n" \
"       U2c[gidx] += (float2) (acc, sum);                                                                         \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_CHROMA)) {                                                                 \n" \
"                                                                                                                 \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"       float2 u1_pq = read_imagef(U1, smp, p + q).xy;                                                            \n" \
"       float2 u1_mq = read_imagef(U1, smp, p - q).xy;                                                            \n" \
"       float  acc_u = (u4 * u1_pq.x) + (u4_mq * u1_mq.x);                                                        \n" \
"       float  acc_v = (u4 * u1_pq.y) + (u4_mq * u1_mq.y);                                                        \n" \
"       float  sum   = (u4 + u4_mq);                                                                              \n" \
"       U2c[gidx] += (float4) (acc_u, acc_v, sum, 0.0f);                                                          \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_YUV)) {                                                                    \n" \
"                                                                                                                 \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"       float3 u1_pq = read_imagef(U1, smp, p + q).xyz;                                                           \n" \
"       float3 u1_mq = read_imagef(U1, smp, p - q).xyz;                                                           \n" \
"       float  acc_y = (u4 * u1_pq.x) + (u4_mq * u1_mq.x);                                                        \n" \
"       float  acc_u = (u4 * u1_pq.y) + (u4_mq * u1_mq.y);                                                        \n" \
"       float  acc_v = (u4 * u1_pq.z) + (u4_mq * u1_mq.z);                                                        \n" \
"       float  sum   = (u4 + u4_mq);                                                                              \n" \
"       U2c[gidx] += (float4) (acc_y, acc_u, acc_v, sum);                                                         \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_RGB)) {                                                                    \n" \
"                                                                                                                 \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"       float3 u1_pq = read_imagef(U1, smp, p + q).xyz;                                                           \n" \
"       float3 u1_mq = read_imagef(U1, smp, p - q).xyz;                                                           \n" \
"       float  acc_r = (u4 * u1_pq.x) + (u4_mq * u1_mq.x);                                                        \n" \
"       float  acc_g = (u4 * u1_pq.y) + (u4_mq * u1_mq.y);                                                        \n" \
"       float  acc_b = (u4 * u1_pq.z) + (u4_mq * u1_mq.z);                                                        \n" \
"       float  sum   = (u4 + u4_mq);                                                                              \n" \
"       U2c[gidx] += (float4) (acc_r, acc_g, acc_b, sum);                                                         \n" \
"                                                                                                                 \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmFinish(__read_only image2d_array_t U1_in, __write_only image2d_t U1_out, __global void* U2,              \n" \
"__global float* M, const int2 dim) {                                                                             \n" \
"                                                                                                                 \n" \
"   int x = get_global_id(0);                                                                                     \n" \
"   int y = get_global_id(1);                                                                                     \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;                    \n" \
"   int4 p = (int4) (x, y, NLM_D, 0);                                                                             \n" \
"   int2 s = (int2) (x, y);                                                                                       \n" \
"   int gidx = mad24(y, dim.x, x);                                                                                \n" \
"   float wM = NLM_WREF * M[gidx];                                                                                \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_CLIP_REF_LUMA)) {                                                                          \n" \
"                                                                                                                 \n" \
"       __global float2* U2c = (__global float2*) U2;                                                             \n" \
"       float  u1    = read_imagef(U1_in, smp, p).x;                                                              \n" \
"       float  num   = U2c[gidx].x + wM * u1;                                                                     \n" \
"       float  den   = U2c[gidx].y + wM;                                                                          \n" \
"       float  val   = native_divide(num, den);                                                                   \n" \
"       write_imagef(U1_out, s, (float4) (val, 0.0f, 0.0f, 0.0f));                                                \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_CHROMA)) {                                                                 \n" \
"                                                                                                                 \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"       float2 u1    = read_imagef(U1_in, smp, p).xy;                                                             \n" \
"       float  num_u = U2c[gidx].x + wM * u1.x;                                                                   \n" \
"       float  num_v = U2c[gidx].y + wM * u1.y;                                                                   \n" \
"       float  den   = U2c[gidx].z + wM;                                                                          \n" \
"       float  val_u = native_divide(num_u, den);                                                                 \n" \
"       float  val_v = native_divide(num_v, den);                                                                 \n" \
"       write_imagef(U1_out, s,  (float4) (val_u, val_v, 0.0f, 0.0f));                                            \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_YUV)) {                                                                    \n" \
"                                                                                                                 \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"       float3 u1    = read_imagef(U1_in, smp, p).xyz;                                                            \n" \
"       float  num_y = U2c[gidx].x + wM * u1.x;                                                                   \n" \
"       float  num_u = U2c[gidx].y + wM * u1.y;                                                                   \n" \
"       float  num_v = U2c[gidx].z + wM * u1.z;                                                                   \n" \
"       float  den   = U2c[gidx].w + wM;                                                                          \n" \
"       float  val_y = native_divide(num_y, den);                                                                 \n" \
"       float  val_u = native_divide(num_u, den);                                                                 \n" \
"       float  val_v = native_divide(num_v, den);                                                                 \n" \
"       write_imagef(U1_out, s,  (float4) (val_y, val_u, val_v, 0.0f));                                           \n" \
"                                                                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_REF_RGB)) {                                                                    \n" \
"                                                                                                                 \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"       float3 u1    = read_imagef(U1_in, smp, p).xyz;                                                            \n" \
"       float  num_r = U2c[gidx].x + wM * u1.x;                                                                   \n" \
"       float  num_g = U2c[gidx].y + wM * u1.y;                                                                   \n" \
"       float  num_b = U2c[gidx].z + wM * u1.z;                                                                   \n" \
"       float  den   = U2c[gidx].w + wM;                                                                          \n" \
"       float  val_r = native_divide(num_r, den);                                                                 \n" \
"       float  val_g = native_divide(num_g, den);                                                                 \n" \
"       float  val_b = native_divide(num_b, den);                                                                 \n" \
"       write_imagef(U1_out, s,  (float4) (val_r, val_g, val_b, 0.0f));                                           \n" \
"                                                                                                                 \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmPack(__read_only image2d_t R, __read_only image2d_t G, __read_only image2d_t B,                          \n" \
"__read_only image2d_t R_lsb, __read_only image2d_t G_lsb, __read_only image2d_t B_lsb,                           \n" \
"__write_only image2d_array_t U1, const int t, const int2 dim) {                                                  \n" \
"                                                                                                                 \n" \
"   const int x = get_global_id(0);                                                                               \n" \
"   const int y = get_global_id(1);                                                                               \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;                    \n" \
"   const int4 p = (int4) (x, y, t, 0);                                                                           \n" \
"   const int2 s = (int2) (x, y);                                                                                 \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_LUMA)) {                                                 \n" \
"       float y     = norm(read_imageui(R, smp, s).x);                                                            \n" \
"       write_imagef(U1, p, (float4) (y, 0.0f, 0.0f, 0.0f));                                                      \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_LUMA)) {                                           \n" \
"       float y_msb = convert_float(read_imageui(R,     smp, s).x);                                               \n" \
"       float y_lsb = convert_float(read_imageui(R_lsb, smp, s).x);                                               \n" \
"       float y     = NLM_16BIT_MSB * y_msb + NLM_16BIT_LSB * y_lsb;                                              \n" \
"       write_imagef(U1, p, (float4) (y, 0.0f, 0.0f, 0.0f));                                                      \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA)) {                                           \n" \
"       float u     = read_imagef(R, smp, s).x;                                                                   \n" \
"       float v     = read_imagef(G, smp, s).x;                                                                   \n" \
"       write_imagef(U1, p, (float4) (u, v, 0.0f, 0.0f));                                                         \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_CHROMA)) {                                        \n" \
"       float u     = norm(read_imageui(R, smp, s).x);                                                            \n" \
"       float v     = norm(read_imageui(G, smp, s).x);                                                            \n" \
"       write_imagef(U1, p, (float4) (u, v, 0.0f, 0.0f));                                                         \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_CHROMA)) {                                         \n" \
"       float u_msb = convert_float(read_imageui(R,     smp, s).x);                                               \n" \
"       float v_msb = convert_float(read_imageui(G,     smp, s).x);                                               \n" \
"       float u_lsb = convert_float(read_imageui(R_lsb, smp, s).x);                                               \n" \
"       float v_lsb = convert_float(read_imageui(G_lsb, smp, s).x);                                               \n" \
"       float u     = NLM_16BIT_MSB * u_msb + NLM_16BIT_LSB * u_lsb;                                              \n" \
"       float v     = NLM_16BIT_MSB * v_msb + NLM_16BIT_LSB * v_lsb;                                              \n" \
"       write_imagef(U1, p, (float4) (u, v, 0.0f, 0.0f));                                                         \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV)) {                                              \n" \
"       float y     = read_imagef(R, smp, s).x;                                                                   \n" \
"       float u     = read_imagef(G, smp, s).x;                                                                   \n" \
"       float v     = read_imagef(B, smp, s).x;                                                                   \n" \
"       write_imagef(U1, p, (float4) (y, u, v, 0.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV)) {                                           \n" \
"       float y     = norm(read_imageui(R, smp, s).x);                                                            \n" \
"       float u     = norm(read_imageui(G, smp, s).x);                                                            \n" \
"       float v     = norm(read_imageui(B, smp, s).x);                                                            \n" \
"       write_imagef(U1, p, (float4) (y, u, v, 0.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_YUV)) {                                            \n" \
"       float y_msb = convert_float(read_imageui(R,     smp, s).x);                                               \n" \
"       float u_msb = convert_float(read_imageui(G,     smp, s).x);                                               \n" \
"       float v_msb = convert_float(read_imageui(B,     smp, s).x);                                               \n" \
"       float y_lsb = convert_float(read_imageui(R_lsb, smp, s).x);                                               \n" \
"       float u_lsb = convert_float(read_imageui(G_lsb, smp, s).x);                                               \n" \
"       float v_lsb = convert_float(read_imageui(B_lsb, smp, s).x);                                               \n" \
"       float y     = NLM_16BIT_MSB * y_msb + NLM_16BIT_LSB * y_lsb;                                              \n" \
"       float u     = NLM_16BIT_MSB * u_msb + NLM_16BIT_LSB * u_lsb;                                              \n" \
"       float v     = NLM_16BIT_MSB * v_msb + NLM_16BIT_LSB * v_lsb;                                              \n" \
"       write_imagef(U1, p, (float4) (y, u, v, 0.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB)) {                                              \n" \
"       float r     = read_imagef(R, smp, s).x;                                                                   \n" \
"       float g     = read_imagef(G, smp, s).x;                                                                   \n" \
"       float b     = read_imagef(B, smp, s).x;                                                                   \n" \
"       write_imagef(U1, p, (float4) (r, g, b, 0.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB)) {                                           \n" \
"       float r     = norm(read_imageui(R, smp, s).x);                                                            \n" \
"       float g     = norm(read_imageui(G, smp, s).x);                                                            \n" \
"       float b     = norm(read_imageui(B, smp, s).x);                                                            \n" \
"       write_imagef(U1, p, (float4) (r, g, b, 0.0f));                                                            \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmUnpack(__write_only image2d_t R, __write_only image2d_t G, __write_only image2d_t B,                     \n" \
"__write_only image2d_t R_lsb, __write_only image2d_t G_lsb, __write_only image2d_t B_lsb,                        \n" \
"__read_only image2d_t U1, const int2 dim) {                                                                      \n" \
"                                                                                                                 \n" \
"   int x = get_global_id(0);                                                                                     \n" \
"   int y = get_global_id(1);                                                                                     \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;                    \n" \
"   int2 s = (int2) (x, y);                                                                                       \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_LUMA)) {                                                 \n" \
"       ushort y   = denorm(read_imagef(U1, smp, s).x);                                                           \n" \
"       write_imageui(R,     s, (uint4) y);                                                                       \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_LUMA)) {                                           \n" \
"       float  val = read_imagef(U1, smp, s).x;                                                                   \n" \
"       ushort y   = denorm(val);                                                                                 \n" \
"       write_imageui(R,     s, (uint4)  (y >> CHAR_BIT, 0u, 0u, 0u));                                            \n" \
"       write_imageui(R_lsb, s, (uint4)  (y &  0xFF,     0u, 0u, 0u));                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_CHROMA)) {                                           \n" \
"       float2 val = read_imagef(U1, smp, s).xy;                                                                  \n" \
"       write_imagef(R,      s, (float4) (val.x, 0.0f, 0.0f, 0.0f));                                              \n" \
"       write_imagef(G,      s, (float4) (val.y, 0.0f, 0.0f, 0.0f));                                              \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_CHROMA)) {                                        \n" \
"       float2 val = read_imagef(U1, smp, s).xy;                                                                  \n" \
"       write_imageui(R,     s, (uint4)  (denorm(val.x), 0u, 0u, 0u));                                            \n" \
"       write_imageui(G,     s, (uint4)  (denorm(val.y), 0u, 0u, 0u));                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_CHROMA)) {                                         \n" \
"       float2 val = read_imagef(U1, smp, s).xy;                                                                  \n" \
"       ushort u   = denorm(val.x);                                                                               \n" \
"       ushort v   = denorm(val.y);                                                                               \n" \
"       write_imageui(R,     s, (uint4)  (u >> CHAR_BIT, 0u, 0u, 0u));                                            \n" \
"       write_imageui(G,     s, (uint4)  (v >> CHAR_BIT, 0u, 0u, 0u));                                            \n" \
"       write_imageui(R_lsb, s, (uint4)  (u &  0xFF,     0u, 0u, 0u));                                            \n" \
"       write_imageui(G_lsb, s, (uint4)  (v &  0xFF,     0u, 0u, 0u));                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_YUV)) {                                              \n" \
"       float3 val = read_imagef(U1, smp, s).xyz;                                                                 \n" \
"       write_imagef(R,      s, (float4) (val.x, 0.0f, 0.0f, 0.0f));                                              \n" \
"       write_imagef(G,      s, (float4) (val.y, 0.0f, 0.0f, 0.0f));                                              \n" \
"       write_imagef(B,      s, (float4) (val.z, 0.0f, 0.0f, 0.0f));                                              \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_YUV)) {                                           \n" \
"       float3 val = read_imagef(U1, smp, s).xyz;                                                                 \n" \
"       write_imageui(R,     s, (uint4)  (denorm(val.x), 0u, 0u, 0u));                                            \n" \
"       write_imageui(G,     s, (uint4)  (denorm(val.y), 0u, 0u, 0u));                                            \n" \
"       write_imageui(B,     s, (uint4)  (denorm(val.z), 0u, 0u, 0u));                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_STACKED | NLM_CLIP_REF_YUV)) {                                            \n" \
"       float3 val = read_imagef(U1, smp, s).xyz;                                                                 \n" \
"       ushort y   = denorm(val.x);                                                                               \n" \
"       ushort u   = denorm(val.y);                                                                               \n" \
"       ushort v   = denorm(val.z);                                                                               \n" \
"       write_imageui(R,     s, (uint4)  (y >> CHAR_BIT, 0u, 0u, 0u));                                            \n" \
"       write_imageui(G,     s, (uint4)  (u >> CHAR_BIT, 0u, 0u, 0u));                                            \n" \
"       write_imageui(B,     s, (uint4)  (v >> CHAR_BIT, 0u, 0u, 0u));                                            \n" \
"       write_imageui(R_lsb, s, (uint4)  (y &  0xFF,     0u, 0u, 0u));                                            \n" \
"       write_imageui(G_lsb, s, (uint4)  (u &  0xFF,     0u, 0u, 0u));                                            \n" \
"       write_imageui(B_lsb, s, (uint4)  (v &  0xFF,     0u, 0u, 0u));                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNORM | NLM_CLIP_REF_RGB)) {                                              \n" \
"       float3  val    = read_imagef(U1, smp, s).xyz;                                                             \n" \
"       write_imagef(R,      s, (float4) (val.x, 0.0f, 0.0f, 0.0f));                                              \n" \
"       write_imagef(G,      s, (float4) (val.y, 0.0f, 0.0f, 0.0f));                                              \n" \
"       write_imagef(B,      s, (float4) (val.z, 0.0f, 0.0f, 0.0f));                                              \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_TYPE_UNSIGNED | NLM_CLIP_REF_RGB)) {                                           \n" \
"       float3  val    = read_imagef(U1, smp, s).xyz;                                                             \n" \
"       write_imageui(R,     s, (uint4)  (denorm(val.x), 0u, 0u, 0u));                                            \n" \
"       write_imageui(G,     s, (uint4)  (denorm(val.y), 0u, 0u, 0u));                                            \n" \
"       write_imageui(B,     s, (uint4)  (denorm(val.z), 0u, 0u, 0u));                                            \n" \
"   }                                                                                                             \n" \
"}                                                                                                                ";
