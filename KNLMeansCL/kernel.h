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
#define NLM_COLOR_GRAY           (1 << 0)
#define NLM_COLOR_YUV            (1 << 1)
#define NLM_COLOR_RGB            (1 << 2)
#define NLM_CLIP_UNORM           (1 << 3)
#define NLM_CLIP_UNSIGNED        (1 << 4)
#define NLM_CLIP_STACKED         (1 << 5)
#define NLM_EXTRA_FALSE          (1 << 6)
#define NLM_EXTRA_TRUE           (1 << 7)

#define nlmSpatialDistance        0x0
#define nlmSpatialHorizontal      0x1
#define nlmSpatialVertical        0x2
#define nlmSpatialAccumulation    0x3
#define nlmSpatialFinish          0x4
#define nlmSpatialPack            0x5
#define nlmSpatialUnpack          0x6
#define nlmDistanceLeft           0x7
#define nlmDistanceRight          0x8
#define nlmHorizontal             0x9
#define nlmVertical               0xA
#define nlmAccumulation           0xB
#define nlmFinish                 0xC
#define nlmPack                   0xD
#define nlmUnpack                 0xE
#define NLM_NUMBER_KERNELS        0xF

#define NLM_RGBA_RED              0.6664827524
#define NLM_RGBA_GREEN            1.2866580779
#define NLM_RGBA_BLUE             1.0468591696
#define NLM_RGBA_ALPHA            0.0
#define NLM_16BIT_MSB             0.9961089494163424
#define NLM_16BIT_LSB             0.003891050583657588

//////////////////////////////////////////
// Kernel Definition
static const char* kernel_source_code_spatial =
"#define CHECK_FLAG(flag) ((NLM_TCLIP & (flag)) == (flag))                                                        \n" \
"                                                                                                                 \n" \
"float   norm(uint u);                                                                                            \n" \
"ushort  denorm(float f);                                                                                         \n" \
"ushort4 denorm4(float4 f);                                                                                       \n" \
"                                                                                                                 \n" \
"float   norm(uint u)      { return native_divide((float) (u << NLM_BIT_SHIFT), (float) USHRT_MAX); }             \n" \
"ushort  denorm(float f)   { return convert_ushort_sat(f * (float) USHRT_MAX) >> (ushort) NLM_BIT_SHIFT; }        \n" \
"ushort4 denorm4(float4 f) { return convert_ushort4_sat(f * (float4) USHRT_MAX) >> (ushort4) NLM_BIT_SHIFT; }     \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmSpatialDistance(__read_only image2d_t U1, __write_only image2d_t U4, const int2 dim, const int2 q) {     \n" \
"                                                                                                                 \n" \
"   const int x = get_global_id(0);                                                                               \n" \
"   const int y = get_global_id(1);                                                                               \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"   const int2 p = (int2) (x, y);                                                                                 \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_COLOR_GRAY)) {                                                                             \n" \
"       const float  u1    = read_imagef(U1, smp, p    ).x;                                                       \n" \
"       const float  u1_pq = read_imagef(U1, smp, p + q).x;                                                       \n" \
"       const float  val   = 3.0f * (u1 - u1_pq) * (u1 - u1_pq);                                                  \n" \
"       write_imagef(U4, p, (float4) val);                                                                        \n" \
"   } else if (CHECK_FLAG(NLM_COLOR_YUV)) {                                                                       \n" \
"       const float4 u1    = read_imagef(U1, smp, p    );                                                         \n" \
"       const float4 u1_pq = read_imagef(U1, smp, p + q);                                                         \n" \
"       const float4 dist  = (u1 - u1_pq) * (u1 - u1_pq);                                                         \n" \
"       const float  val   = dist.x + dist.y + dist.z;                                                            \n" \
"       write_imagef(U4, p, (float4) val);                                                                        \n" \
"   } else if (CHECK_FLAG(NLM_COLOR_RGB)) {                                                                       \n" \
"       const float4 u1    = read_imagef(U1, smp, p    );                                                         \n" \
"       const float4 u1_pq = read_imagef(U1, smp, p + q);                                                         \n" \
"       const float4 wgh   = (float4) (NLM_RGBA_RED, NLM_RGBA_GREEN, NLM_RGBA_BLUE, NLM_RGBA_ALPHA);              \n" \
"       const float4 dist  = wgh * (u1 - u1_pq) * (u1 - u1_pq);                                                   \n" \
"       const float  val   = dist.x + dist.y + dist.z;                                                            \n" \
"       write_imagef(U4, p, (float4) val);                                                                        \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel __attribute__((reqd_work_group_size(HRZ_BLOCK_X, HRZ_BLOCK_Y, 1)))                                      \n" \
"void nlmSpatialHorizontal(__read_only image2d_t U4_in, __write_only image2d_t U4_out, const int2 dim) {          \n" \
"                                                                                                                 \n" \
"   __local float buffer[HRZ_BLOCK_Y][3*HRZ_BLOCK_X];                                                             \n" \
"   const int x = get_global_id(0);                                                                               \n" \
"   const int y = get_global_id(1);                                                                               \n" \
"   const int lx = get_local_id(0);                                                                               \n" \
"   const int ly = get_local_id(1);                                                                               \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"   const int2 p = (int2) (x, y);                                                                                 \n" \
"                                                                                                                 \n" \
"   buffer[ly][lx + HRZ_BLOCK_X]   = read_imagef(U4_in, smp, p                          ).x;                      \n" \
"   buffer[ly][lx]                 = read_imagef(U4_in, smp, p - (int2) (HRZ_BLOCK_X, 0)).x;                      \n" \
"   buffer[ly][lx + 2*HRZ_BLOCK_X] = read_imagef(U4_in, smp, p + (int2) (HRZ_BLOCK_X, 0)).x;                      \n" \
"   barrier(CLK_LOCAL_MEM_FENCE);                                                                                 \n" \
"                                                                                                                 \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"   float sum = 0.0f;                                                                                             \n" \
"   for(int i = -NLM_S; i <= NLM_S; i++)                                                                          \n" \
"       sum += buffer[ly][lx + HRZ_BLOCK_X + i];                                                                  \n" \
"   write_imagef(U4_out, p, (float4) sum);                                                                        \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel __attribute__((reqd_work_group_size(VRT_BLOCK_X, VRT_BLOCK_Y, 1)))                                      \n" \
"void nlmSpatialVertical(__read_only image2d_t U4_in, __write_only image2d_t U4_out, const int2 dim) {            \n" \
"                                                                                                                 \n" \
"   __local float buffer[3*VRT_BLOCK_Y][VRT_BLOCK_X];                                                             \n" \
"   const int x = get_global_id(0);                                                                               \n" \
"   const int y = get_global_id(1);                                                                               \n" \
"   const int lx = get_local_id(0);                                                                               \n" \
"   const int ly = get_local_id(1);                                                                               \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"   const int2 p = (int2) (x, y);                                                                                 \n" \
"                                                                                                                 \n" \
"   buffer[ly + VRT_BLOCK_Y][lx]   = read_imagef(U4_in, smp, p                          ).x;                      \n" \
"   buffer[ly][lx]                 = read_imagef(U4_in, smp, p - (int2) (0, VRT_BLOCK_Y)).x;                      \n" \
"   buffer[ly + 2*VRT_BLOCK_Y][lx] = read_imagef(U4_in, smp, p + (int2) (0, VRT_BLOCK_Y)).x;                      \n" \
"   barrier(CLK_LOCAL_MEM_FENCE);                                                                                 \n" \
"                                                                                                                 \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"   float sum = 0.0f;                                                                                             \n" \
"   for(int j = -NLM_S; j <= NLM_S; j++)                                                                          \n" \
"       sum += buffer[ly + VRT_BLOCK_Y + j][lx];                                                                  \n" \
"                                                                                                                 \n" \
"   if(NLM_WMODE == 0) {                                                                                          \n" \
"       const float val = native_recip(1.0f + sum * NLM_H2_INV_NORM);                                             \n" \
"       write_imagef(U4_out, p, (float4) val);                                                                    \n" \
"   } else if (NLM_WMODE == 1) {                                                                                  \n" \
"       const float val = native_exp(- sum * NLM_H2_INV_NORM);                                                    \n" \
"       write_imagef(U4_out, p, (float4) val);                                                                    \n" \
"   } else if (NLM_WMODE == 2) {                                                                                  \n" \
"       const float val = pown(fdim(1.0f, sum * NLM_H2_INV_NORM), 2);                                             \n" \
"       write_imagef(U4_out, p, (float4) val);                                                                    \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmSpatialAccumulation(__read_only image2d_t U1, __global void* U2, __read_only image2d_t U4,               \n" \
"__global float* M, const int2 dim, const int2 q) {                                                               \n" \
"                                                                                                                 \n" \
"   const int x = get_global_id(0);                                                                               \n" \
"   const int y = get_global_id(1);                                                                               \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"   const int2 p = (int2) (x, y);                                                                                 \n" \
"   const int gidx = mad24(y, dim.x, x);                                                                          \n" \
"                                                                                                                 \n" \
"   const float u4    = read_imagef(U4, smp, p    ).x;                                                            \n" \
"   const float u4_mq = read_imagef(U4, smp, p - q).x;                                                            \n" \
"   M[gidx] = fmax(M[gidx], fmax(u4, u4_mq));                                                                     \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_COLOR_GRAY)) {                                                                             \n" \
"       __global float2* U2c = (__global float2*) U2;                                                             \n" \
"       const float u1_pq = read_imagef(U1, smp, p + q).x;                                                        \n" \
"       const float u1_mq = read_imagef(U1, smp, p - q).x;                                                        \n" \
"       float2 accu;                                                                                              \n" \
"              accu.x = (u4 * u1_pq) + (u4_mq * u1_mq);                                                           \n" \
"              accu.y = (u4 + u4_mq);                                                                             \n" \
"       U2c[gidx] += accu;                                                                                        \n" \
"   } else if (CHECK_FLAG(NLM_COLOR_YUV) | CHECK_FLAG(NLM_COLOR_RGB)) {                                           \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"       const float4 u1_pq = read_imagef(U1, smp, p + q);                                                         \n" \
"       const float4 u1_mq = read_imagef(U1, smp, p - q);                                                         \n" \
"       float4 accu   = (u4 * u1_pq) + (u4_mq * u1_mq);                                                           \n" \
"              accu.w = (u4 + u4_mq);                                                                             \n" \
"       U2c[gidx] += accu;                                                                                        \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmSpatialFinish(__read_only image2d_t U1_in, __write_only image2d_t U1_out, __global void* U2,             \n" \
"__global float* M, const int2 dim) {                                                                             \n" \
"                                                                                                                 \n" \
"   const int x = get_global_id(0);                                                                               \n" \
"   const int y = get_global_id(1);                                                                               \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;                    \n" \
"   const int2 p = (int2) (x, y);                                                                                 \n" \
"   const int gidx = mad24(y, dim.x, x);                                                                          \n" \
"   const float wM = NLM_WREF * M[gidx];                                                                          \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_COLOR_GRAY)) {                                                                             \n" \
"       __global float2* U2c = (__global float2*) U2;                                                             \n" \
"           const float  u1  = read_imagef(U1_in, smp, p).x;                                                      \n" \
"           const float  num = U2c[gidx].x + wM * u1;                                                             \n" \
"           const float  den = U2c[gidx].y + wM;                                                                  \n" \
"           const float  val = native_divide(num, den);                                                           \n" \
"           write_imagef(U1_out, p, (float4) val);                                                                \n" \
"   } else if (CHECK_FLAG(NLM_COLOR_YUV) | CHECK_FLAG(NLM_COLOR_RGB)) {                                           \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"           const float4 u1  = read_imagef(U1_in, smp, p);                                                        \n" \
"           const float4 num = U2c[gidx] + (float4) wM * u1;                                                      \n" \
"           const float  den = U2c[gidx].w + wM;                                                                  \n" \
"           const float4 val = native_divide(num, (float4) den);                                                  \n" \
"           write_imagef(U1_out, p, val);                                                                         \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmSpatialPack(__read_only image2d_t R, __read_only image2d_t G, __read_only image2d_t B,                   \n" \
"__write_only image2d_t U1, const int2 dim) {                                                                     \n" \
"                                                                                                                 \n" \
"   const int x = get_global_id(0);                                                                               \n" \
"   const int y = get_global_id(1);                                                                               \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;                    \n" \
"   const int2 p = (int2) (x, y);                                                                                 \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_CLIP_UNSIGNED | NLM_COLOR_GRAY)) {                                                         \n" \
"       const float r     = norm(read_imageui(R, smp, p).x);                                                      \n" \
"       write_imagef(U1, p, (float4) r);                                                                          \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_STACKED | NLM_COLOR_GRAY)) {                                                   \n" \
"       const int2  p_lsb = (int2) (x, y + dim.y);                                                                \n" \
"       const float r_msb = read_imagef(R, smp, p    ).x;                                                         \n" \
"       const float r_lsb = read_imagef(R, smp, p_lsb).x;                                                         \n" \
"       const float r     = NLM_16BIT_MSB * r_msb + NLM_16BIT_LSB * r_lsb;                                        \n" \
"       write_imagef(U1, p, (float4) r);                                                                          \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNORM | NLM_COLOR_YUV)) {                                                      \n" \
"       const float r     = read_imagef(R, smp, p).x;                                                             \n" \
"       const float g     = read_imagef(G, smp, p).x;                                                             \n" \
"       const float b     = read_imagef(B, smp, p).x;                                                             \n" \
"       write_imagef(U1, p, (float4) (r, g, b, 1.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNSIGNED | NLM_COLOR_YUV)) {                                                   \n" \
"       const float r     = norm(read_imageui(R, smp, p).x);                                                      \n" \
"       const float g     = norm(read_imageui(G, smp, p).x);                                                      \n" \
"       const float b     = norm(read_imageui(B, smp, p).x);                                                      \n" \
"       write_imagef(U1, p, (float4) (r, g, b, 1.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_STACKED | NLM_COLOR_YUV)) {                                                    \n" \
"       const int2 p_lsb  = (int2) (x, y + dim.y);                                                                \n" \
"       const float r_msb = read_imagef(R, smp, p    ).x;                                                         \n" \
"       const float g_msb = read_imagef(G, smp, p    ).x;                                                         \n" \
"       const float b_msb = read_imagef(B, smp, p    ).x;                                                         \n" \
"       const float r_lsb = read_imagef(R, smp, p_lsb).x;                                                         \n" \
"       const float g_lsb = read_imagef(G, smp, p_lsb).x;                                                         \n" \
"       const float b_lsb = read_imagef(B, smp, p_lsb).x;                                                         \n" \
"       const float r     = NLM_16BIT_MSB * r_msb + NLM_16BIT_LSB * r_lsb;                                        \n" \
"       const float g     = NLM_16BIT_MSB * g_msb + NLM_16BIT_LSB * g_lsb;                                        \n" \
"       const float b     = NLM_16BIT_MSB * b_msb + NLM_16BIT_LSB * b_lsb;                                        \n" \
"       write_imagef(U1, p, (float4) (r, g, b, 1.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNORM | NLM_COLOR_RGB)) {                                                      \n" \
"       const float r     = read_imagef(R, smp, p).x;                                                             \n" \
"       const float g     = read_imagef(G, smp, p).x;                                                             \n" \
"       const float b     = read_imagef(B, smp, p).x;                                                             \n" \
"       write_imagef(U1, p, (float4) (r, g, b, 1.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNSIGNED | NLM_COLOR_RGB)) {                                                   \n" \
"       const float r     = norm(read_imageui(R, smp, p).x);                                                      \n" \
"       const float g     = norm(read_imageui(G, smp, p).x);                                                      \n" \
"       const float b     = norm(read_imageui(B, smp, p).x);                                                      \n" \
"       write_imagef(U1, p, (float4) (r, g, b, 1.0f));                                                            \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmSpatialUnpack(__write_only image2d_t R, __write_only image2d_t G, __write_only image2d_t B,              \n" \
"__read_only image2d_t U1, const int2 dim) {                                                                      \n" \
"                                                                                                                 \n" \
"   const int x = get_global_id(0);                                                                               \n" \
"   const int y = get_global_id(1);                                                                               \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;                    \n" \
"   const int2 s = (int2) (x, y);                                                                                 \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_CLIP_UNSIGNED | NLM_COLOR_GRAY)) {                                                         \n" \
"       const ushort  val     = denorm(read_imagef(U1, smp, s).x);                                                \n" \
"       write_imageui(R, s,    (uint4) val);                                                                      \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_STACKED | NLM_COLOR_GRAY)) {                                                   \n" \
"       const int2    s_lsb   = (int2) (x, y + dim.y);                                                            \n" \
"       const ushort  in      = convert_ushort_sat(read_imagef(U1, smp, s).x * (float) USHRT_MAX);                \n" \
"       const float   in_msb  = convert_float(in >> CHAR_BIT);                                                    \n" \
"       const float   in_lsb  = convert_float(in & 0xFF);                                                         \n" \
"       const float   val_msb = native_divide(in_msb, (float) UCHAR_MAX);                                         \n" \
"       const float   val_lsb = native_divide(in_lsb, (float) UCHAR_MAX);                                         \n" \
"       write_imagef(R, s,     (float4) val_msb);                                                                 \n" \
"       write_imagef(R, s_lsb, (float4) val_lsb);                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNORM | NLM_COLOR_YUV)) {                                                      \n" \
"       const float4  val     = read_imagef(U1, smp, s);                                                          \n" \
"       write_imagef(R, s,     (float4) val.x);                                                                   \n" \
"       write_imagef(G, s,     (float4) val.y);                                                                   \n" \
"       write_imagef(B, s,     (float4) val.z);                                                                   \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNSIGNED | NLM_COLOR_YUV)) {                                                   \n" \
"       const ushort4 val     = denorm4(read_imagef(U1, smp, s));                                                 \n" \
"       write_imageui(R, s,    (uint4) val.x);                                                                    \n" \
"       write_imageui(G, s,    (uint4) val.y);                                                                    \n" \
"       write_imageui(B, s,    (uint4) val.z);                                                                    \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_STACKED | NLM_COLOR_YUV)) {                                                    \n" \
"       const int2    s_lsb   = (int2) (x, y + dim.y);                                                            \n" \
"       const ushort4 in      = convert_ushort4_sat(read_imagef(U1, smp, s) * (float4) USHRT_MAX);                \n" \
"       const float4  in_msb  = convert_float4(in >> (ushort4) CHAR_BIT);                                         \n" \
"       const float4  in_lsb  = convert_float4(in & (ushort4) 0xFF);                                              \n" \
"       const float4  val_msb = native_divide(in_msb, (float4) UCHAR_MAX);                                        \n" \
"       const float4  val_lsb = native_divide(in_lsb, (float4) UCHAR_MAX);                                        \n" \
"       write_imagef(R, s,     (float4) val_msb.x);                                                               \n" \
"       write_imagef(G, s,     (float4) val_msb.y);                                                               \n" \
"       write_imagef(B, s,     (float4) val_msb.z);                                                               \n" \
"       write_imagef(R, s_lsb, (float4) val_lsb.x);                                                               \n" \
"       write_imagef(G, s_lsb, (float4) val_lsb.y);                                                               \n" \
"       write_imagef(B, s_lsb, (float4) val_lsb.z);                                                               \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNORM | NLM_COLOR_RGB)) {                                                      \n" \
"       const float4  val     = read_imagef(U1, smp, s);                                                          \n" \
"       write_imagef(R, s,     (float4) val.x);                                                                   \n" \
"       write_imagef(G, s,     (float4) val.y);                                                                   \n" \
"       write_imagef(B, s,     (float4) val.z);                                                                   \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNSIGNED | NLM_COLOR_RGB)) {                                                   \n" \
"       const ushort4 val     = denorm4(read_imagef(U1, smp, s));                                                 \n" \
"       write_imageui(R, s,    (uint4) val.x);                                                                    \n" \
"       write_imageui(G, s,    (uint4) val.y);                                                                    \n" \
"       write_imageui(B, s,    (uint4) val.z);                                                                    \n" \
"   }                                                                                                             \n" \
"}                                                                                                                ";

static const char* kernel_source_code =
"#define CHECK_FLAG(flag) ((NLM_TCLIP & (flag)) == (flag))                                                        \n" \
"                                                                                                                 \n" \
"float   norm(uint u);                                                                                            \n" \
"ushort  denorm(float f);                                                                                         \n" \
"ushort4 denorm4(float4 f);                                                                                       \n" \
"                                                                                                                 \n" \
"float   norm(uint u)      { return native_divide((float) (u << NLM_BIT_SHIFT), (float) USHRT_MAX); }             \n" \
"ushort  denorm(float f)   { return convert_ushort_sat(f * (float) USHRT_MAX) >> (ushort) NLM_BIT_SHIFT; }        \n" \
"ushort4 denorm4(float4 f) { return convert_ushort4_sat(f * (float4) USHRT_MAX) >> (ushort4) NLM_BIT_SHIFT; }     \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmDistanceLeft(__read_only image2d_array_t U1, __write_only image2d_array_t U4, const int2 dim,            \n" \
"const int4 q) {                                                                                                  \n" \
"                                                                                                                 \n" \
"   const int x = get_global_id(0);                                                                               \n" \
"   const int y = get_global_id(1);                                                                               \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"   const int4 p = (int4) (x, y, NLM_D, 0);                                                                       \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_COLOR_GRAY)) {                                                                             \n" \
"       const float  u1    = read_imagef(U1, smp, p    ).x;                                                       \n" \
"       const float  u1_pq = read_imagef(U1, smp, p + q).x;                                                       \n" \
"       const float  val   = 3.0f * (u1 - u1_pq) * (u1 - u1_pq);                                                  \n" \
"       write_imagef(U4, p, (float4) val);                                                                        \n" \
"   } else if (CHECK_FLAG(NLM_COLOR_YUV)) {                                                                       \n" \
"       const float4 u1    = read_imagef(U1, smp, p    );                                                         \n" \
"       const float4 u1_pq = read_imagef(U1, smp, p + q);                                                         \n" \
"       const float4 dist  = (u1 - u1_pq) * (u1 - u1_pq);                                                         \n" \
"       const float  val   = dist.x + dist.y + dist.z;                                                            \n" \
"       write_imagef(U4, p, (float4) val);                                                                        \n" \
"   } else if (CHECK_FLAG(NLM_COLOR_RGB)) {                                                                       \n" \
"       const float4 u1    = read_imagef(U1, smp, p    );                                                         \n" \
"       const float4 u1_pq = read_imagef(U1, smp, p + q);                                                         \n" \
"       const float4 wgh   = (float4) (NLM_RGBA_RED, NLM_RGBA_GREEN, NLM_RGBA_BLUE, NLM_RGBA_ALPHA);              \n" \
"       const float4 dist  = wgh * (u1 - u1_pq) * (u1 - u1_pq);                                                   \n" \
"       const float  val   = dist.x + dist.y + dist.z;                                                            \n" \
"       write_imagef(U4, p, (float4) val);                                                                        \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmDistanceRight(__read_only image2d_array_t U1, __write_only image2d_array_t U4, const int2 dim,           \n" \
"const int4 q) {                                                                                                  \n" \
"                                                                                                                 \n" \
"   const int x = get_global_id(0);                                                                               \n" \
"   const int y = get_global_id(1);                                                                               \n" \
"   if((x - q.x) < 0 || (x - q.x) >= dim.x || (y - q.y) < 0 || (y - q.y) >= dim.y) return;                        \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"   const int4 p = (int4) (x, y, NLM_D, 0);                                                                       \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_COLOR_GRAY)) {                                                                             \n" \
"       const float  u1    = read_imagef(U1, smp, p    ).x;                                                       \n" \
"       const float  u1_mq = read_imagef(U1, smp, p - q).x;                                                       \n" \
"       const float  val   = 3.0f * (u1 - u1_mq) * (u1 - u1_mq);                                                  \n" \
"       write_imagef(U4, p - q, (float4) val);                                                                    \n" \
"   } else if (CHECK_FLAG(NLM_COLOR_YUV)) {                                                                       \n" \
"       const float4 u1    = read_imagef(U1, smp, p    );                                                         \n" \
"       const float4 u1_mq = read_imagef(U1, smp, p - q);                                                         \n" \
"       const float4 dist  = (u1 - u1_mq) * (u1 - u1_mq);                                                         \n" \
"       const float  val   = dist.x + dist.y + dist.z;                                                            \n" \
"       write_imagef(U4, p - q, (float4) val);                                                                    \n" \
"   } else if (CHECK_FLAG(NLM_COLOR_RGB)) {                                                                       \n" \
"       const float4 u1    = read_imagef(U1, smp, p    );                                                         \n" \
"       const float4 u1_mq = read_imagef(U1, smp, p - q);                                                         \n" \
"       const float4 wgh   = (float4) (NLM_RGBA_RED, NLM_RGBA_GREEN, NLM_RGBA_BLUE, NLM_RGBA_ALPHA);              \n" \
"       const float4 dist  = wgh * (u1 - u1_mq) * (u1 - u1_mq);                                                   \n" \
"       const float  val   = dist.x + dist.y + dist.z;                                                            \n" \
"       write_imagef(U4, p - q, (float4) val);                                                                    \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel __attribute__((reqd_work_group_size(HRZ_BLOCK_X, HRZ_BLOCK_Y, 1)))                                      \n" \
"void nlmHorizontal(__read_only image2d_array_t U4_in, __write_only image2d_array_t U4_out,                       \n" \
"const int t, const int2 dim) {                                                                                   \n" \
"                                                                                                                 \n" \
"   __local float buffer[HRZ_BLOCK_Y][3*HRZ_BLOCK_X];                                                             \n" \
"   const int x = get_global_id(0);                                                                               \n" \
"   const int y = get_global_id(1);                                                                               \n" \
"   const int lx = get_local_id(0);                                                                               \n" \
"   const int ly = get_local_id(1);                                                                               \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"   const int4 p = (int4) (x, y, t, 0);                                                                           \n" \
"                                                                                                                 \n" \
"   buffer[ly][lx + HRZ_BLOCK_X]   = read_imagef(U4_in, smp, p                                ).x;                \n" \
"   buffer[ly][lx]                 = read_imagef(U4_in, smp, p - (int4) (HRZ_BLOCK_X, 0, 0, 0)).x;                \n" \
"   buffer[ly][lx + 2*HRZ_BLOCK_X] = read_imagef(U4_in, smp, p + (int4) (HRZ_BLOCK_X, 0, 0, 0)).x;                \n" \
"   barrier(CLK_LOCAL_MEM_FENCE);                                                                                 \n" \
"                                                                                                                 \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"   float sum = 0.0f;                                                                                             \n" \
"   for(int i = -NLM_S; i <= NLM_S; i++)                                                                          \n" \
"       sum += buffer[ly][lx + HRZ_BLOCK_X + i];                                                                  \n" \
"   write_imagef(U4_out, p, (float4) sum);                                                                        \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel __attribute__((reqd_work_group_size(VRT_BLOCK_X, VRT_BLOCK_Y, 1)))                                      \n" \
"void nlmVertical(__read_only image2d_array_t U4_in, __write_only image2d_array_t U4_out,                         \n" \
"const int t, const int2 dim) {                                                                                   \n" \
"                                                                                                                 \n" \
"   __local float buffer[3*VRT_BLOCK_Y][VRT_BLOCK_X];                                                             \n" \
"   const int x = get_global_id(0);                                                                               \n" \
"   const int y = get_global_id(1);                                                                               \n" \
"   const int lx = get_local_id(0);                                                                               \n" \
"   const int ly = get_local_id(1);                                                                               \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"   const int4 p = (int4) (x, y, t, 0);                                                                           \n" \
"                                                                                                                 \n" \
"   buffer[ly + VRT_BLOCK_Y][lx]   = read_imagef(U4_in, smp, p                                ).x;                \n" \
"   buffer[ly][lx]                 = read_imagef(U4_in, smp, p - (int4) (0, VRT_BLOCK_Y, 0, 0)).x;                \n" \
"   buffer[ly + 2*VRT_BLOCK_Y][lx] = read_imagef(U4_in, smp, p + (int4) (0, VRT_BLOCK_Y, 0, 0)).x;                \n" \
"   barrier(CLK_LOCAL_MEM_FENCE);                                                                                 \n" \
"                                                                                                                 \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"   float sum = 0.0f;                                                                                             \n" \
"   for(int j = -NLM_S; j <= NLM_S; j++)                                                                          \n" \
"       sum += buffer[ly + VRT_BLOCK_Y + j][lx];                                                                  \n" \
"                                                                                                                 \n" \
"   if(NLM_WMODE == 0) {                                                                                          \n" \
"       const float val = native_recip(1.0f + sum * NLM_H2_INV_NORM);                                             \n" \
"       write_imagef(U4_out, p, (float4) val);                                                                    \n" \
"   } else if (NLM_WMODE == 1) {                                                                                  \n" \
"       const float val = native_exp(- sum * NLM_H2_INV_NORM);                                                    \n" \
"       write_imagef(U4_out, p, (float4) val);                                                                    \n" \
"   } else if (NLM_WMODE == 2) {                                                                                  \n" \
"       const float val = pown(fdim(1.0f, sum * NLM_H2_INV_NORM), 2);                                             \n" \
"       write_imagef(U4_out, p, (float4) val);                                                                    \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmAccumulation(__read_only image2d_array_t U1, __global void* U2, __read_only image2d_array_t U4,          \n" \
"__global float* M, const int2 dim, const int4 q) {                                                               \n" \
"                                                                                                                 \n" \
"   const int x = get_global_id(0);                                                                               \n" \
"   const int y = get_global_id(1);                                                                               \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;                   \n" \
"   const int4 p = (int4) (x, y, NLM_D, 0);                                                                       \n" \
"   const int gidx = mad24(y, dim.x, x);                                                                          \n" \
"                                                                                                                 \n" \
"   const float u4    = read_imagef(U4, smp, p    ).x;                                                            \n" \
"   const float u4_mq = read_imagef(U4, smp, p - q).x;                                                            \n" \
"   M[gidx] = fmax(M[gidx], fmax(u4, u4_mq));                                                                     \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_COLOR_GRAY)) {                                                                             \n" \
"       __global float2* U2c = (__global float2*) U2;                                                             \n" \
"       const float u1_pq = read_imagef(U1, smp, p + q).x;                                                        \n" \
"       const float u1_mq = read_imagef(U1, smp, p - q).x;                                                        \n" \
"       float2 accu;                                                                                              \n" \
"              accu.x = (u4 * u1_pq) + (u4_mq * u1_mq);                                                           \n" \
"              accu.y = (u4 + u4_mq);                                                                             \n" \
"       U2c[gidx] += accu;                                                                                        \n" \
"   } else if (CHECK_FLAG(NLM_COLOR_YUV) | CHECK_FLAG(NLM_COLOR_RGB)) {                                           \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"       const float4 u1_pq = read_imagef(U1, smp, p + q);                                                         \n" \
"       const float4 u1_mq = read_imagef(U1, smp, p - q);                                                         \n" \
"       float4 accu   = (u4 * u1_pq) + (u4_mq * u1_mq);                                                           \n" \
"              accu.w = (u4 + u4_mq);                                                                             \n" \
"       U2c[gidx] += accu;                                                                                        \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmFinish(__read_only image2d_array_t U1_in, __write_only image2d_t U1_out, __global void* U2,              \n" \
"__global float* M, const int2 dim) {                                                                             \n" \
"                                                                                                                 \n" \
"   const int x = get_global_id(0);                                                                               \n" \
"   const int y = get_global_id(1);                                                                               \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;                    \n" \
"   const int4 p = (int4) (x, y, NLM_D, 0);                                                                       \n" \
"   const int2 s = (int2) (x, y);                                                                                 \n" \
"   const int gidx = mad24(y, dim.x, x);                                                                          \n" \
"   const float wM = NLM_WREF * M[gidx];                                                                          \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_COLOR_GRAY)) {                                                                             \n" \
"       __global float2* U2c = (__global float2*) U2;                                                             \n" \
"           const float  u1  = read_imagef(U1_in, smp, p).x;                                                      \n" \
"           const float  num = U2c[gidx].x + wM * u1;                                                             \n" \
"           const float  den = U2c[gidx].y + wM;                                                                  \n" \
"           const float  val = native_divide(num, den);                                                           \n" \
"           write_imagef(U1_out, s, (float4) val);                                                                \n" \
"   } else if (CHECK_FLAG(NLM_COLOR_YUV) | CHECK_FLAG(NLM_COLOR_RGB)) {                                           \n" \
"       __global float4* U2c = (__global float4*) U2;                                                             \n" \
"           const float4 u1  = read_imagef(U1_in, smp, p);                                                        \n" \
"           const float4 num = U2c[gidx] + (float4) wM * u1;                                                      \n" \
"           const float  den = U2c[gidx].w + wM;                                                                  \n" \
"           const float4 val = native_divide(num, (float4) den);                                                  \n" \
"           write_imagef(U1_out, s, val);                                                                         \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmPack(__read_only image2d_t R, __read_only image2d_t G, __read_only image2d_t B,                          \n" \
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
"   if (CHECK_FLAG(NLM_CLIP_UNSIGNED | NLM_COLOR_GRAY)) {                                                         \n" \
"       const float r     = norm(read_imageui(R, smp, s).x);                                                      \n" \
"       write_imagef(U1, p, (float4) r);                                                                          \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_STACKED | NLM_COLOR_GRAY)) {                                                   \n" \
"       const int2  s_lsb = (int2) (x, y + dim.y);                                                                \n" \
"       const float r_msb = read_imagef(R, smp, s    ).x;                                                         \n" \
"       const float r_lsb = read_imagef(R, smp, s_lsb).x;                                                         \n" \
"       const float r     = NLM_16BIT_MSB * r_msb + NLM_16BIT_LSB * r_lsb;                                        \n" \
"       write_imagef(U1, p, (float4) r);                                                                          \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNORM | NLM_COLOR_YUV)) {                                                      \n" \
"       const float r     = read_imagef(R, smp, s).x;                                                             \n" \
"       const float g     = read_imagef(G, smp, s).x;                                                             \n" \
"       const float b     = read_imagef(B, smp, s).x;                                                             \n" \
"       write_imagef(U1, p, (float4) (r, g, b, 1.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNSIGNED | NLM_COLOR_YUV)) {                                                   \n" \
"       const float r     = norm(read_imageui(R, smp, s).x);                                                      \n" \
"       const float g     = norm(read_imageui(G, smp, s).x);                                                      \n" \
"       const float b     = norm(read_imageui(B, smp, s).x);                                                      \n" \
"       write_imagef(U1, p, (float4) (r, g, b, 1.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_STACKED | NLM_COLOR_YUV)) {                                                    \n" \
"       const int2 s_lsb  = (int2) (x, y + dim.y);                                                                \n" \
"       const float r_msb = read_imagef(R, smp, s    ).x;                                                         \n" \
"       const float g_msb = read_imagef(G, smp, s    ).x;                                                         \n" \
"       const float b_msb = read_imagef(B, smp, s    ).x;                                                         \n" \
"       const float r_lsb = read_imagef(R, smp, s_lsb).x;                                                         \n" \
"       const float g_lsb = read_imagef(G, smp, s_lsb).x;                                                         \n" \
"       const float b_lsb = read_imagef(B, smp, s_lsb).x;                                                         \n" \
"       const float r     = NLM_16BIT_MSB * r_msb + NLM_16BIT_LSB * r_lsb;                                        \n" \
"       const float g     = NLM_16BIT_MSB * g_msb + NLM_16BIT_LSB * g_lsb;                                        \n" \
"       const float b     = NLM_16BIT_MSB * b_msb + NLM_16BIT_LSB * b_lsb;                                        \n" \
"       write_imagef(U1, p, (float4) (r, g, b, 1.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNORM | NLM_COLOR_RGB)) {                                                      \n" \
"       const float r     = read_imagef(R, smp, s).x;                                                             \n" \
"       const float g     = read_imagef(G, smp, s).x;                                                             \n" \
"       const float b     = read_imagef(B, smp, s).x;                                                             \n" \
"       write_imagef(U1, p, (float4) (r, g, b, 1.0f));                                                            \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNSIGNED | NLM_COLOR_RGB)) {                                                   \n" \
"       const float r     = norm(read_imageui(R, smp, s).x);                                                      \n" \
"       const float g     = norm(read_imageui(G, smp, s).x);                                                      \n" \
"       const float b     = norm(read_imageui(B, smp, s).x);                                                      \n" \
"       write_imagef(U1, p, (float4) (r, g, b, 1.0f));                                                            \n" \
"   }                                                                                                             \n" \
"}                                                                                                                \n" \
"                                                                                                                 \n" \
"__kernel                                                                                                         \n" \
"void nlmUnpack(__write_only image2d_t R, __write_only image2d_t G, __write_only image2d_t B,                     \n" \
"__read_only image2d_t U1, const int2 dim) {                                                                      \n" \
"                                                                                                                 \n" \
"   const int x = get_global_id(0);                                                                               \n" \
"   const int y = get_global_id(1);                                                                               \n" \
"   if(x >= dim.x || y >= dim.y) return;                                                                          \n" \
"                                                                                                                 \n" \
"   const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;                    \n" \
"   const int2 s = (int2) (x, y);                                                                                 \n" \
"                                                                                                                 \n" \
"   if (CHECK_FLAG(NLM_CLIP_UNSIGNED | NLM_COLOR_GRAY)) {                                                         \n" \
"       const ushort  val     = denorm(read_imagef(U1, smp, s).x);                                                \n" \
"       write_imageui(R, s,    (uint4) val);                                                                      \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_STACKED | NLM_COLOR_GRAY)) {                                                   \n" \
"       const int2    s_lsb   = (int2) (x, y + dim.y);                                                            \n" \
"       const ushort  in      = convert_ushort_sat(read_imagef(U1, smp, s).x * (float) USHRT_MAX);                \n" \
"       const float   in_msb  = convert_float(in >> CHAR_BIT);                                                    \n" \
"       const float   in_lsb  = convert_float(in & 0xFF);                                                         \n" \
"       const float   val_msb = native_divide(in_msb, (float) UCHAR_MAX);                                         \n" \
"       const float   val_lsb = native_divide(in_lsb, (float) UCHAR_MAX);                                         \n" \
"       write_imagef(R, s,     (float4) val_msb);                                                                 \n" \
"       write_imagef(R, s_lsb, (float4) val_lsb);                                                                 \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNORM | NLM_COLOR_YUV)) {                                                      \n" \
"       const float4  val     = read_imagef(U1, smp, s);                                                          \n" \
"       write_imagef(R, s,     (float4) val.x);                                                                   \n" \
"       write_imagef(G, s,     (float4) val.y);                                                                   \n" \
"       write_imagef(B, s,     (float4) val.z);                                                                   \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNSIGNED | NLM_COLOR_YUV)) {                                                   \n" \
"       const ushort4 val     = denorm4(read_imagef(U1, smp, s));                                                 \n" \
"       write_imageui(R, s,    (uint4) val.x);                                                                    \n" \
"       write_imageui(G, s,    (uint4) val.y);                                                                    \n" \
"       write_imageui(B, s,    (uint4) val.z);                                                                    \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_STACKED | NLM_COLOR_YUV)) {                                                    \n" \
"       const int2    s_lsb   = (int2) (x, y + dim.y);                                                            \n" \
"       const ushort4 in      = convert_ushort4_sat(read_imagef(U1, smp, s) * (float4) USHRT_MAX);                \n" \
"       const float4  in_msb  = convert_float4(in >> (ushort4) CHAR_BIT);                                         \n" \
"       const float4  in_lsb  = convert_float4(in & (ushort4) 0xFF);                                              \n" \
"       const float4  val_msb = native_divide(in_msb, (float4) UCHAR_MAX);                                        \n" \
"       const float4  val_lsb = native_divide(in_lsb, (float4) UCHAR_MAX);                                        \n" \
"       write_imagef(R, s,     (float4) val_msb.x);                                                               \n" \
"       write_imagef(G, s,     (float4) val_msb.y);                                                               \n" \
"       write_imagef(B, s,     (float4) val_msb.z);                                                               \n" \
"       write_imagef(R, s_lsb, (float4) val_lsb.x);                                                               \n" \
"       write_imagef(G, s_lsb, (float4) val_lsb.y);                                                               \n" \
"       write_imagef(B, s_lsb, (float4) val_lsb.z);                                                               \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNORM | NLM_COLOR_RGB)) {                                                      \n" \
"       const float4  val     = read_imagef(U1, smp, s);                                                          \n" \
"       write_imagef(R, s,     (float4) val.x);                                                                   \n" \
"       write_imagef(G, s,     (float4) val.y);                                                                   \n" \
"       write_imagef(B, s,     (float4) val.z);                                                                   \n" \
"   } else if (CHECK_FLAG(NLM_CLIP_UNSIGNED | NLM_COLOR_RGB)) {                                                   \n" \
"       const ushort4 val     = denorm4(read_imagef(U1, smp, s));                                                 \n" \
"       write_imageui(R, s,    (uint4) val.x);                                                                    \n" \
"       write_imageui(G, s,    (uint4) val.y);                                                                    \n" \
"       write_imageui(B, s,    (uint4) val.z);                                                                    \n" \
"   }                                                                                                             \n" \
"}                                                                                                                ";
