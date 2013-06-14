/*
 * Copyright 2012, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "SKIA"
#include <cutils/log.h>
#include <stdlib.h>
#include "SkFimgApi4x.h"

/* int comp_value[src][dst][scale][global_alpha][filter_mode][blending_mode][dithering]
   [src]
    0 : kRGB_565_Config
    1 : kARGB_4444_Config
    2 : kARGB_8888_Config
    3 : kNo_Config
    [dst]
    0 : kRGB_565_Config
    1 : kARGB_4444_Config
    2 : kARGB_8888_Config
    [scale]
    0 : No scaling
    1 : Scaling_up
    2 : Scaling_down
    [global_alpha]
    0 : per pixel alpha
    1 : global alpha
    [filter_mode]
    0 : nearest
    1 : bilinear
    [blending_mode]
    0 : SRC
    1 : SRC_OVER
    [dithering]
    0 : no dithering
    1 : dithering
*/
int comp_value[4][3][3][2][2][2][2] = {
{{{{{{ 106*  60,  98*  55},{ 882* 496, 882* 496}},{{  96*  54,  96*  54},{ 720* 405, 800* 450}}},
   {{{  88*  50,  89*  50},{ 149*  84, 150*  84}},{{  88*  49,  88*  50},{ 151*  85, 151*  85}}}},
  {{{{  99*  56, 119*  67},{ 390* 220, 387* 218}},{{ 101*  57,  90*  51},{ 131*  74, 129*  73}}},
   {{{  92*  52, 117*  66},{ 157*  89, 147*  83}},{{  86*  48,  84*  47},{ 101*  57, 102*  57}}}},
  {{{{ 100*  56,  88*  49},{ 204* 115, 201* 113}},{{  73*  41,  74*  42},{ 105*  59, 105*  59}}},
   {{{  80*  45,  81*  45},{ 119*  67, 119*  67}},{{  69*  39,  70*  39},{  85*  48,  86*  48}}}}},
 {{{{{  93*  52, 102*  57},{ 121*  68,  98*  55}},{{ 101*  57, 102*  57},{ 121*  68,  98*  55}}},
   {{{  91*  51,  92*  52},{  90*  51,  73*  41}},{{  91*  51,  92*  52},{  90*  51,  73*  41}}}},
  {{{{ 105*  59, 130*  73},{ 141*  79, 117*  66}},{{ 105*  59,  96*  54},{ 104*  58,  88*  50}}},
   {{{  96*  54, 111*  63},{ 104*  59,  78*  44}},{{  88*  50,  86*  48},{  88*  49,  72*  41}}}},
  {{{{ 106*  59,  92*  52},{ 108*  61,  91*  51}},{{  75*  42,  75*  42},{  82*  46,  74*  42}}},
   {{{  83*  47,  83*  47},{  86*  48,  70*  39}},{{  71*  40,  71*  40},{  71*  40,   0*   0}}}}},
 {{{{{ 127*  72, 140*  79},{ 148*  83, 148*  83}},{{ 143*  81, 141*  80},{ 148*  83, 149*  84}}},
   {{{ 117*  66, 118*  66},{ 104*  58, 101*  57}},{{ 117*  66, 117*  66},{ 104*  58, 105*  59}}}},
  {{{{ 153*  86, 161*  90},{ 168*  94, 168*  95}},{{ 114*  64, 113*  63},{ 116*  65, 117*  66}}},
   {{{ 122*  69, 129*  72},{ 113*  63, 112*  63}},{{ 104*  58, 101*  57},{  94*  53,  93*  53}}}},
  {{{{ 139*  78, 125*  70},{ 132*  74, 132*  74}},{{  87*  49,  88*  49},{  90*  50,  90*  51}}},
   {{{ 105*  59, 105*  59},{  94*  53,  95*  54}},{{  80*  45,  82*  46},{  75*  42,  76*  43}}}}}},
{{{{{{  92*  52, 101*  57},{ 119*  67, 121*  68}},{{ 102*  57, 102*  57},{ 120*  68, 119*  67}}},
   {{{  94*  53,  94*  53},{ 100*  56,  99*  56}},{{  95*  53,  95*  54},{ 100*  56, 100*  56}}}},
  {{{{ 104*  59, 104*  58},{ 146*  82, 137*  77}},{{  81*  45,  79*  45},{  92*  52,  89*  50}}},
   {{{  94*  53,  95*  53},{ 126*  71, 122*  68}},{{  77*  44,  78*  44},{  90*  51,  87*  49}}}},
  {{{{  97*  55,  90*  51},{ 125*  70, 116*  65}},{{  69*  39,  69*  39},{  78*  44,  76*  43}}},
   {{{  87*  49,  86*  48},{ 114*  64, 108*  61}},{{  67*  38,  67*  37},{  77*  43,  73*  41}}}}},
 {{{{{  98*  55, 108*  61},{ 110*  62,  91*  51}},{{ 106*  60, 107*  60},{ 110*  62,  92*  52}}},
   {{{ 100*  56,  99*  56},{ 102*  58,  86*  49}},{{ 100*  56, 100*  56},{ 102*  57,  86*  48}}}},
  {{{{ 110*  62, 112*  63},{ 112*  63,  91*  51}},{{  82*  46,  82*  46},{  84*  47,  74*  42}}},
   {{{ 102*  57, 101*  57},{ 105*  59,  86*  49}},{{  79*  44,  79*  44},{  81*  45,  72*  41}}}},
  {{{{ 108*  61,  95*  54},{ 101*  57,  85*  48}},{{  69*  39,  70*  39},{  71*  40,   0*   0}}},
   {{{  90*  51,  89*  50},{  95*  54,  82*  46}},{{  70*  39,  69*  39},{  70*  39,   0*   0}}}}},
 {{{{{ 178* 100, 194* 109},{ 137*  77, 138*  78}},{{ 191* 107, 193* 108},{ 139*  78, 137*  77}}},
   {{{ 135*  76, 135*  76},{ 114*  64, 116*  65}},{{ 136*  76, 135*  76},{ 114*  64, 113*  64}}}},
  {{{{ 168*  95, 163*  92},{ 129*  72, 129*  73}},{{  93*  52,  96*  54},{  89*  50,  87*  49}}},
   {{{ 139*  78, 138*  77},{ 116*  65, 115*  65}},{{  92*  52,  92*  52},{  86*  48,  82*  46}}}},
  {{{{ 145*  82, 135*  76},{ 113*  64, 111*  62}},{{  79*  45,  80*  45},{  76*  43,  75*  42}}},
   {{{ 121*  68, 120*  68},{ 105*  59, 106*  60}},{{  77*  43,  77*  43},{  73*  41,  73*  41}}}}}},
{{{{{{ 102*  57, 112*  63},{ 219* 123, 181* 102}},{{ 113*  64, 112*  63},{ 219* 123, 182* 102}}},
   {{{ 104*  59, 104*  59},{ 187* 105, 120*  67}},{{ 105*  59, 104*  59},{ 185* 104, 118*  66}}}},
  {{{{ 120*  67, 119*  67},{ 199* 112, 180* 101}},{{  89*  50,  88*  50},{ 107*  60, 104*  59}}},
   {{{ 109*  61, 111*  62},{ 162*  91, 150*  85}},{{  84*  48,  86*  48},{ 102*  57,  99*  56}}}},
  {{{{ 119*  67, 288* 162},{ 542* 305, 544* 306}},{{  85*  48,  82*  46},{ 179* 101, 139*  78}}},
   {{{ 203* 114, 245* 138},{ 518* 292, 159*  90}},{{  76*  43,  80*  45},{ 109*  61, 107*  60}}}}},
 {{{{{ 111*  63, 120*  67},{ 123*  69,  99*  55}},{{ 120*  67, 121*  68},{ 124*  70, 100*  56}}},
   {{{ 109*  61, 110*  62},{ 114*  64,  93*  52}},{{ 110*  62, 110*  62},{ 114*  64,  93*  52}}}},
  {{{{ 130*  73, 130*  73},{ 133*  75, 100*  56}},{{  93*  52,  92*  52},{  92*  52,  80*  45}}},
   {{{ 116*  65, 116*  65},{ 118*  66,  95*  53}},{{  87*  49,  88*  50},{  89*  50,  78*  44}}}},
  {{{{ 126*  71, 508* 286},{ 196* 110, 494* 278}},{{  93*  52,  84*  47},{  87*  49,  76*  43}}},
   {{{ 500* 281, 259* 146},{ 293* 165, 115*  65}},{{  82*  46,  90*  51},{  84*  47,  71*  40}}}}},
 {{{{{ 521* 293, 511* 287},{ 188* 106, 189* 106}},{{ 515* 289, 508* 286},{ 188* 106, 187* 105}}},
   {{{ 147*  82, 142*  80},{ 119*  67, 120*  68}},{{ 149*  84, 151*  85},{ 121*  68, 121*  68}}}},
  {{{{ 271* 152, 284* 160},{ 168*  95, 170*  95}},{{ 113*  64, 115*  65},{ 102*  58, 103*  58}}},
   {{{ 192* 108, 194* 109},{ 145*  82, 145*  81}},{{ 106*  60, 105*  59},{  97*  55,  97*  54}}}},
  {{{{ 180* 101, 626* 352},{ 612* 344, 596* 335}},{{ 545* 306, 515* 290},{ 491* 276, 183* 103}}},
   {{{ 566* 318, 581* 327},{ 586* 330, 568* 320}},{{ 125*  70, 345* 194},{ 497* 280, 147*  83}}}}}},
{{{{{{ 504* 840, 504* 840},{ 504* 840, 504* 840}},{{ 504* 840, 504* 840},{ 484* 272, 504* 840}}},
   {{{ 504* 840, 504* 840},{ 504* 840, 504* 840}},{{ 504* 840, 459* 258},{ 504* 840, 504* 840}}}},
  {{{{ 504* 840, 175*  99},{ 504* 840, 504* 840}},{{ 504* 840, 105*  59},{ 539* 303, 466* 262}}},
   {{{ 504* 840, 504* 840},{ 504* 840, 381* 214}},{{ 504* 840, 463* 260},{ 647* 364, 504* 840}}}},
  {{{{ 504* 840, 459* 258},{ 497* 280, 453* 255}},{{ 504* 840, 504* 840},{ 504* 840, 504* 840}}},
   {{{ 504* 840, 504* 840},{ 504* 840, 504* 840}},{{ 504* 840, 504* 840},{ 510* 287, 504* 840}}}}},
 {{{{{ 504* 840, 504* 840},{ 504* 840, 504* 840}},{{ 504* 840, 504* 840},{ 504* 840, 504* 840}}},
   {{{ 504* 840, 504* 840},{ 504* 840, 504* 840}},{{ 504* 840, 504* 840},{ 479* 269, 504* 840}}}},
  {{{{ 521* 293, 454* 255},{ 478* 269, 504* 840}},{{ 484* 272, 504* 840},{ 476* 268, 504* 840}}},
   {{{ 504* 840, 504* 840},{ 504* 840, 504* 840}},{{ 504* 840, 504* 840},{ 504* 840, 504* 840}}}},
  {{{{ 504* 840, 504* 840},{ 504* 840, 504* 840}},{{ 504* 840, 504* 840},{ 504* 840, 504* 840}}},
   {{{ 504* 840, 346* 195},{ 504* 840, 143*  81}},{{ 504* 840, 412* 232},{ 504* 840, 539* 303}}}}},
 {{{{{ 791* 445, 814* 458},{ 811* 456, 811* 456}},{{ 812* 457, 806* 453},{ 827* 465, 818* 460}}},
   {{{ 810* 456, 817* 460},{ 805* 453, 817* 459}},{{ 809* 455, 812* 457},{ 810* 455, 817* 460}}}},
  {{{{ 824* 463, 812* 457},{ 813* 457, 818* 460}},{{ 820* 461, 819* 461},{ 818* 460, 816* 459}}},
   {{{ 812* 457, 807* 454},{ 814* 458, 812* 457}},{{ 817* 460, 812* 457},{ 818* 460, 816* 459}}}},
  {{{{ 812* 457, 809* 455},{ 817* 459, 821* 462}},{{ 814* 458, 811* 456},{ 817* 460, 827* 465}}},
   {{{ 814* 458, 815* 459},{ 815* 458, 824* 463}},{{ 821* 462, 821* 462},{ 811* 456, 808* 454}}}}}}};

enum color_format formatSkiaToDriver[] = {
    SRC_DST_FORMAT_END, //!< bitmap has not been configured
    SRC_DST_FORMAT_END, //!< Mask 1bit is not supported by FIMG2D
    SRC_DST_FORMAT_END, //!< Mask 8bit is not supported by FIMG2D
    SRC_DST_FORMAT_END, //!< kIndex8_Config is not supported by FIMG2D
    CF_RGB_565,
    SRC_DST_FORMAT_END, //!< ARGB4444 is not supported by FIMG2D
    CF_ARGB_8888,
};

enum blit_op blendingSkiaToDriver[] = {
    BLIT_OP_CLR,
    BLIT_OP_SRC,
    BLIT_OP_DST,
    BLIT_OP_SRC_OVER,
};

enum scaling filterSkiaToDriver[] = {
    SCALING_NEAREST,
    SCALING_BILINEAR,
};

bool FimgApiCheckPossible(Fimg *fimg)
{
    if (fimg->srcAddr != NULL) {
        switch (fimg->srcColorFormat) {
        case SkBitmap::kRGB_565_Config:
        case SkBitmap::kARGB_8888_Config:
            break;
        default:
            return false;
        }
    }

    switch (fimg->dstColorFormat) {
    case SkBitmap::kRGB_565_Config:
        if ((fimg->srcColorFormat == SkBitmap::kARGB_8888_Config) &&
	    (fimg->isDither == true))
            return false;
        break;
    case SkBitmap::kARGB_8888_Config:
        break;
    default:
        return false;
    }

    switch (fimg->xfermode) {
    case SkXfermode::kSrcOver_Mode:
    case SkXfermode::kClear_Mode:
    case SkXfermode::kSrc_Mode:
    case SkXfermode::kDst_Mode:
        break;
    default:
        return false;
    }

    if (fimg->colorFilter != 0)
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

bool FimgApiIsDstMode(Fimg *fimg)
{
    if (fimg->xfermode == SkXfermode::kDst_Mode)
        return true;
    else
        return false;
}

bool FimgApiCheckPossible_Clipping(Fimg *fimg)
{
    if (((fimg->clipR - fimg->clipL) <= 0) || ((fimg->clipB - fimg->clipT) <= 0))
        return false;

    return true;
}

bool FimgApiCompromise(Fimg *fimg)
{
    int src_fmt = 0;
    int dst_fmt = 0;
    int isScaling = 0;
    int isG_alpha = 0;
    int isFilter = 0;
    int isSrcOver = 0;
    int isDither = 0;
    int clipW = 0, clipH = 0;

    /* source format setting*/
    switch (fimg->srcColorFormat) {
        case SkBitmap::kRGB_565_Config:
            src_fmt = 0;
            break;
        case SkBitmap::kARGB_8888_Config:
            src_fmt = 2;
            break;
        case SkBitmap::kNo_Config:
            src_fmt = 3;
            break;
        default :
            break;
    }
    /* destination format setting */
    switch (fimg->dstColorFormat) {
        case SkBitmap::kRGB_565_Config:
            dst_fmt = 0;
            break;
        case SkBitmap::kARGB_8888_Config:
            dst_fmt = 2;
            break;
        default :
            break;
    }
    /* scaling setting */
    if (fimg->srcW == fimg->dstW && fimg->srcH == fimg->dstH)
        isScaling = 0;
    else if (fimg->srcW * fimg->srcH < fimg->dstW * fimg->dstH)
        isScaling = 1;
    else
        isScaling = 2;
    /* global alpha or per pixel alpha */
    if (fimg->alpha != 255)
        isG_alpha = 1;
    /* filter_mode setting */
    isFilter = fimg->isFilter;
    /* blending mode setting */
    if (fimg->xfermode == SkXfermode::kSrc_Mode)
        isSrcOver = 0;
    else
        isSrcOver = 1;
    /* dither_mode setting */
    isDither = fimg->isDither;

    clipW = (fimg->clipR - fimg->clipL) * 1.2;
    clipH = (fimg->clipB - fimg->clipT) * 0.8;

    if ((clipW * clipH) < comp_value[src_fmt][dst_fmt][isScaling][isG_alpha][isFilter][isSrcOver][isDither])
        return false;

    return true;
}

int FimgApiStretch(Fimg *fimg, const char *func_name)
{
    static unsigned int seq_no = 100;

    struct fimg2d_blit cmd;
    struct fimg2d_image srcImage;
    struct fimg2d_image dstImage;

    /* to support negative Y coordinate */
    if ((fimg->dstAddr != NULL) && (fimg->dstY < 0)) {
        if (fimg->dstH > fimg->dstFH)
            fimg->dstFH = fimg->dstH;
        fimg->dstAddr += fimg->dstFWStride * fimg->dstY;
        fimg->clipT -= fimg->dstY;
        fimg->clipB -= fimg->dstY;
        fimg->dstY = 0;
    }

    if (FimgApiCheckPossible(fimg) == false)
        return false;

    if (FimgApiIsDstMode(fimg) == true)
        return FIMGAPI_FINISHED;

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
    enum rotation rotate;

    switch (fimg->rotate) {
    case 0:
        rotate = ORIGIN;
        break;
    case 90:
        rotate = ROT_90;
        break;
    case 180:
        rotate = ROT_180;
        break;
    case 270:
        rotate = ROT_270;
        break;
    default:
        return false;
    }

    cmd.op = blendingSkiaToDriver[fimg->xfermode];
    cmd.param.g_alpha = fimg->alpha;
    cmd.param.premult = PREMULTIPLIED;
    cmd.param.dither = fimg->isDither;
    cmd.param.rotate = rotate;
    cmd.param.solid_color = fimg->fillcolor;

    if (fimg->srcAddr != NULL && (fimg->srcW != fimg->dstW || fimg->srcH != fimg->dstH)) {
        cmd.param.scaling.mode = filterSkiaToDriver[fimg->isFilter];
        cmd.param.scaling.src_w = fimg->srcW;
        cmd.param.scaling.src_h = fimg->srcH;
        cmd.param.scaling.dst_w = fimg->dstW;
        cmd.param.scaling.dst_h = fimg->dstH;
    } else
        cmd.param.scaling.mode = NO_SCALING;

    cmd.param.repeat.mode = NO_REPEAT;
    cmd.param.repeat.pad_color = 0x0;

    cmd.param.bluscr.mode = OPAQUE;
    cmd.param.bluscr.bs_color = 0x0;
    cmd.param.bluscr.bg_color = 0x0;

    if (fimg->srcAddr != NULL) {
        srcImage.addr.type = ADDR_USER;
        srcImage.addr.start = (long unsigned)fimg->srcAddr;
        srcImage.need_cacheopr = true;
        srcImage.width = fimg->srcFWStride / fimg->srcBPP;
        srcImage.height = fimg->srcFH;
        srcImage.stride = fimg->srcFWStride;
        if (fimg->srcColorFormat == SkBitmap::kRGB_565_Config)
            srcImage.order = AX_RGB;
        else
            srcImage.order = AX_BGR; // kARGB_8888_Config

        srcImage.fmt = formatSkiaToDriver[fimg->srcColorFormat];
        srcImage.rect.x1 = fimg->srcX;
        srcImage.rect.y1 = fimg->srcY;
        srcImage.rect.x2 = fimg->srcX + fimg->srcW;
        srcImage.rect.y2 = fimg->srcY + fimg->srcH;
        cmd.src = &srcImage;
    } else
        cmd.src = NULL;

    if (fimg->dstAddr != NULL) {
        dstImage.addr.type = ADDR_USER;
        dstImage.addr.start = (long unsigned)fimg->dstAddr;
        dstImage.need_cacheopr = true;
        dstImage.width = fimg->dstFWStride / fimg->dstBPP;
        dstImage.height = fimg->dstFH;
        dstImage.stride = fimg->dstFWStride;
        if (fimg->dstColorFormat == SkBitmap::kRGB_565_Config)
            dstImage.order = AX_RGB;
        else
            dstImage.order = AX_BGR; // kARGB_8888_Config

        dstImage.fmt = formatSkiaToDriver[fimg->dstColorFormat];
        dstImage.rect.x1 = fimg->dstX;
        dstImage.rect.y1 = fimg->dstY;
        dstImage.rect.x2 = fimg->dstX + fimg->dstW;
        dstImage.rect.y2 = fimg->dstY + fimg->dstH;

        cmd.param.clipping.enable = true;
        cmd.param.clipping.x1 = fimg->clipL;
        cmd.param.clipping.y1 = fimg->clipT;
        cmd.param.clipping.x2 = fimg->clipR;
        cmd.param.clipping.y2 = fimg->clipB;

        cmd.dst = &dstImage;

    } else
        cmd.dst = NULL;

    cmd.msk = NULL;

    cmd.tmp = NULL;
    cmd.sync = BLIT_SYNC;
    cmd.seq_no = seq_no++;

    if (FimgApiCheckPossible_Clipping(fimg) == false)
        return false;

#if defined(FIMGAPI_DEBUG_MESSAGE)
    printDataBlit("Before stretchFimgApi:", &cmd);
    printDataMatrix(fimg->matrixType);
#endif

    if (stretchFimgApi(&cmd) < 0) {
#if defined(FIMGAPI_DEBUG_MESSAGE)
        ALOGE("%s:stretch failed\n", __FUNCTION__);
#endif
        return false;
    }

    return FIMGAPI_FINISHED;
}

int FimgARGB32_Rect(struct Fimg &fimg, uint32_t *device, int x, int y, int width, int height,
                    size_t rowbyte, uint32_t color)
{
    fimg.srcAddr        = (unsigned char *)NULL;
    fimg.srcColorFormat = SkBitmap::kNo_Config;
    fimg.mskAddr        = (unsigned char *)NULL;

    fimg.fillcolor      = toARGB32(color);
    fimg.srcColorFormat = SkBitmap::kARGB_8888_Config;

    fimg.dstX           = x;
    fimg.dstY           = y;
    fimg.dstW           = width;
    fimg.dstH           = height;
    fimg.dstFWStride    = rowbyte;
    fimg.dstFH          = y + height;
    fimg.dstBPP         = 4; /* 4Byte */
    fimg.dstColorFormat = SkBitmap::kARGB_8888_Config;
    fimg.dstAddr        = (unsigned char *)device;

    fimg.clipT          = y;
    fimg.clipB          = y + height;
    fimg.clipL          = x;
    fimg.clipR          = x + width;

    fimg.rotate         = 0;

    fimg.xfermode       = SkXfermode::kSrcOver_Mode;
    fimg.isDither       = false;
    fimg.colorFilter    = 0;
    fimg.matrixType     = 0;
    fimg.matrixSx       = 0;
    fimg.matrixSy       = 0;
    fimg.alpha          = 0xFF;

    return FimgApiStretch(&fimg, __func__);
}

uint32_t toARGB32(uint32_t color)
{
    U8CPU a = SkGetPackedA32(color);
    U8CPU r = SkGetPackedR32(color);
    U8CPU g = SkGetPackedG32(color);
    U8CPU b = SkGetPackedB32(color);

    return (a << 24) | (r << 16) | (g << 8) | (b << 0);
}
