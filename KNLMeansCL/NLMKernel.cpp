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

#include "NLMKernel.h"

#define STR(code) case code: return #code

//////////////////////////////////////////
// Kernel function
const char* nlmClipTypeToString(cl_uint clip) {
    if (clip & NLM_CLIP_TYPE_UNORM)
        return "NLM_CLIP_TYPE_UNORM";
    else if (clip & NLM_CLIP_TYPE_UNSIGNED)
        return "NLM_CLIP_TYPE_UNSIGNED";
    else if (clip & NLM_CLIP_TYPE_STACKED)
        return "NLM_CLIP_TYPE_STACKED";
    else
        return "OCL_UTILS_CLIP_TYPE_ERROR";
}

const char* nlmClipRefToString(cl_uint clip) {
    if (clip & NLM_CLIP_REF_LUMA)
        return "NLM_CLIP_REF_LUMA";
    else if (clip & NLM_CLIP_REF_CHROMA)
        return "NLM_CLIP_REF_CHROMA";
    else if (clip & NLM_CLIP_REF_YUV)
        return "NLM_CLIP_REF_YUV";
    else if (clip & NLM_CLIP_REF_RGB)
        return "NLM_CLIP_REF_RGB";
    else
        return "OCL_UTILS_CLIP_REF_ERROR";
}

const char* nlmWmodeToString(cl_int wmode) {
    switch (wmode) {
        STR(NLM_WMODE_WELSCH);
        STR(NLM_WMODE_BISQUARE_A);
        STR(NLM_WMODE_BISQUARE_B);
        STR(NLM_WMODE_BISQUARE_C);
        default: return "OCL_UTILS_WMODE_ERROR";
    }
}