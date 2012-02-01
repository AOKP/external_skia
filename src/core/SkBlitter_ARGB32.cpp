/* libs/graphics/sgl/SkBlitter_ARGB32.cpp
**
** Copyright 2006, The Android Open Source Project
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
*/

#include "SkCoreBlitters.h"
#include "SkColorPriv.h"
#include "SkShader.h"
#include "SkUtils.h"
#include "SkXfermode.h"

#if defined(SK_SUPPORT_LCDTEXT)
namespace skia_blitter_support {
// subpixel helper functions from SkBlitter_ARGB32_Subpixel.cpp
uint32_t* adjustForSubpixelClip(const SkMask& mask,
                                const SkIRect& clip, const SkBitmap& device,
                                int* widthAdjustment, int* heightAdjustment,
                                const uint32_t** alpha32);
extern uint32_t BlendLCDPixelWithColor(const uint32_t alphaPixel, const uint32_t originalPixel,
                                       const uint32_t sourcePixel);
extern uint32_t BlendLCDPixelWithOpaqueColor(const uint32_t alphaPixel, const uint32_t originalPixel,
                                             const uint32_t sourcePixel);
extern uint32_t BlendLCDPixelWithBlack(const uint32_t alphaPixel, const uint32_t originalPixel);
}

using namespace skia_blitter_support;
#endif

///////////////////////////////////////////////////////////////////////////////

static inline int upscale31To32(int value) {
    SkASSERT((unsigned)value <= 31);
    return value + (value >> 4);
}

static inline int blend32(int src, int dst, int scale) {
    SkASSERT((unsigned)src <= 0xFF);
    SkASSERT((unsigned)dst <= 0xFF);
    SkASSERT((unsigned)scale <= 32);
    return dst + ((src - dst) * scale >> 5);
}

static void blit_lcd16_opaque(SkPMColor dst[], const uint16_t src[],
                              SkPMColor color, int width) {
    int srcR = SkGetPackedR32(color);
    int srcG = SkGetPackedG32(color);
    int srcB = SkGetPackedB32(color);

    for (int i = 0; i < width; i++) {
        uint16_t mask = src[i];
        if (0 == mask) {
            continue;
        }

        SkPMColor d = dst[i];
        
        /*  We want all of these in 5bits, hence the shifts in case one of them
         *  (green) is 6bits.
         */
        int maskR = SkGetPackedR16(mask) >> (SK_R16_BITS - 5);
        int maskG = SkGetPackedG16(mask) >> (SK_G16_BITS - 5);
        int maskB = SkGetPackedB16(mask) >> (SK_B16_BITS - 5);

        // Now upscale them to 0..256, so we can use SkAlphaBlend
        maskR = upscale31To32(maskR);
        maskG = upscale31To32(maskG);
        maskB = upscale31To32(maskB);

        int maskA = SkMax32(SkMax32(maskR, maskG), maskB);

        int dstA = SkGetPackedA32(d);
        int dstR = SkGetPackedR32(d);
        int dstG = SkGetPackedG32(d);
        int dstB = SkGetPackedB32(d);

        dst[i] = SkPackARGB32(blend32(0xFF, dstA, maskA),
                              blend32(srcR, dstR, maskR),
                              blend32(srcG, dstG, maskG),
                              blend32(srcB, dstB, maskB));
    }
}

static void blitmask_lcd16(const SkBitmap& device, const SkMask& mask,
                           const SkIRect& clip, SkPMColor srcColor) {
    int x = clip.fLeft;
    int y = clip.fTop;
    int width = clip.width();
    int height = clip.height();

    SkPMColor*		dstRow = device.getAddr32(x, y);
    const uint16_t* srcRow = mask.getAddrLCD16(x, y);

    do {
        blit_lcd16_opaque(dstRow, srcRow, srcColor, width);
        dstRow = (SkPMColor*)((char*)dstRow + device.rowBytes());
        srcRow = (const uint16_t*)((const char*)srcRow + mask.fRowBytes);
    } while (--height != 0);
}

//////////////////////////////////////////////////////////////////////////////////////

static void SkARGB32_Blit32(const SkBitmap& device, const SkMask& mask,
							const SkIRect& clip, SkPMColor srcColor) {
	U8CPU alpha = SkGetPackedA32(srcColor);
	unsigned flags = SkBlitRow::kSrcPixelAlpha_Flag32;
	if (alpha != 255) {
		flags |= SkBlitRow::kGlobalAlpha_Flag32;
	}
	SkBlitRow::Proc32 proc = SkBlitRow::Factory32(flags);

    int x = clip.fLeft;
    int y = clip.fTop;
    int width = clip.width();
    int height = clip.height();

    SkPMColor*		 dstRow = device.getAddr32(x, y);
    const SkPMColor* srcRow = reinterpret_cast<const SkPMColor*>(mask.getAddr(x, y));

    do {
		proc(dstRow, srcRow, width, alpha);
        dstRow = (SkPMColor*)((char*)dstRow + device.rowBytes());
        srcRow = (const SkPMColor*)((const char*)srcRow + mask.fRowBytes);
    } while (--height != 0);
}

//////////////////////////////////////////////////////////////////////////////////////

SkARGB32_Blitter::SkARGB32_Blitter(const SkBitmap& device, const SkPaint& paint)
        : INHERITED(device) {
    SkColor color = paint.getColor();
    fColor = color;

    fSrcA = SkColorGetA(color);
    unsigned scale = SkAlpha255To256(fSrcA);
    fSrcR = SkAlphaMul(SkColorGetR(color), scale);
    fSrcG = SkAlphaMul(SkColorGetG(color), scale);
    fSrcB = SkAlphaMul(SkColorGetB(color), scale);

    fPMColor = SkPackARGB32(fSrcA, fSrcR, fSrcG, fSrcB);
    fColor32Proc = SkBlitRow::ColorProcFactory();

    // init the pro for blitmask
    fBlitMaskProc = SkBlitMask::Factory(SkBitmap::kARGB_8888_Config, color);
}

const SkBitmap* SkARGB32_Blitter::justAnOpaqueColor(uint32_t* value) {
    if (255 == fSrcA) {
        *value = fPMColor;
        return &fDevice;
    }
    return NULL;
}

#if defined _WIN32 && _MSC_VER >= 1300  // disable warning : local variable used without having been initialized
#pragma warning ( push )
#pragma warning ( disable : 4701 )
#endif

void SkARGB32_Blitter::blitH(int x, int y, int width) {
    SkASSERT(x >= 0 && y >= 0 && x + width <= fDevice.width());

    uint32_t*   device = fDevice.getAddr32(x, y);
    fColor32Proc(device, device, width, fPMColor);
}

void SkARGB32_Blitter::blitAntiH(int x, int y, const SkAlpha antialias[],
                                 const int16_t runs[]) {
    if (fSrcA == 0) {
        return;
    }

    uint32_t    color = fPMColor;
    uint32_t*   device = fDevice.getAddr32(x, y);
    unsigned    opaqueMask = fSrcA; // if fSrcA is 0xFF, then we will catch the fast opaque case

    for (;;) {
        int count = runs[0];
        SkASSERT(count >= 0);
        if (count <= 0) {
            return;
        }
        unsigned aa = antialias[0];
        if (aa) {
            if ((opaqueMask & aa) == 255) {
                sk_memset32(device, color, count);
            } else {
                uint32_t sc = SkAlphaMulQ(color, SkAlpha255To256(aa));
                fColor32Proc(device, device, count, sc);
            }
        }
        runs += count;
        antialias += count;
        device += count;
    }
}

//////////////////////////////////////////////////////////////////////////////////////

#define solid_8_pixels(mask, dst, color)    \
    do {                                    \
        if (mask & 0x80) dst[0] = color;    \
        if (mask & 0x40) dst[1] = color;    \
        if (mask & 0x20) dst[2] = color;    \
        if (mask & 0x10) dst[3] = color;    \
        if (mask & 0x08) dst[4] = color;    \
        if (mask & 0x04) dst[5] = color;    \
        if (mask & 0x02) dst[6] = color;    \
        if (mask & 0x01) dst[7] = color;    \
    } while (0)

#define SK_BLITBWMASK_NAME                  SkARGB32_BlitBW
#define SK_BLITBWMASK_ARGS                  , SkPMColor color
#define SK_BLITBWMASK_BLIT8(mask, dst)      solid_8_pixels(mask, dst, color)
#define SK_BLITBWMASK_GETADDR               getAddr32
#define SK_BLITBWMASK_DEVTYPE               uint32_t
#include "SkBlitBWMaskTemplate.h"

#define blend_8_pixels(mask, dst, sc, dst_scale)                            \
    do {                                                                    \
        if (mask & 0x80) { dst[0] = sc + SkAlphaMulQ(dst[0], dst_scale); }  \
        if (mask & 0x40) { dst[1] = sc + SkAlphaMulQ(dst[1], dst_scale); }  \
        if (mask & 0x20) { dst[2] = sc + SkAlphaMulQ(dst[2], dst_scale); }  \
        if (mask & 0x10) { dst[3] = sc + SkAlphaMulQ(dst[3], dst_scale); }  \
        if (mask & 0x08) { dst[4] = sc + SkAlphaMulQ(dst[4], dst_scale); }  \
        if (mask & 0x04) { dst[5] = sc + SkAlphaMulQ(dst[5], dst_scale); }  \
        if (mask & 0x02) { dst[6] = sc + SkAlphaMulQ(dst[6], dst_scale); }  \
        if (mask & 0x01) { dst[7] = sc + SkAlphaMulQ(dst[7], dst_scale); }  \
    } while (0)

#define SK_BLITBWMASK_NAME                  SkARGB32_BlendBW
#define SK_BLITBWMASK_ARGS                  , uint32_t sc, unsigned dst_scale
#define SK_BLITBWMASK_BLIT8(mask, dst)      blend_8_pixels(mask, dst, sc, dst_scale)
#define SK_BLITBWMASK_GETADDR               getAddr32
#define SK_BLITBWMASK_DEVTYPE               uint32_t
#include "SkBlitBWMaskTemplate.h"

void SkARGB32_Blitter::blitMask(const SkMask& mask, const SkIRect& clip) {
    SkASSERT(mask.fBounds.contains(clip));
    SkASSERT(fSrcA != 0xFF);

    if (fSrcA == 0) {
        return;
    }

    if (mask.fFormat == SkMask::kBW_Format) {
        SkARGB32_BlendBW(fDevice, mask, clip, fPMColor, SkAlpha255To256(255 - fSrcA));
        return;
    } else if (SkMask::kARGB32_Format == mask.fFormat) {
		SkARGB32_Blit32(fDevice, mask, clip, fPMColor);
		return;
    } else if (SkMask::kLCD16_Format == mask.fFormat) {
        blitmask_lcd16(fDevice, mask, clip, fPMColor);
        return;
    }

    int x = clip.fLeft;
    int y = clip.fTop;

    fBlitMaskProc(fDevice.getAddr32(x, y), fDevice.rowBytes(),
                  SkBitmap::kARGB_8888_Config,
                  mask.getAddr(x, y), mask.fRowBytes,
                  fColor, clip.width(), clip.height());
}

void SkARGB32_Opaque_Blitter::blitMask(const SkMask& mask,
                                       const SkIRect& clip) {
    SkASSERT(mask.fBounds.contains(clip));

    if (mask.fFormat == SkMask::kBW_Format) {
        SkARGB32_BlitBW(fDevice, mask, clip, fPMColor);
        return;
    } else if (SkMask::kARGB32_Format == mask.fFormat) {
		SkARGB32_Blit32(fDevice, mask, clip, fPMColor);
		return;
    } else if (SkMask::kLCD16_Format == mask.fFormat) {
        blitmask_lcd16(fDevice, mask, clip, fPMColor);
        return;
	}

    int x = clip.fLeft;
    int y = clip.fTop;
    int width = clip.width();
    int height = clip.height();

#if defined(SK_SUPPORT_LCDTEXT)
    const bool lcdMode = mask.fFormat == SkMask::kHorizontalLCD_Format;
    const bool verticalLCDMode = mask.fFormat == SkMask::kVerticalLCD_Format;

    // In LCD mode the masks have either an extra couple of rows or columns on the edges.
    if (lcdMode || verticalLCDMode) {
        int widthAdjustment, heightAdjustment;
        const uint32_t* alpha32;
        uint32_t* device = adjustForSubpixelClip(mask, clip, fDevice, &widthAdjustment, &heightAdjustment, &alpha32);

        width += widthAdjustment;
        height += heightAdjustment;

        unsigned devRB = fDevice.rowBytes() - (width << 2);
        unsigned alphaExtraRowWords = mask.rowWordsLCD() - width;
        SkPMColor srcColor = fPMColor;

        do {
            unsigned w = width;
            do {
                const uint32_t alphaPixel = *alpha32++;
                const uint32_t originalPixel = *device;
                *device++ = BlendLCDPixelWithOpaqueColor(alphaPixel, originalPixel, srcColor);
            } while (--w != 0);
            device = (uint32_t*)((char*)device + devRB);
            alpha32 += alphaExtraRowWords;
        } while (--height != 0);

        return;
    }
#endif

    fBlitMaskProc(fDevice.getAddr32(x, y), fDevice.rowBytes(),
                  SkBitmap::kARGB_8888_Config,
                  mask.getAddr(x, y), mask.fRowBytes, fColor, width, height);
}

//////////////////////////////////////////////////////////////////////////////////////

void SkARGB32_Blitter::blitV(int x, int y, int height, SkAlpha alpha) {
    if (alpha == 0 || fSrcA == 0) {
        return;
    }

    uint32_t* device = fDevice.getAddr32(x, y);
    uint32_t  color = fPMColor;

    if (alpha != 255) {
        color = SkAlphaMulQ(color, SkAlpha255To256(alpha));
    }

    unsigned dst_scale = 255 - SkGetPackedA32(color);
    uint32_t prevDst = ~device[0];
    uint32_t result  SK_INIT_TO_AVOID_WARNING;
    uint32_t rowBytes = fDevice.rowBytes();

    while (--height >= 0) {
        uint32_t dst = device[0];
        if (dst != prevDst) {
            result = color + SkAlphaMulQ(dst, dst_scale);
            prevDst = dst;
        }
        device[0] = result;
        device = (uint32_t*)((char*)device + rowBytes);
    }
}

void SkARGB32_Blitter::blitRect(int x, int y, int width, int height) {
    SkASSERT(x >= 0 && y >= 0 && x + width <= fDevice.width() && y + height <= fDevice.height());

    if (fSrcA == 0) {
        return;
    }

    uint32_t*   device = fDevice.getAddr32(x, y);
    uint32_t    color = fPMColor;
    size_t      rowBytes = fDevice.rowBytes();

    while (--height >= 0) {
        fColor32Proc(device, device, width, color);
        device = (uint32_t*)((char*)device + rowBytes);
    }
}

#if defined _WIN32 && _MSC_VER >= 1300
#pragma warning ( pop )
#endif

///////////////////////////////////////////////////////////////////////

void SkARGB32_Black_Blitter::blitMask(const SkMask& mask, const SkIRect& clip) {
    SkASSERT(mask.fBounds.contains(clip));

    if (mask.fFormat == SkMask::kBW_Format) {
        SkPMColor black = (SkPMColor)(SK_A32_MASK << SK_A32_SHIFT);

        SkARGB32_BlitBW(fDevice, mask, clip, black);
    } else if (SkMask::kARGB32_Format == mask.fFormat) {
		SkARGB32_Blit32(fDevice, mask, clip, fPMColor);
    } else if (SkMask::kLCD16_Format == mask.fFormat) {
        blitmask_lcd16(fDevice, mask, clip, fPMColor);
    } else {
#if defined(SK_SUPPORT_LCDTEXT)
        const bool      lcdMode = mask.fFormat == SkMask::kHorizontalLCD_Format;
        const bool      verticalLCDMode = mask.fFormat == SkMask::kVerticalLCD_Format;
#endif

        // In LCD mode the masks have either an extra couple of rows or columns on the edges.
        unsigned        width = clip.width();
        unsigned        height = clip.height();

        SkASSERT((int)height > 0);
        SkASSERT((int)width > 0);

#if defined(SK_SUPPORT_LCDTEXT)
        if (lcdMode || verticalLCDMode) {
            int widthAdjustment, heightAdjustment;
            const uint32_t* alpha32;
            uint32_t* device = adjustForSubpixelClip(mask, clip, fDevice, &widthAdjustment, &heightAdjustment, &alpha32);

            width += widthAdjustment;
            height += heightAdjustment;

            unsigned deviceRB = fDevice.rowBytes() - (width << 2);
            unsigned alphaExtraRowWords = mask.rowWordsLCD() - width;

            do {
                unsigned w = width;
                do {
                    const uint32_t alphaPixel = *alpha32++;
                    const uint32_t originalPixel = *device;
                    *device++ = BlendLCDPixelWithBlack(alphaPixel, originalPixel);
                } while (--w != 0);
                device = (uint32_t*)((char*)device + deviceRB);
                alpha32 += alphaExtraRowWords;
            } while (--height != 0);

            return;
        }
#endif

        uint32_t*      device = fDevice.getAddr32(clip.fLeft, clip.fTop);
        unsigned       maskRB = mask.fRowBytes - width;
        unsigned       deviceRB = fDevice.rowBytes() - (width << 2);
        const uint8_t* alpha = mask.getAddr(clip.fLeft, clip.fTop);

#if defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN)
        if (width > 8) {
            asm volatile (
            // Setup constants
            "vmov.i16       q15, #256                       \n\t"   // Set up alpha constant
            "sub            %[width], %[width], #8          \n\t"   // Decrement width loop counter
            // loop height
            "1:                                             \n\t"   //
            "vld1.8         {d4}, [%[alpha_buffer]]         \n\t"   // Load eight alpha values
            "vld4.8         {d0-d3}, [%[rgb_buffer]]        \n\t"   // Load eight RGBX pixels
            "pld            [%[alpha_buffer], #8]           \n\t"   // Pre-load next eight alpha values
            "pld            [%[rgb_buffer], #32]            \n\t"   // Pre-load next eight RGBX pixels
            "mov            r6, %[width]                    \n\t"   // Put width counter in register
            "ands           r4, r6, #0x7                    \n\t"   // Should we do a partial first iteration?
            "moveq          r4, #8                          \n\t"   // Do full iteration?
            // loop width
            "2:                                             \n\t"   //
            "vsubw.u8       q14, q15, d4                    \n\t"   // Calculate inverse alpha (scale)
            "vmov.8         d5, d4                          \n\t"   // Backup alpha
            "vmovl.u8       q8, d0                          \n\t"   // Expand destination red to 16-bit
            "vmovl.u8       q9, d1                          \n\t"   // Expand destination green to 16-bit
            "vmovl.u8       q10, d2                         \n\t"   // Expand destination blue to 16-bit
            "vmovl.u8       q11, d3                         \n\t"   // Expand destination alpha to 16-bit
            "vmul.i16       q8, q8, q14                     \n\t"   // Calculate red
            "add            %[alpha_buffer], r4             \n\t"   // Increment source pointer
            "mov            r5, %[rgb_buffer]               \n\t"   // Backup destination pointer
            "vmul.i16       q9, q9, q14                     \n\t"   // Calculate green
            "add            %[rgb_buffer], r4, lsl #2       \n\t"   // Increment destination pointer
            "vld1.8         {d4}, [%[alpha_buffer]]         \n\t"   // Load next eight source RGBA pixels
            "vmul.i16       q10, q10, q14                   \n\t"   // Calculate blue
            "vmul.i16       q11, q11, q14                   \n\t"   // Calculate alpha
            "vld4.8         {d0-d3}, [%[rgb_buffer]]        \n\t"   // Load next eight destination RGBA pixels
            "vshrn.i16      d24, q8, #8                     \n\t"   // Shift and narrow red
            "vshrn.i16      d25, q9, #8                     \n\t"   // Shift and narrow green
            "pld            [%[alpha_buffer], #8]           \n\t"   // Pre-load next eight alpha values
            "pld            [%[rgb_buffer], #32]            \n\t"   // Pre-load next eight RGBX pixels
            "vshrn.i16      d26, q10, #8                    \n\t"   // Shift and narrow blue
            "vshrn.i16      d27, q11, #8                    \n\t"   // Shift and narrow alpha
            "vadd.i8        d27, d5                         \n\t"   // Add alpha to results
            "subs           r6, r6, r4                      \n\t"   // Decrement loop counter
            "vst4.8         {d24-d27}, [r5]                 \n\t"   // Write result to memory
            "mov            r4, #8                          \n\t"   // Set next loop iteration length
            "bne            2b                              \n\t"   // If width loop counter != 0, loop
            // Handle the last iteration of pixels
            "vsubw.u8       q14, q15, d4                    \n\t"   // Calculate inverse alpha (scale)
            "vmovl.u8       q8, d0                          \n\t"   // Expand destination red to 16-bit
            "vmovl.u8       q9, d1                          \n\t"   // Expand destination green to 16-bit
            "vmovl.u8       q10, d2                         \n\t"   // Expand destination blue to 16-bit
            "vmovl.u8       q11, d3                         \n\t"   // Expand destination alpha to 16-bit
            "vmul.i16       q8, q8, q14                     \n\t"   // Calculate red
            "vmul.i16       q9, q9, q14                     \n\t"   // Calculate green
            "subs           %[height], %[height], #1        \n\t"   // Decrement loop counter
            "vmul.i16       q10, q10, q14                   \n\t"   // Calculate blue
            "vmul.i16       q11, q11, q14                   \n\t"   // Calculate alpha
            "vshrn.i16      d24, q8, #8                     \n\t"   // Shift and narrow red
            "vshrn.i16      d25, q9, #8                     \n\t"   // Shift and narrow green
            "vshrn.i16      d26, q10, #8                    \n\t"   // Shift and narrow blue
            "vshrn.i16      d27, q11, #8                    \n\t"   // Shift and narrow alpha
            "vadd.i8        d27, d4                         \n\t"   // Add alpha to results
            "vst4.8         {d24-d27}, [%[rgb_buffer]]      \n\t"   // Write result to memory
            "add            %[alpha_buffer], r4             \n\t"   // Increment source pointer
            "add            %[rgb_buffer], r4, lsl #2       \n\t"   // Increment destination pointer
            // Jump to next line
            "add            %[alpha_buffer], %[alpha_rowbytes]\n\t" // Increment alpha pointer
            "add            %[rgb_buffer], %[rgb_rowbytes]  \n\t"   // Increment RGBX pointer
            "bne            1b                              \n\t"   // If height loop counter != 0, loop
            : [height] "+r" (height), [rgb_buffer] "+r" (device), [alpha_buffer] "+r" (alpha)
            : [width] "r" (width), [rgb_rowbytes] "r" (deviceRB), [alpha_rowbytes] "r" (maskRB)
            : "cc", "memory", "r4", "r5", "r6", "d0", "d1", "d2", "d3", "d4", "d5", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31"
            );
        }
        else {
            const uint8_t index_tbl[8] = {0, 0, 0, 0, 1, 1, 1, 1};

            asm volatile (
            // Setup constants
            "pld            [%[alpha_buffer]]               \n\t"   // Pre-load eight alpha values
            "pld            [%[rgb_buffer]]                 \n\t"   // Pre-load eight RGBX pixels
            "vld1.8         {d29}, [%[index_tbl]]           \n\t"   // Set up alpha index table
            "vmov.i16       q15, #256                       \n\t"   // Set up alpha constant
            "vmov.i32       d28, #0xFF000000                \n\t"   // Set up alpha mask
            // height loop
            "10:                                            \n\t"   //
            "mov            r6, %[width]                    \n\t"   // Put width counter in register
            "cmp            r6, #1                          \n\t"   // Exit if count is zero
            "beq            12f                             \n\t"   //
            "blt            14f                             \n\t"   //
            // width loop for neon 2-pixel code
            "11:                                            \n\t"   //
            "vld1.16        {d1[0]}, [%[alpha_buffer]]!     \n\t"   // Load two alpha values
            "vld1.8         {d0}, [%[rgb_buffer]]           \n\t"   // Load two RGBX pixels
            "sub            r6, r6, #2                      \n\t"   // Decrement width counter
            "vtbl.8         d2, {d1}, d29                   \n\t"   // Spread out alpha to match pixel format
            "vsubw.u8       q2, q15, d2                     \n\t"   // Calculate inverse alpha (scale)
            "vmovl.u8       q3, d0                          \n\t"   // Expand destination to 16-bit
            "vmul.i16       q3, q3, q2                      \n\t"   // Scale pixels
            "vand.i32       d2, d28                         \n\t"   // Mask alpha
            "vshrn.i16      d0, q3, #8                      \n\t"   // Shift and narrow result
            "vadd.i8        d0, d2                          \n\t"   // Add alpha to results
            "vst1.8         {d0}, [%[rgb_buffer]]!          \n\t"   // Store two RGBX pixels
            "cmp            r6, #1                          \n\t"   // Exit if count is zero
            "bhi            11b                             \n\t"   // Still two or more pixels left
            "blt            13f                             \n\t"   // Zero pixels left
            // code to handle any one last pixel
            "12:                                            \n\t"   //
            "vld1.8         {d1[0]}, [%[alpha_buffer]]!     \n\t"   // Load one alpha value
            "vld1.32        {d0[0]}, [%[rgb_buffer]]        \n\t"   // Load one RGBX pixel
            "vtbl.8         d2, {d1}, d29                   \n\t"   // Spread out alpha to match pixel format
            "vsubw.u8       q2, q15, d2                     \n\t"   // Calculate inverse alpha (scale)
            "vmovl.u8       q3, d0                          \n\t"   // Expand destination to 16-bit
            "vmul.i16       d6, d6, d4                      \n\t"   // Scale pixels
            "vand.i32       d2, d28                         \n\t"   // Mask alpha
            "vshrn.i16      d0, q3, #8                      \n\t"   // Shift and narrow result
            "vadd.i8        d0, d2                          \n\t"   // Add alpha to results
            "vst1.32        {d0[0]}, [%[rgb_buffer]]!       \n\t"   // Store one RGBX pixel
            "13:                                            \n\t"   //
            "add            %[alpha_buffer], %[alpha_rowbytes]\n\t" // Increment alpha pointer
            "subs           %[height], %[height], #1        \n\t"   // Decrement loop counter
            "add            %[rgb_buffer], %[rgb_rowbytes]  \n\t"   // Increment RGBX pointer
            "bne            10b                             \n\t"   //
            "14:                                            \n\t"   //
            : [height] "+r" (height), [rgb_buffer] "+r" (device), [alpha_buffer] "+r" (alpha)
            : [width] "r" (width), [rgb_rowbytes] "r" (deviceRB), [alpha_rowbytes] "r" (maskRB), [index_tbl] "r" (index_tbl)
            : "cc", "memory", "r6", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d28", "d29", "d30", "d31"
            );
        }
#else /* ! __ARM_HAVE_NEON */
        do {
            unsigned w = width;
            do {
                unsigned aa = *alpha++;
                *device = (aa << SK_A32_SHIFT) + SkAlphaMulQ(*device, SkAlpha255To256(255 - aa));
                device += 1;
            } while (--w != 0);
            device = (uint32_t*)((char*)device + deviceRB);
            alpha += maskRB;
        } while (--height != 0);
#endif /* __ARM_HAVE_NEON */
    }
}

void SkARGB32_Black_Blitter::blitAntiH(int x, int y, const SkAlpha antialias[],
                                       const int16_t runs[]) {
    uint32_t*   device = fDevice.getAddr32(x, y);
    SkPMColor   black = (SkPMColor)(SK_A32_MASK << SK_A32_SHIFT);

    for (;;) {
        int count = runs[0];
        SkASSERT(count >= 0);
        if (count <= 0) {
            return;
        }
        unsigned aa = antialias[0];
        if (aa) {
            if (aa == 255) {
                sk_memset32(device, black, count);
            } else {
                SkPMColor src = aa << SK_A32_SHIFT;
                unsigned dst_scale = 256 - aa;
                int n = count;
                do {
                    --n;
                    device[n] = src + SkAlphaMulQ(device[n], dst_scale);
                } while (n > 0);
            }
        }
        runs += count;
        antialias += count;
        device += count;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

SkARGB32_Shader_Blitter::SkARGB32_Shader_Blitter(const SkBitmap& device,
                            const SkPaint& paint) : INHERITED(device, paint) {
    fBuffer = (SkPMColor*)sk_malloc_throw(device.width() * (sizeof(SkPMColor)));

    fXfermode = paint.getXfermode();
    SkSafeRef(fXfermode);

    int flags = 0;
    if (!(fShader->getFlags() & SkShader::kOpaqueAlpha_Flag)) {
        flags |= SkBlitRow::kSrcPixelAlpha_Flag32;
    }
    // we call this on the output from the shader
    fProc32 = SkBlitRow::Factory32(flags);
    // we call this on the output from the shader + alpha from the aa buffer
    fProc32Blend = SkBlitRow::Factory32(flags | SkBlitRow::kGlobalAlpha_Flag32);
}

SkARGB32_Shader_Blitter::~SkARGB32_Shader_Blitter() {
    SkSafeUnref(fXfermode);
    sk_free(fBuffer);
}

void SkARGB32_Shader_Blitter::blitH(int x, int y, int width) {
    SkASSERT(x >= 0 && y >= 0 && x + width <= fDevice.width());

    uint32_t*   device = fDevice.getAddr32(x, y);

    if (fXfermode == NULL && (fShader->getFlags() & SkShader::kOpaqueAlpha_Flag)) {
        fShader->shadeSpan(x, y, device, width);
    } else {
        SkPMColor*  span = fBuffer;
        fShader->shadeSpan(x, y, span, width);
        if (fXfermode) {
            fXfermode->xfer32(device, span, width, NULL);
        } else {
            fProc32(device, span, width, 255);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

void SkARGB32_Shader_Blitter::blitAntiH(int x, int y, const SkAlpha antialias[],
                                        const int16_t runs[]) {
    SkPMColor*  span = fBuffer;
    uint32_t*   device = fDevice.getAddr32(x, y);
    SkShader*   shader = fShader;

    if (fXfermode) {
        for (;;) {
            SkXfermode* xfer = fXfermode;

            int count = *runs;
            if (count <= 0)
                break;
            int aa = *antialias;
            if (aa) {
                shader->shadeSpan(x, y, span, count);
                if (aa == 255) {
                    xfer->xfer32(device, span, count, NULL);
                } else {
                    // count is almost always 1
                    for (int i = count - 1; i >= 0; --i) {
                        xfer->xfer32(&device[i], &span[i], 1, antialias);
                    }
                }
            }
            device += count;
            runs += count;
            antialias += count;
            x += count;
        }
    } else if (fShader->getFlags() & SkShader::kOpaqueAlpha_Flag) {
        for (;;) {
            int count = *runs;
            if (count <= 0) {
                break;
            }
            int aa = *antialias;
            if (aa) {
                if (aa == 255) {
                    // cool, have the shader draw right into the device
                    shader->shadeSpan(x, y, device, count);
                } else {
                    shader->shadeSpan(x, y, span, count);
                    fProc32Blend(device, span, count, aa);
                }
            }
            device += count;
            runs += count;
            antialias += count;
            x += count;
        }
    } else {    // no xfermode but the shader not opaque
        for (;;) {
            int count = *runs;
            if (count <= 0) {
                break;
            }
            int aa = *antialias;
            if (aa) {
                fShader->shadeSpan(x, y, span, count);
                if (aa == 255) {
                    fProc32(device, span, count, 255);
                } else {
                    fProc32Blend(device, span, count, aa);
                }
            }
            device += count;
            runs += count;
            antialias += count;
            x += count;
        }
    }
}
