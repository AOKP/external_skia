/*
**
** Copyright 2009 Samsung Electronics Co, Ltd.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
*/

#if defined(FIMG2D_ENABLED)

#include "SkFimgApi3x.h"

#define G2D_BLOCKING_USE 0

//  0      kRGB_565_Config
//  1      kARGB_4444_Config
//  2      kARGB_8888_Config
//          [dst][src][option]

#if FIMGAPI_DMC_SIMD_OPT_USE
#if FIMGAPI_HYBRID_USE
int comp_value[3][3][4] = {
    {{360*600, 78*130, 168*280, 66*110}, {78*130, 54*90, 80*140, 66*110}, {168*280, 162*270, 102*170, 72*120}},
    {{54*90, 42*70, 54*90, 42*70}, {54*90, 48*80, 54*90, 48*80}, {66*110, 54*90, 66*110, 54*90}},
    {{84*140, 54*90, 84*130, 54*90}, {102*170, 78*130, 96*160, 78*130}, {120*200, 66*110, 96*160, 72*120}}
};
#else
int comp_value[3][3][4] = {
    {{480*800, 78*130, 168*280, 66*110}, {78*130, 54*90, 80*140, 66*110}, {204*340, 216*360, 102*170, 72*120}},
    {{54*90, 42*70, 54*90, 42*70}, {54*90, 48*80, 54*90, 48*80}, {66*110, 54*90, 66*110, 54*90}},
    {{84*140, 54*90, 84*130, 54*90}, {102*170, 78*130, 96*160, 78*130}, {180*300, 66*110, 96*160, 72*120}}
};
#endif
#else
#if FIMGAPI_HYBRID_USE
int comp_value[3][3][4] = {
    {{360*600, 78*130, 168*280, 66*110}, {78*130, 54*90, 80*140, 66*110}, {168*280, 162*270, 102*170, 72*120}},
    {{54*90, 42*70, 54*90, 42*70}, {54*90, 48*80, 54*90, 48*80}, {66*110, 54*90, 66*110, 54*90}},
    {{84*140, 54*90, 84*130, 54*90}, {102*170, 78*130, 96*160, 78*130}, {120*200, 66*110, 96*160, 72*120}}
};
#else
int comp_value[3][3][4] = {
    {{480*800, 78*130, 168*280, 66*110}, {78*130, 54*90, 80*140, 66*110}, {204*340, 216*360, 102*170, 72*120}},
    {{54*90, 42*70, 54*90, 42*70}, {54*90, 48*80, 54*90, 48*80}, {66*110, 54*90, 66*110, 54*90}},
    {{84*140, 54*90, 84*130, 54*90}, {102*170, 78*130, 96*160, 78*130}, {180*300, 66*110, 96*160, 72*120}}
};
#endif
#endif

int FimgApiCheckPossibleHybrid(Fimg *fimg)
{
    if (!((fimg->srcW == fimg->dstW) && (fimg->srcH == fimg->dstH)))
        return 0;
    if (!((fimg->clipB - fimg->clipT) >= 40))
        return 0;

    if (fimg->canusehybrid == 0)
        return 0;

    if (fimg->srcColorFormat == SkBitmap::kARGB_8888_Config) {
        if (fimg->dstColorFormat == SkBitmap::kARGB_8888_Config) {
            if (fimg->alpha < G2D_ALPHA_VALUE_MAX)
                return 9;
            else
                return 7;
        } else if (fimg->dstColorFormat == SkBitmap::kRGB_565_Config) {
            if (fimg->alpha < G2D_ALPHA_VALUE_MAX)
                return 7;
            else
                return 7;
        }
    } else if (fimg->srcColorFormat == SkBitmap::kRGB_565_Config) {
        if (fimg->dstColorFormat == SkBitmap::kRGB_565_Config) {
            if (fimg->alpha < G2D_ALPHA_VALUE_MAX)
                return 9;
            else
                return 5;
        }
    }
        return 0;
}

bool FimgApiCheckPossible(Fimg *fimg)
{
    switch (fimg->srcColorFormat) {
    case SkBitmap::kRGB_565_Config:
    case SkBitmap::kARGB_8888_Config:
    case SkBitmap::kARGB_4444_Config:
        break;
    default:
        return false;
        break;
    }

    switch (fimg->dstColorFormat) {
    case SkBitmap::kRGB_565_Config:
#if FIMGAPI_DITHERMODE_USE_SW
        if ((fimg->srcColorFormat == SkBitmap::kARGB_8888_Config) &&
            (fimg->isDither == true))
            return false;
        else
            break;
#endif
    case SkBitmap::kARGB_8888_Config:
        break;
    case SkBitmap::kARGB_4444_Config:
#if FIMGAPI_DITHERMODE_USE_SW
        if ((fimg->srcColorFormat == SkBitmap::kARGB_8888_Config) &&
            (fimg->isDither == true))
            return false;
#endif
        break;
    default:
        return false;
    }

    switch (fimg->xfermode) {
    case SkXfermode::kSrcOver_Mode:
        break;
    case SkXfermode::kClear_Mode:
        break;
    case SkXfermode::kSrc_Mode:
        if (fimg->alpha < G2D_ALPHA_VALUE_MAX)
            return false;
        break;
    default:
        return false;
    }

#if FIMGAPI_FILTERMODE_USE_SW
    if ((fimg->srcW != fimg->dstW) || (fimg->srcH != fimg->dstH))
        if (fimg->isFilter == true)
            return false;
#endif

    if (fimg->colorFilter != NULL)
        return false;

    if ((fimg->srcAddr == NULL) || (fimg->dstAddr == NULL))
        return false;

    if (fimg->matrixType & SkMatrix::kAffine_Mask)
        return false;

    if ((fimg->matrixSx < 0) || (fimg->matrixSy < 0))
        return false;

    if ((fimg->srcX + fimg->srcW) > 8000 || (fimg->srcY + fimg->srcH) > 8000)
        return false;

    if ((fimg->dstX + fimg->dstW) > 8000 || (fimg->dstY + fimg->dstH) > 8000)
        return false;

    if ((fimg->clipT < 0) || (fimg->clipB < 0) || (fimg->clipL < 0) || (fimg->clipR < 0)) {
        SkDebugf("Invalid clip value: TBLR = (%d, %d, %d, %d)",fimg->clipT, fimg->clipB, fimg->clipL, fimg->clipR);
        return false;
    }

    if ((fimg->clipT >= fimg->clipB) || (fimg->clipL >= fimg->clipR)) {
        SkDebugf("Invalid clip value: TBLR = (%d, %d, %d, %d)",fimg->clipT, fimg->clipB, fimg->clipL, fimg->clipR);
        return false;
    }

    return true;
}

bool FimgApiCheckPossible_Clipping(Fimg *fimg)
{
    if (((fimg->clipR - fimg->clipL) <= 0) || ((fimg->clipB - fimg->clipT) <= 0))
        return false;
    return true;
}

bool FimgApiCompromise(Fimg *fimg)
{
    int comp_opt = 0;
    if ((fimg->srcW != fimg->dstW) || (fimg->srcH != fimg->dstH)) {
        if ((fimg->alpha != 256) && (fimg->alpha != 255))
            comp_opt = COPT_AS;
        else
            comp_opt = COPT_NAS;
    } else {
        if ((fimg->alpha != 256) && (fimg->alpha != 255))
            comp_opt = COPT_ANS;
        else
            comp_opt = COPT_NANS;
    }

    if ((fimg->isFilter == true) && ((comp_opt == COPT_AS) || (comp_opt == COPT_NAS)))
        if (((fimg->clipR - fimg->clipL) * (fimg->clipB - fimg->clipT)) >= COMP_VALUE_FILTER_SCALE)
            return true;

    if ((((fimg->clipR - fimg->clipL)*1.2) * ((fimg->clipB - fimg->clipT)*0.8)) < comp_value[fimg->dstColorFormat -4][fimg->srcColorFormat - 4][comp_opt])
        return false;

    return true;
}

int FimgApiStretch(Fimg *fimg, const char *func_name)
{
    int ret;

    if (FimgApiCheckPossible(fimg) == false)
        return false;

    if (fimg->clipL < fimg->dstX)
        fimg->clipL = fimg->dstX;
    if (fimg->clipT < fimg->dstY)
        fimg->clipT = fimg->dstY;
    if (fimg->clipR > (fimg->dstX + fimg->dstW))
        fimg->clipR = fimg->dstX + fimg->dstW;
    if (fimg->clipB > (fimg->dstY + fimg->dstH))
        fimg->clipB = fimg->dstY + fimg->dstH;

#if FIMGAPI_COMPROMISE_USE
    if (FimgApiCompromise(fimg) == false)
        return false;
#endif

    int srcColorFormat = FimgApi::COLOR_FORMAT_BASE;
    int dstColorFormat = FimgApi::COLOR_FORMAT_BASE;
    int rotate         = FimgApi::ROTATE_0;
    int alpha          = fimg->alpha;

    switch (fimg->srcColorFormat) {
    case SkBitmap::kRGB_565_Config:
        srcColorFormat = FimgApi::COLOR_FORMAT_RGB_565;
        break;
    case SkBitmap::kARGB_4444_Config:
        srcColorFormat = FimgApi::COLOR_FORMAT_RGBA_4444;
        break;
    case SkBitmap::kARGB_8888_Config:
        srcColorFormat = FimgApi::COLOR_FORMAT_ABGR_8888;
        break;
    default:
        return false;
    }

    switch (fimg->dstColorFormat) {
    case SkBitmap::kRGB_565_Config:
        dstColorFormat = FimgApi::COLOR_FORMAT_RGB_565;
        break;
    case SkBitmap::kARGB_4444_Config:
        dstColorFormat = FimgApi::COLOR_FORMAT_RGBA_4444;
        break;
    case SkBitmap::kARGB_8888_Config:
        dstColorFormat = FimgApi::COLOR_FORMAT_ABGR_8888;
        break;
    default:
        return false;
    }

    switch (fimg->rotate) {
    case 0:
        rotate = FimgApi::ROTATE_0;
        break;
    case 90:
        rotate = FimgApi::ROTATE_90;
        break;
    case 180:
        rotate = FimgApi::ROTATE_180;
        break;
    case 270:
        rotate = FimgApi::ROTATE_270;
        break;
    default:
        #ifdef DEBUG_ON_FIMGAPISTRETCH_LEVEL
            SkDebugf("%s::unsuppoted angle(%d) fail \n", func_name, fimg->rotate);
        #endif
        return false;
    }

    FimgRect fimgSrcRect = {fimg->srcX, fimg->srcY,
                            fimg->srcW, fimg->srcH,
                            (fimg->srcFWStride / fimg->srcBPP), fimg->srcFH,
                            srcColorFormat,
                            fimg->srcBPP,
                            fimg->srcAddr};

    FimgRect fimgDstRect = {fimg->dstX, fimg->dstY,
                            fimg->dstW, fimg->dstH,
                            (fimg->dstFWStride / fimg->dstBPP), fimg->dstFH,
                            dstColorFormat,
                            fimg->dstBPP,
                            fimg->dstAddr};

#if FIMGAPI_HYBRID_USE
    int ratio = FimgApiCheckPossibleHybrid(fimg);
    if (ratio == 0)
        ret = FIMGAPI_FINISHED;
    else {
        int temp_clipT = fimg->clipT;
        fimg->clipT = fimg->clipT + ((fimg->clipB - fimg->clipT) / 10) * (10 - ratio);

        if ((temp_clipT >= fimg->clipT) || (fimg->clipB <= fimg->clipT)) {
            SkDebugf("Invalid clip value: TBLR = (%d, %d, %d, %d), mT = %d",
                temp_clipT, fimg->clipB, fimg->clipL, fimg->clipR, fimg->clipT);
            return false;
        }

        ret = FIMGAPI_HYBRID;
    }
#else
    ret = FIMGAPI_FINISHED;
#endif

#if (FIMGAPI_COMPROMISE_USE == FALSE)
    if (FimgApiCheckPossible_Clipping(fimg) == false)
        return false;
#endif

    FimgClip fimgClip = {fimg->clipT, fimg->clipB, fimg->clipL, fimg->clipR};
    FimgFlag fimgFlag;

    if (ret == FIMGAPI_HYBRID)
        fimgFlag = {rotate, alpha, 0, 0, 0, 0, 0, 0, 0,
            G2D_CACHE_OP | G2D_INTERRUPT | G2D_HYBRID_MODE, fimg->xfermode, G2D_MEMORY_USER};
    else
        fimgFlag = {rotate, alpha, 0, 0, 0, 0, 0, 0, 0,
            G2D_CACHE_OP | G2D_INTERRUPT, fimg->xfermode, G2D_MEMORY_USER};

    if (stretchFimgApi(&fimgSrcRect, &fimgDstRect, &fimgClip, &fimgFlag) < 0)
        return false;

    return ret;
}

int FimgApiSync(const char *func_name)
{
    if (SyncFimgApi() < 0)
        return false;

    return true;
}
#endif
