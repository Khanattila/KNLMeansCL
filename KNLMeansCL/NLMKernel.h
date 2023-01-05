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

#ifndef __NLM_KERNEL_H
#define __NLM_KERNEL_H

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

#define NLM_CLIP_TYPE_UNSIGNED_101010 (1 << 0)
#define NLM_CLIP_TYPE_UNORM_IN_UNSIGNED_OUT (1 << 1)
#define NLM_CLIP_TYPE_UNORM       (1 << 2)
#define NLM_CLIP_TYPE_UNSIGNED    (1 << 3)
#define NLM_CLIP_TYPE_STACKED     (1 << 4)
#define NLM_CLIP_REF_LUMA         (1 << 5)
#define NLM_CLIP_REF_CHROMA       (1 << 6)
#define NLM_CLIP_REF_YUV          (1 << 7)
#define NLM_CLIP_REF_RGB          (1 << 8)
#define NLM_CLIP_REF_PACKEDRGB    (1 << 9)

#define NLM_WMODE_WELSCH           0x0
#define NLM_WMODE_BISQUARE_A       0x1
#define NLM_WMODE_BISQUARE_B       0x2
#define NLM_WMODE_BISQUARE_C       0x3

//////////////////////////////////////////
// Kernel function
const char* nlmClipTypeToString(
    unsigned int clip
);

const char* nlmClipRefToString(
    unsigned int clip
);

const char* nlmWmodeToString(
    unsigned int wmode
);

//////////////////////////////////////////
// Kernel Declaration
extern const char* kernel_source_code;

#endif //__NLM_KERNEL_H__
