/*
 * Copyright 2012 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "SkBlitMask.h"
#include "SkBlitRow.h"
#include "SkColorPriv.h"
#include "SkDither.h"
#include "SkUtils.h"

#include "SkCachePreload_arm.h"

#if defined(__ARM_HAVE_NEON)
#include <arm_neon.h>
#endif

extern "C"  void S32A_Opaque_BlitRow32_arm(SkPMColor* SK_RESTRICT dst,
                                            const SkPMColor* SK_RESTRICT src,
                                            int count,
                                            U8CPU alpha);

extern "C"  void S32A_Blend_BlitRow32_arm_neon(SkPMColor* SK_RESTRICT dst,
                                          const SkPMColor* SK_RESTRICT src,
                                          int count,
                                          U8CPU alpha);

#if defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN)
static void S32A_D565_Opaque_neon(uint16_t* SK_RESTRICT dst,
                                  const SkPMColor* SK_RESTRICT src, int count,
                                  U8CPU alpha, int /*x*/, int /*y*/) {
    SkASSERT(255 == alpha);

    if (count >= 8) {
        uint16_t* SK_RESTRICT keep_dst;
        
        asm volatile (
                      "ands       ip, %[count], #7            \n\t"
                      "vmov.u8    d31, #1<<7                  \n\t"
                      "vld1.16    {q12}, [%[dst]]             \n\t"
                      "vld4.8     {d0-d3}, [%[src]]           \n\t"
                      // Thumb does not support the standard ARM conditional
                      // instructions but instead requires the 'it' instruction
                      // to signal conditional execution
                      "it eq                                  \n\t"
                      "moveq      ip, #8                      \n\t"
                      "mov        %[keep_dst], %[dst]         \n\t"
                      
                      "add        %[src], %[src], ip, LSL#2   \n\t"
                      "add        %[dst], %[dst], ip, LSL#1   \n\t"
                      "subs       %[count], %[count], ip      \n\t"
                      "b          9f                          \n\t"
                      // LOOP
                      "2:                                         \n\t"
                      
                      "vld1.16    {q12}, [%[dst]]!            \n\t"
                      "vld4.8     {d0-d3}, [%[src]]!          \n\t"
                      "vst1.16    {q10}, [%[keep_dst]]        \n\t"
                      "sub        %[keep_dst], %[dst], #8*2   \n\t"
                      "subs       %[count], %[count], #8      \n\t"
                      "9:                                         \n\t"
                      "pld        [%[dst],#32]                \n\t"
                      // expand 0565 q12 to 8888 {d4-d7}
                      "vmovn.u16  d4, q12                     \n\t"
                      "vshr.u16   q11, q12, #5                \n\t"
                      "vshr.u16   q10, q12, #6+5              \n\t"
                      "vmovn.u16  d5, q11                     \n\t"
                      "vmovn.u16  d6, q10                     \n\t"
                      "vshl.u8    d4, d4, #3                  \n\t"
                      "vshl.u8    d5, d5, #2                  \n\t"
                      "vshl.u8    d6, d6, #3                  \n\t"
                      
                      "vmovl.u8   q14, d31                    \n\t"
                      "vmovl.u8   q13, d31                    \n\t"
                      "vmovl.u8   q12, d31                    \n\t"
                      
                      // duplicate in 4/2/1 & 8pix vsns
                      "vmvn.8     d30, d3                     \n\t"
                      "vmlal.u8   q14, d30, d6                \n\t"
                      "vmlal.u8   q13, d30, d5                \n\t"
                      "vmlal.u8   q12, d30, d4                \n\t"
                      "vshr.u16   q8, q14, #5                 \n\t"
                      "vshr.u16   q9, q13, #6                 \n\t"
                      "vaddhn.u16 d6, q14, q8                 \n\t"
                      "vshr.u16   q8, q12, #5                 \n\t"
                      "vaddhn.u16 d5, q13, q9                 \n\t"
                      "vqadd.u8   d6, d6, d0                  \n\t"  // moved up
                      "vaddhn.u16 d4, q12, q8                 \n\t"
                      // intentionally don't calculate alpha
                      // result in d4-d6
                      
                      "vqadd.u8   d5, d5, d1                  \n\t"
                      "vqadd.u8   d4, d4, d2                  \n\t"
                      
                      // pack 8888 {d4-d6} to 0565 q10
                      "vshll.u8   q10, d6, #8                 \n\t"
                      "vshll.u8   q3, d5, #8                  \n\t"
                      "vshll.u8   q2, d4, #8                  \n\t"
                      "vsri.u16   q10, q3, #5                 \n\t"
                      "vsri.u16   q10, q2, #11                \n\t"
                      
                      "bne        2b                          \n\t"
                      
                      "1:                                         \n\t"
                      "vst1.16      {q10}, [%[keep_dst]]      \n\t"
                      : [count] "+r" (count)
                      : [dst] "r" (dst), [keep_dst] "r" (keep_dst), [src] "r" (src) 
                      : "ip", "cc", "memory", "d0","d1","d2","d3","d4","d5","d6","d7",
                      "d16","d17","d18","d19","d20","d21","d22","d23","d24","d25","d26","d27","d28","d29",
                      "d30","d31"
                      );
    }
    else 
    {   // handle count < 8
        uint16_t* SK_RESTRICT keep_dst;
        
        asm volatile (
                      "vmov.u8    d31, #1<<7                  \n\t"
                      "mov        %[keep_dst], %[dst]         \n\t"
                      
                      "tst        %[count], #4                \n\t"
                      "beq        14f                         \n\t"
                      "vld1.16    {d25}, [%[dst]]!            \n\t"
                      "vld1.32    {q1}, [%[src]]!             \n\t"
                      
                      "14:                                        \n\t"
                      "tst        %[count], #2                \n\t"
                      "beq        12f                         \n\t"
                      "vld1.32    {d24[1]}, [%[dst]]!         \n\t"
                      "vld1.32    {d1}, [%[src]]!             \n\t"
                      
                      "12:                                        \n\t"
                      "tst        %[count], #1                \n\t"
                      "beq        11f                         \n\t"
                      "vld1.16    {d24[1]}, [%[dst]]!         \n\t"
                      "vld1.32    {d0[1]}, [%[src]]!          \n\t"
                      
                      "11:                                        \n\t"
                      // unzips achieve the same as a vld4 operation
                      "vuzpq.u16  q0, q1                      \n\t"
                      "vuzp.u8    d0, d1                      \n\t"
                      "vuzp.u8    d2, d3                      \n\t"
                      // expand 0565 q12 to 8888 {d4-d7}
                      "vmovn.u16  d4, q12                     \n\t"
                      "vshr.u16   q11, q12, #5                \n\t"
                      "vshr.u16   q10, q12, #6+5              \n\t"
                      "vmovn.u16  d5, q11                     \n\t"
                      "vmovn.u16  d6, q10                     \n\t"
                      "vshl.u8    d4, d4, #3                  \n\t"
                      "vshl.u8    d5, d5, #2                  \n\t"
                      "vshl.u8    d6, d6, #3                  \n\t"
                      
                      "vmovl.u8   q14, d31                    \n\t"
                      "vmovl.u8   q13, d31                    \n\t"
                      "vmovl.u8   q12, d31                    \n\t"
                      
                      // duplicate in 4/2/1 & 8pix vsns
                      "vmvn.8     d30, d3                     \n\t"
                      "vmlal.u8   q14, d30, d6                \n\t"
                      "vmlal.u8   q13, d30, d5                \n\t"
                      "vmlal.u8   q12, d30, d4                \n\t"
                      "vshr.u16   q8, q14, #5                 \n\t"
                      "vshr.u16   q9, q13, #6                 \n\t"
                      "vaddhn.u16 d6, q14, q8                 \n\t"
                      "vshr.u16   q8, q12, #5                 \n\t"
                      "vaddhn.u16 d5, q13, q9                 \n\t"
                      "vqadd.u8   d6, d6, d0                  \n\t"  // moved up
                      "vaddhn.u16 d4, q12, q8                 \n\t"
                      // intentionally don't calculate alpha
                      // result in d4-d6
                      
                      "vqadd.u8   d5, d5, d1                  \n\t"
                      "vqadd.u8   d4, d4, d2                  \n\t"
                      
                      // pack 8888 {d4-d6} to 0565 q10
                      "vshll.u8   q10, d6, #8                 \n\t"
                      "vshll.u8   q3, d5, #8                  \n\t"
                      "vshll.u8   q2, d4, #8                  \n\t"
                      "vsri.u16   q10, q3, #5                 \n\t"
                      "vsri.u16   q10, q2, #11                \n\t"
                      
                      // store
                      "tst        %[count], #4                \n\t"
                      "beq        24f                         \n\t"
                      "vst1.16    {d21}, [%[keep_dst]]!       \n\t"
                      
                      "24:                                        \n\t"
                      "tst        %[count], #2                \n\t"
                      "beq        22f                         \n\t"
                      "vst1.32    {d20[1]}, [%[keep_dst]]!    \n\t"
                      
                      "22:                                        \n\t"
                      "tst        %[count], #1                \n\t"
                      "beq        21f                         \n\t"
                      "vst1.16    {d20[1]}, [%[keep_dst]]!    \n\t"
                      
                      "21:                                        \n\t"
                      : [count] "+r" (count)
                      : [dst] "r" (dst), [keep_dst] "r" (keep_dst), [src] "r" (src)
                      : "ip", "cc", "memory", "d0","d1","d2","d3","d4","d5","d6","d7",
                      "d16","d17","d18","d19","d20","d21","d22","d23","d24","d25","d26","d27","d28","d29",
                      "d30","d31"
                      );
    }
}

static void S32A_D565_Blend_neon(uint16_t* SK_RESTRICT dst,
                                 const SkPMColor* SK_RESTRICT src, int count,
                                 U8CPU alpha, int /*x*/, int /*y*/) {

    U8CPU alpha_for_asm = alpha;

    asm volatile (
    /* This code implements a Neon version of S32A_D565_Blend. The output differs from
     * the original in two respects:
     *  1. The results have a few mismatches compared to the original code. These mismatches
     *     never exceed 1. It's possible to improve accuracy vs. a floating point
     *     implementation by introducing rounding right shifts (vrshr) for the final stage.
     *     Rounding is not present in the code below, because although results would be closer
     *     to a floating point implementation, the number of mismatches compared to the 
     *     original code would be far greater.
     *  2. On certain inputs, the original code can overflow, causing colour channels to
     *     mix. Although the Neon code can also overflow, it doesn't allow one colour channel
     *     to affect another.
     */
                  
#if 1
		/* reflects SkAlpha255To256()'s change from a+a>>7 to a+1 */
                  "add        %[alpha], %[alpha], #1         \n\t"   // adjust range of alpha 0-256
#else
                  "add        %[alpha], %[alpha], %[alpha], lsr #7    \n\t"   // adjust range of alpha 0-256
#endif
                  "vmov.u16   q3, #255                        \n\t"   // set up constant
                  "movs       r4, %[count], lsr #3            \n\t"   // calc. count>>3
                  "vmov.u16   d2[0], %[alpha]                 \n\t"   // move alpha to Neon
                  "beq        2f                              \n\t"   // if count8 == 0, exit
                  "vmov.u16   q15, #0x1f                      \n\t"   // set up blue mask
                  
                  "1:                                             \n\t"
                  "vld1.u16   {d0, d1}, [%[dst]]              \n\t"   // load eight dst RGB565 pixels
                  "subs       r4, r4, #1                      \n\t"   // decrement loop counter
                  "vld4.u8    {d24, d25, d26, d27}, [%[src]]! \n\t"   // load eight src ABGR32 pixels
                  //  and deinterleave
                  
                  "vshl.u16   q9, q0, #5                      \n\t"   // shift green to top of lanes
                  "vand       q10, q0, q15                    \n\t"   // extract blue
                  "vshr.u16   q8, q0, #11                     \n\t"   // extract red
                  "vshr.u16   q9, q9, #10                     \n\t"   // extract green
                  // dstrgb = {q8, q9, q10}
                  
                  "vshr.u8    d24, d24, #3                    \n\t"   // shift red to 565 range
                  "vshr.u8    d25, d25, #2                    \n\t"   // shift green to 565 range
                  "vshr.u8    d26, d26, #3                    \n\t"   // shift blue to 565 range
                  
                  "vmovl.u8   q11, d24                        \n\t"   // widen red to 16 bits
                  "vmovl.u8   q12, d25                        \n\t"   // widen green to 16 bits
                  "vmovl.u8   q14, d27                        \n\t"   // widen alpha to 16 bits
                  "vmovl.u8   q13, d26                        \n\t"   // widen blue to 16 bits
                  // srcrgba = {q11, q12, q13, q14}
                  
                  "vmul.u16   q2, q14, d2[0]                  \n\t"   // sa * src_scale
                  "vmul.u16   q11, q11, d2[0]                 \n\t"   // red result = src_red * src_scale
                  "vmul.u16   q12, q12, d2[0]                 \n\t"   // grn result = src_grn * src_scale
                  "vmul.u16   q13, q13, d2[0]                 \n\t"   // blu result = src_blu * src_scale
                  
                  "vshr.u16   q2, q2, #8                      \n\t"   // sa * src_scale >> 8
                  "vsub.u16   q2, q3, q2                      \n\t"   // 255 - (sa * src_scale >> 8)
                  // dst_scale = q2
                  
                  "vmla.u16   q11, q8, q2                     \n\t"   // red result += dst_red * dst_scale
                  "vmla.u16   q12, q9, q2                     \n\t"   // grn result += dst_grn * dst_scale
                  "vmla.u16   q13, q10, q2                    \n\t"   // blu result += dst_blu * dst_scale

#if 1
	// trying for a better match with SkDiv255Round(a)
	// C alg is:  a+=128; (a+a>>8)>>8
	// we'll use just a rounding shift [q2 is available for scratch]
                  "vrshr.u16   q11, q11, #8                    \n\t"   // shift down red
                  "vrshr.u16   q12, q12, #8                    \n\t"   // shift down green
                  "vrshr.u16   q13, q13, #8                    \n\t"   // shift down blue
#else
	// arm's original "truncating divide by 256"
                  "vshr.u16   q11, q11, #8                    \n\t"   // shift down red
                  "vshr.u16   q12, q12, #8                    \n\t"   // shift down green
                  "vshr.u16   q13, q13, #8                    \n\t"   // shift down blue
#endif
                  
                  "vsli.u16   q13, q12, #5                    \n\t"   // insert green into blue
                  "vsli.u16   q13, q11, #11                   \n\t"   // insert red into green/blue
                  "vst1.16    {d26, d27}, [%[dst]]!           \n\t"   // write pixel back to dst, update ptr
                  
                  "bne        1b                              \n\t"   // if counter != 0, loop
                  "2:                                             \n\t"   // exit
                  
                  : [src] "+r" (src), [dst] "+r" (dst), [count] "+r" (count), [alpha] "+r" (alpha_for_asm)
                  :
                  : "cc", "memory", "r4", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31"
                  );

    count &= 7;
    if (count > 0) {
        do {
            SkPMColor sc = *src++;
            if (sc) {
                uint16_t dc = *dst;
                unsigned dst_scale = 255 - SkMulDiv255Round(SkGetPackedA32(sc), alpha);
                unsigned dr = SkMulS16(SkPacked32ToR16(sc), alpha) + SkMulS16(SkGetPackedR16(dc), dst_scale);
                unsigned dg = SkMulS16(SkPacked32ToG16(sc), alpha) + SkMulS16(SkGetPackedG16(dc), dst_scale);
                unsigned db = SkMulS16(SkPacked32ToB16(sc), alpha) + SkMulS16(SkGetPackedB16(dc), dst_scale);
                *dst = SkPackRGB16(SkDiv255Round(dr), SkDiv255Round(dg), SkDiv255Round(db));
            }
            dst += 1;
        } while (--count != 0);
    }
}

/* dither matrix for Neon, derived from gDitherMatrix_3Bit_16.
 * each dither value is spaced out into byte lanes, and repeated
 * to allow an 8-byte load from offsets 0, 1, 2 or 3 from the
 * start of each row.
 */
static const uint8_t gDitherMatrix_Neon[48] = {
    0, 4, 1, 5, 0, 4, 1, 5, 0, 4, 1, 5,
    6, 2, 7, 3, 6, 2, 7, 3, 6, 2, 7, 3,
    1, 5, 0, 4, 1, 5, 0, 4, 1, 5, 0, 4,
    7, 3, 6, 2, 7, 3, 6, 2, 7, 3, 6, 2,
    
};

static void S32_D565_Blend_Dither_neon(uint16_t *dst, const SkPMColor *src,
                                       int count, U8CPU alpha, int x, int y)
{
    /* select row and offset for dither array */
    const uint8_t *dstart = &gDitherMatrix_Neon[(y&3)*12 + (x&3)];
    
    /* rescale alpha to range 0 - 256 */
    int scale = SkAlpha255To256(alpha);
    
    asm volatile (
                  "vld1.8         {d31}, [%[dstart]]              \n\t"   // load dither values
                  "vshr.u8        d30, d31, #1                    \n\t"   // calc. green dither values
                  "vdup.16        d6, %[scale]                    \n\t"   // duplicate scale into neon reg
                  "vmov.i8        d29, #0x3f                      \n\t"   // set up green mask
                  "vmov.i8        d28, #0x1f                      \n\t"   // set up blue mask
                  "1:                                                 \n\t"
                  "vld4.8         {d0, d1, d2, d3}, [%[src]]!     \n\t"   // load 8 pixels and split into argb
                  "vshr.u8        d22, d0, #5                     \n\t"   // calc. red >> 5
                  "vshr.u8        d23, d1, #6                     \n\t"   // calc. green >> 6
                  "vshr.u8        d24, d2, #5                     \n\t"   // calc. blue >> 5
                  "vaddl.u8       q8, d0, d31                     \n\t"   // add in dither to red and widen
                  "vaddl.u8       q9, d1, d30                     \n\t"   // add in dither to green and widen
                  "vaddl.u8       q10, d2, d31                    \n\t"   // add in dither to blue and widen
                  "vsubw.u8       q8, q8, d22                     \n\t"   // sub shifted red from result
                  "vsubw.u8       q9, q9, d23                     \n\t"   // sub shifted green from result
                  "vsubw.u8       q10, q10, d24                   \n\t"   // sub shifted blue from result
                  "vshrn.i16      d22, q8, #3                     \n\t"   // shift right and narrow to 5 bits
                  "vshrn.i16      d23, q9, #2                     \n\t"   // shift right and narrow to 6 bits
                  "vshrn.i16      d24, q10, #3                    \n\t"   // shift right and narrow to 5 bits
                  // load 8 pixels from dst, extract rgb
                  "vld1.16        {d0, d1}, [%[dst]]              \n\t"   // load 8 pixels
                  "vshrn.i16      d17, q0, #5                     \n\t"   // shift green down to bottom 6 bits
                  "vmovn.i16      d18, q0                         \n\t"   // narrow to get blue as bytes
                  "vshr.u16       q0, q0, #11                     \n\t"   // shift down to extract red
                  "vand           d17, d17, d29                   \n\t"   // and green with green mask
                  "vand           d18, d18, d28                   \n\t"   // and blue with blue mask
                  "vmovn.i16      d16, q0                         \n\t"   // narrow to get red as bytes
                  // src = {d22 (r), d23 (g), d24 (b)}
                  // dst = {d16 (r), d17 (g), d18 (b)}
                  // subtract dst from src and widen
                  "vsubl.s8       q0, d22, d16                    \n\t"   // subtract red src from dst
                  "vsubl.s8       q1, d23, d17                    \n\t"   // subtract green src from dst
                  "vsubl.s8       q2, d24, d18                    \n\t"   // subtract blue src from dst
                  // multiply diffs by scale and shift
                  "vmul.i16       q0, q0, d6[0]                   \n\t"   // multiply red by scale
                  "vmul.i16       q1, q1, d6[0]                   \n\t"   // multiply blue by scale
                  "vmul.i16       q2, q2, d6[0]                   \n\t"   // multiply green by scale
                  "subs           %[count], %[count], #8          \n\t"   // decrement loop counter
                  "vshrn.i16      d0, q0, #8                      \n\t"   // shift down red by 8 and narrow
                  "vshrn.i16      d2, q1, #8                      \n\t"   // shift down green by 8 and narrow
                  "vshrn.i16      d4, q2, #8                      \n\t"   // shift down blue by 8 and narrow
                  // add dst to result
                  "vaddl.s8       q0, d0, d16                     \n\t"   // add dst to red
                  "vaddl.s8       q1, d2, d17                     \n\t"   // add dst to green
                  "vaddl.s8       q2, d4, d18                     \n\t"   // add dst to blue
                  // put result into 565 format
                  "vsli.i16       q2, q1, #5                      \n\t"   // shift up green and insert into blue
                  "vsli.i16       q2, q0, #11                     \n\t"   // shift up red and insert into blue
                  "vst1.16        {d4, d5}, [%[dst]]!             \n\t"   // store result
                  "bgt            1b                              \n\t"   // loop if count > 0
                  : [src] "+r" (src), [dst] "+r" (dst), [count] "+r" (count)
                  : [dstart] "r" (dstart), [scale] "r" (scale)
                  : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
                  );
    
    DITHER_565_SCAN(y);
    
    while((count & 7) > 0)
    {
        SkPMColor c = *src++;
        
        int dither = DITHER_VALUE(x);
        int sr = SkGetPackedR32(c);
        int sg = SkGetPackedG32(c);
        int sb = SkGetPackedB32(c);
        sr = SkDITHER_R32To565(sr, dither);
        sg = SkDITHER_G32To565(sg, dither);
        sb = SkDITHER_B32To565(sb, dither);
        
        uint16_t d = *dst;
        *dst++ = SkPackRGB16(SkAlphaBlend(sr, SkGetPackedR16(d), scale),
                             SkAlphaBlend(sg, SkGetPackedG16(d), scale),
                             SkAlphaBlend(sb, SkGetPackedB16(d), scale));
        DITHER_INC_X(x);
        count--;
    }
}

#define S32A_D565_Opaque_PROC       S32A_D565_Opaque_neon
#define S32A_D565_Blend_PROC        S32A_D565_Blend_neon
#define S32_D565_Blend_Dither_PROC  S32_D565_Blend_Dither_neon
#elif __ARM_ARCH__ >= 7 && !defined(SK_CPU_BENDIAN)
static void S32A_D565_Opaque_v7(uint16_t* SK_RESTRICT dst,
                                  const SkPMColor* SK_RESTRICT src, int count,
                                  U8CPU alpha, int /*x*/, int /*y*/) {
    SkASSERT(255 == alpha);

    asm volatile (
                  "1:                                   \n\t"
                  "ldr     r3, [%[src]], #4             \n\t"
                  "cmp     r3, #0xff000000              \n\t"
                  "blo     2f                           \n\t"
                  "and     r4, r3, #0x0000f8            \n\t"
                  "and     r5, r3, #0x00fc00            \n\t"
                  "and     r6, r3, #0xf80000            \n\t"
                  "pld     [r1, #32]                    \n\t"
                  "lsl     r3, r4, #8                   \n\t"
                  "orr     r3, r3, r5, lsr #5           \n\t"
                  "orr     r3, r3, r6, lsr #19          \n\t"
                  "subs    %[count], %[count], #1       \n\t"
                  "strh    r3, [%[dst]], #2             \n\t"
                  "bne     1b                           \n\t"
                  "b       4f                           \n\t"
                  "2:                                   \n\t"
                  "lsrs    r7, r3, #24                  \n\t"
                  "beq     3f                           \n\t"
                  "ldrh    r4, [%[dst]]                 \n\t"
                  "rsb     r7, r7, #255                 \n\t"
                  "and     r6, r4, #0x001f              \n\t"
                  "ubfx    r5, r4, #5, #6               \n\t"
                  "pld     [r0, #16]                    \n\t"
                  "lsr     r4, r4, #11                  \n\t"
                  "smulbb  r6, r6, r7                   \n\t"
                  "smulbb  r5, r5, r7                   \n\t"
                  "smulbb  r4, r4, r7                   \n\t"
                  "ubfx    r7, r3, #16, #8              \n\t"
                  "ubfx    ip, r3, #8, #8               \n\t"
                  "and     r3, r3, #0xff                \n\t"
                  "add     r6, r6, #16                  \n\t"
                  "add     r5, r5, #32                  \n\t"
                  "add     r4, r4, #16                  \n\t"
                  "add     r6, r6, r6, lsr #5           \n\t"
                  "add     r5, r5, r5, lsr #6           \n\t"
                  "add     r4, r4, r4, lsr #5           \n\t"
                  "add     r6, r7, r6, lsr #5           \n\t"
                  "add     r5, ip, r5, lsr #6           \n\t"
                  "add     r4, r3, r4, lsr #5           \n\t"
                  "lsr     r6, r6, #3                   \n\t"
                  "and     r5, r5, #0xfc                \n\t"
                  "and     r4, r4, #0xf8                \n\t"
                  "orr     r6, r6, r5, lsl #3           \n\t"
                  "orr     r4, r6, r4, lsl #8           \n\t"
                  "strh    r4, [%[dst]], #2             \n\t"
                  "pld     [r1, #32]                    \n\t"
                  "subs    %[count], %[count], #1       \n\t"
                  "bne     1b                           \n\t"
                  "b       4f                           \n\t"
                  "3:                                   \n\t"
                  "subs    %[count], %[count], #1       \n\t"
                  "add     %[dst], %[dst], #2           \n\t"
                  "bne     1b                           \n\t"
                  "4:                                   \n\t"
                  : [dst] "+r" (dst), [src] "+r" (src), [count] "+r" (count)
                  :
                  : "memory", "cc", "r3", "r4", "r5", "r6", "r7", "ip"
                  );
}
#define S32A_D565_Blend_PROC        NULL
#define S32_D565_Blend_Dither_PROC  NULL
#else
#define S32A_D565_Blend_PROC        NULL
#define S32_D565_Blend_Dither_PROC  NULL
#endif

/*
 * Use neon version of BLIT assembly code from S32A_D565_Opaque_arm.S, where we process
 * 16 pixels at-a-time and also optimize for alpha=255 case.
 */
#define S32A_D565_Opaque_PROC       NULL

/* Don't have a special version that assumes each src is opaque, but our S32A
    is still faster than the default, so use it here
 */
#define S32_D565_Opaque_PROC    S32A_D565_Opaque_PROC
#define S32_D565_Blend_PROC     S32A_D565_Blend_PROC

///////////////////////////////////////////////////////////////////////////////

#if defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN) && defined(ENABLE_OPTIMIZED_S32A_BLITTERS)

/* External function in file S32A_Opaque_BlitRow32_neon.S */
extern "C" void S32A_Opaque_BlitRow32_neon(SkPMColor* SK_RESTRICT dst,
                                           const SkPMColor* SK_RESTRICT src,
                                           int count, U8CPU alpha);

#define S32A_Opaque_BlitRow32_PROC  S32A_Opaque_BlitRow32_neon

#elif defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN) && defined(TEST_SRC_ALPHA)

static void S32A_Opaque_BlitRow32_neon_test_alpha(SkPMColor* SK_RESTRICT dst,
                                  const SkPMColor* SK_RESTRICT src,
                                  int count, U8CPU alpha) {
	SkASSERT(255 == alpha);
	if (count <= 0)
	return;

	/* Use these to check if src is transparent or opaque */
	const unsigned int ALPHA_OPAQ  = 0xFF000000;
	const unsigned int ALPHA_TRANS = 0x00FFFFFF;

#define UNROLL  4
	const SkPMColor* SK_RESTRICT src_end = src + count - (UNROLL + 1);
	const SkPMColor* SK_RESTRICT src_temp = src;

	/* set up the NEON variables */
	uint8x8_t alpha_mask;
	static const uint8_t alpha_mask_setup[] = {3,3,3,3,7,7,7,7};
	alpha_mask = vld1_u8(alpha_mask_setup);

	uint8x8_t src_raw, dst_raw, dst_final;
	uint8x8_t src_raw_2, dst_raw_2, dst_final_2;
	uint8x8_t dst_cooked;
	uint16x8_t dst_wide;
	uint8x8_t alpha_narrow;
	uint16x8_t alpha_wide;

	/* choose the first processing type */
	if( src >= src_end)
		goto TAIL;
	if(*src <= ALPHA_TRANS)
		goto ALPHA_0;
	if(*src >= ALPHA_OPAQ)
		goto ALPHA_255;
	/* fall-thru */

ALPHA_1_TO_254:
	do {

		/* get the source */
		src_raw = vreinterpret_u8_u32(vld1_u32(src));
		src_raw_2 = vreinterpret_u8_u32(vld1_u32(src+2));

		/* get and hold the dst too */
		dst_raw = vreinterpret_u8_u32(vld1_u32(dst));
		dst_raw_2 = vreinterpret_u8_u32(vld1_u32(dst+2));


		/* get the alphas spread out properly */
		alpha_narrow = vtbl1_u8(src_raw, alpha_mask);
		/* reflect SkAlpha255To256() semantics a+1 vs a+a>>7 */
		/* we collapsed (255-a)+1 ... */
		alpha_wide = vsubw_u8(vdupq_n_u16(256), alpha_narrow);

		/* spread the dest */
		dst_wide = vmovl_u8(dst_raw);

		/* alpha mul the dest */
		dst_wide = vmulq_u16 (dst_wide, alpha_wide);
		dst_cooked = vshrn_n_u16(dst_wide, 8);

		/* sum -- ignoring any byte lane overflows */
		dst_final = vadd_u8(src_raw, dst_cooked);

		alpha_narrow = vtbl1_u8(src_raw_2, alpha_mask);
		/* reflect SkAlpha255To256() semantics a+1 vs a+a>>7 */
		/* we collapsed (255-a)+1 ... */
		alpha_wide = vsubw_u8(vdupq_n_u16(256), alpha_narrow);

		/* spread the dest */
		dst_wide = vmovl_u8(dst_raw_2);

		/* alpha mul the dest */
		dst_wide = vmulq_u16 (dst_wide, alpha_wide);
		dst_cooked = vshrn_n_u16(dst_wide, 8);

		/* sum -- ignoring any byte lane overflows */
		dst_final_2 = vadd_u8(src_raw_2, dst_cooked);

		vst1_u32(dst, vreinterpret_u32_u8(dst_final));
		vst1_u32(dst+2, vreinterpret_u32_u8(dst_final_2));

		src += UNROLL;
		dst += UNROLL;

		/* if 2 of the next pixels aren't between 1 and 254
		it might make sense to go to the optimized loops */
		if((src[0] <= ALPHA_TRANS && src[1] <= ALPHA_TRANS) || (src[0] >= ALPHA_OPAQ && src[1] >= ALPHA_OPAQ))
			break;

	} while(src < src_end);

	if (src >= src_end)
		goto TAIL;

	if(src[0] >= ALPHA_OPAQ && src[1] >= ALPHA_OPAQ)
		goto ALPHA_255;

	/*fall-thru*/

ALPHA_0:

	/*In this state, we know the current alpha is 0 and
	 we optimize for the next alpha also being zero. */
	src_temp = src;  //so we don't have to increment dst every time
	do {
		if(*(++src) > ALPHA_TRANS)
			break;
		if(*(++src) > ALPHA_TRANS)
			break;
		if(*(++src) > ALPHA_TRANS)
			break;
		if(*(++src) > ALPHA_TRANS)
			break;
	} while(src < src_end);

	dst += (src - src_temp);

	/* no longer alpha 0, so determine where to go next. */
	if( src >= src_end)
		goto TAIL;
	if(*src >= ALPHA_OPAQ)
		goto ALPHA_255;
	else
		goto ALPHA_1_TO_254;

ALPHA_255:
	while((src[0] & src[1] & src[2] & src[3]) >= ALPHA_OPAQ) {
		dst[0]=src[0];
		dst[1]=src[1];
		dst[2]=src[2];
		dst[3]=src[3];
		src+=UNROLL;
		dst+=UNROLL;
		if(src >= src_end)
			goto TAIL;
	}

	//Handle remainder.
	if(*src >= ALPHA_OPAQ) { *dst++ = *src++;
		if(*src >= ALPHA_OPAQ) { *dst++ = *src++;
			if(*src >= ALPHA_OPAQ) { *dst++ = *src++; }
		}
	}

	if( src >= src_end)
		goto TAIL;
	if(*src <= ALPHA_TRANS)
		goto ALPHA_0;
	else
		goto ALPHA_1_TO_254;

TAIL:
	/* do any residual iterations */
	src_end += UNROLL + 1;  //goto the real end
	while(src != src_end) {
		if( *src != 0 ) {
			if( *src >= ALPHA_OPAQ ) {
				*dst = *src;
			}
			else {
				*dst = SkPMSrcOver(*src, *dst);
			}
		}
		src++;
		dst++;
	}
	return;
}

#define S32A_Opaque_BlitRow32_PROC  S32A_Opaque_BlitRow32_neon_test_alpha

#elif defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN)

/*
 * User S32A_Opaque_BlitRow32 function from S32A_Opaque_BlitRow32.S
 */
#if 0
static void S32A_Opaque_BlitRow32_neon(SkPMColor* SK_RESTRICT dst,
                                  const SkPMColor* SK_RESTRICT src,
                                  int count, U8CPU alpha) {

    SkASSERT(255 == alpha);
    if (count > 0) {


	uint8x8_t alpha_mask;

	static const uint8_t alpha_mask_setup[] = {3,3,3,3,7,7,7,7};
	alpha_mask = vld1_u8(alpha_mask_setup);

	/* do the NEON unrolled code */
#define	UNROLL	4
	while (count >= UNROLL) {
	    uint8x8_t src_raw, dst_raw, dst_final;
	    uint8x8_t src_raw_2, dst_raw_2, dst_final_2;

	    /* get the source */
	    src_raw = vreinterpret_u8_u32(vld1_u32(src));
#if	UNROLL > 2
	    src_raw_2 = vreinterpret_u8_u32(vld1_u32(src+2));
#endif

	    /* get and hold the dst too */
	    dst_raw = vreinterpret_u8_u32(vld1_u32(dst));
#if	UNROLL > 2
	    dst_raw_2 = vreinterpret_u8_u32(vld1_u32(dst+2));
#endif

	/* 1st and 2nd bits of the unrolling */
	{
	    uint8x8_t dst_cooked;
	    uint16x8_t dst_wide;
	    uint8x8_t alpha_narrow;
	    uint16x8_t alpha_wide;

	    /* get the alphas spread out properly */
	    alpha_narrow = vtbl1_u8(src_raw, alpha_mask);
#if 1
	    /* reflect SkAlpha255To256() semantics a+1 vs a+a>>7 */
	    /* we collapsed (255-a)+1 ... */
	    alpha_wide = vsubw_u8(vdupq_n_u16(256), alpha_narrow);
#else
	    alpha_wide = vsubw_u8(vdupq_n_u16(255), alpha_narrow);
	    alpha_wide = vaddq_u16(alpha_wide, vshrq_n_u16(alpha_wide,7));
#endif

	    /* spread the dest */
	    dst_wide = vmovl_u8(dst_raw);

	    /* alpha mul the dest */
	    dst_wide = vmulq_u16 (dst_wide, alpha_wide);
	    dst_cooked = vshrn_n_u16(dst_wide, 8);

	    /* sum -- ignoring any byte lane overflows */
	    dst_final = vadd_u8(src_raw, dst_cooked);
	}

#if	UNROLL > 2
	/* the 3rd and 4th bits of our unrolling */
	{
	    uint8x8_t dst_cooked;
	    uint16x8_t dst_wide;
	    uint8x8_t alpha_narrow;
	    uint16x8_t alpha_wide;

	    alpha_narrow = vtbl1_u8(src_raw_2, alpha_mask);
#if 1
	    /* reflect SkAlpha255To256() semantics a+1 vs a+a>>7 */
	    /* we collapsed (255-a)+1 ... */
	    alpha_wide = vsubw_u8(vdupq_n_u16(256), alpha_narrow);
#else
	    alpha_wide = vsubw_u8(vdupq_n_u16(255), alpha_narrow);
	    alpha_wide = vaddq_u16(alpha_wide, vshrq_n_u16(alpha_wide,7));
#endif

	    /* spread the dest */
	    dst_wide = vmovl_u8(dst_raw_2);

	    /* alpha mul the dest */
	    dst_wide = vmulq_u16 (dst_wide, alpha_wide);
	    dst_cooked = vshrn_n_u16(dst_wide, 8);

	    /* sum -- ignoring any byte lane overflows */
	    dst_final_2 = vadd_u8(src_raw_2, dst_cooked);
	}
#endif

	    vst1_u32(dst, vreinterpret_u32_u8(dst_final));
#if	UNROLL > 2
	    vst1_u32(dst+2, vreinterpret_u32_u8(dst_final_2));
#endif

	    src += UNROLL;
	    dst += UNROLL;
	    count -= UNROLL;
	}
#undef	UNROLL

	/* do any residual iterations */
        while (--count >= 0) {
#ifdef TEST_SRC_ALPHA
            SkPMColor sc = *src;
            if (sc) {
                unsigned srcA = SkGetPackedA32(sc);
                SkPMColor result = sc;
                if (srcA != 255) {
                    result = SkPMSrcOver(sc, *dst);
                }
                *dst = result;
            }
#else
            *dst = SkPMSrcOver(*src, *dst);
#endif
            src += 1;
            dst += 1;
        }
    }
}

#define	S32A_Opaque_BlitRow32_PROC	S32A_Opaque_BlitRow32_neon
#endif

/*
 * Use asm version of BlitRow function. Neon instructions are
 * used for armv7 targets.
 */
#define S32A_Opaque_BlitRow32_PROC  S32A_Opaque_BlitRow32_arm

#elif defined (__ARM_ARCH__) /* #if defined(__ARM_HAVE_NEON) && defined... */

#if defined(TEST_SRC_ALPHA)

static void __attribute__((naked)) S32A_Opaque_BlitRow32_arm_test_alpha
                                        (SkPMColor* SK_RESTRICT dst,
                                         const SkPMColor* SK_RESTRICT src,
                                         int count, U8CPU alpha) {

/* Optimizes for alpha == 0, alpha == 255, and 1 < alpha < 255 cases individually */
/* Predicts that the next pixel will have the same alpha type as the current pixel */

asm volatile (

    "\tSTMDB  r13!, {r4-r12, r14}        \n" /* saving r4-r12, lr on the stack */
                                             /* we should not save r0-r3 according to ABI */

    "\tCMP    r2, #0                     \n" /* if (count == 0) */
    "\tBEQ    9f                         \n" /* go to EXIT */

    "\tMOV    r12, #0xff                 \n" /* load the 0xff mask in r12 */
    "\tORR    r12, r12, r12, LSL #16     \n" /* convert it to 0xff00ff in r12 */

    "\tMOV    r14, #255                  \n" /* r14 = 255 */
                                             /* will be used later for left-side comparison */

    "\tADD    r2, %[src], r2, LSL #2     \n" /* r2 points to last array element which can be used */
    "\tSUB    r2, r2, #16                \n" /* as a base for 4-way processing algorithm */

    "\tCMP    %[src], r2                 \n" /* if our current [src] array pointer is bigger than */
    "\tBGT    8f                         \n" /* calculated marker for 4-way -> */
                                             /* use simple one-by-one processing */

    /* START OF DISPATCHING BLOCK */

    "\t0:                                \n"

    "\tLDM    %[src]!, {r3, r4, r5, r6}  \n" /* 4-way loading of source values to r3-r6 */

    "\tLSR    r7, r3, #24                \n" /* if not all src alphas of 4-way block are equal -> */
    "\tCMP    r7, r4, LSR #24            \n"
    "\tCMPEQ  r7, r5, LSR #24            \n"
    "\tCMPEQ  r7, r6, LSR #24            \n"
    "\tBNE    1f                         \n" /* -> go to general 4-way processing routine */

    "\tCMP    r14, r7                    \n" /* if all src alphas are equal to 255 */
    "\tBEQ    3f                         \n" /* go to alpha == 255 optimized routine */

    "\tCMP    r7,  #0                    \n" /* if all src alphas are equal to 0 */
    "\tBEQ    6f                         \n" /* go to alpha == 0 optimized routine */

    /* END OF DISPATCHING BLOCK */

    /* START OF BLOCK OPTIMIZED FOR 0 < ALPHA < 255 */

    "\t1:                                \n"
                                             /* we do not have enough registers to make */
                                             /* 4-way [dst] loading -> we are using 2 * 2-way */

    "\tLDM    %[dst], {r7, r8}           \n" /* 1st 2-way loading of dst values to r7-r8 */

    /* PROCESSING BLOCK 1 */
    /* r3 = src, r7 = dst */

    "\tLSR    r11, r3,  #24              \n" /* extracting alpha from source and storing to r11 */
    "\tAND    r9,  r12, r7               \n" /* r9 = br masked by r12 (0xff00ff) */
    "\tRSB    r11, r11, #256             \n" /* subtracting the alpha from 255 -> r11 = scale */
    "\tAND    r10, r12, r7, LSR #8       \n" /* r10 = ag masked by r12 (0xff00ff) */
    "\tMUL    r9,  r9,  r11              \n" /* br = br * scale */
    "\tAND    r9,  r12, r9, LSR #8       \n" /* lsr br by 8 and mask it */
    "\tMUL    r10, r10, r11              \n" /* ag = ag * scale */
    "\tAND    r10, r10, r12, LSL #8      \n" /* mask ag with reverse mask */
    "\tORR    r7,  r9,  r10              \n" /* br | ag */
    "\tADD    r7,  r3,  r7               \n" /* dst = src + calc dest(r8) */

    /* PROCESSING BLOCK 2 */
    /* r4 = src, r8 = dst */

    "\tLSR    r11, r4,  #24              \n" /* see PROCESSING BLOCK 1 */
    "\tAND    r9,  r12, r8               \n"
    "\tRSB    r11, r11, #256             \n"
    "\tAND    r10, r12, r8, LSR #8       \n"
    "\tMUL    r9,  r9,  r11              \n"
    "\tAND    r9,  r12, r9, LSR #8       \n"
    "\tMUL    r10, r10, r11              \n"
    "\tAND    r10, r10, r12, LSL #8      \n"
    "\tORR    r8,  r9,  r10              \n"
    "\tADD    r8,  r4,  r8               \n"

    "\tSTM    %[dst]!, {r7, r8}          \n" /* 1st 2-way storing of processed dst values */

    "\tLDM    %[dst], {r9, r10}          \n" /* 2nd 2-way loading of dst values to r9-r10 */

    /* PROCESSING BLOCK 3 */
    /* r5 = src, r9 = dst */

    "\tLSR    r11, r5,  #24              \n" /* see PROCESSING BLOCK 1 */
    "\tAND    r7,  r12, r9               \n"
    "\tRSB    r11, r11, #256             \n"
    "\tAND    r8,  r12, r9, LSR #8       \n"
    "\tMUL    r7,  r7,  r11              \n"
    "\tAND    r7,  r12, r7, LSR #8       \n"
    "\tMUL    r8,  r8,  r11              \n"
    "\tAND    r8,  r8,  r12, LSL #8      \n"
    "\tORR    r9,  r7,  r8               \n"
    "\tADD    r9,  r5,  r9               \n"

    /* PROCESSING BLOCK 4 */
    /* r6 = src, r10 = dst */

    "\tLSR    r11, r6,  #24              \n" /* see PROCESSING BLOCK 1 */
    "\tAND    r7,  r12, r10              \n"
    "\tRSB    r11, r11, #256             \n"
    "\tAND    r8,  r12, r10, LSR #8      \n"
    "\tMUL    r7,  r7,  r11              \n"
    "\tAND    r7,  r12, r7, LSR #8       \n"
    "\tMUL    r8,  r8,  r11              \n"
    "\tAND    r8,  r8,  r12, LSL #8      \n"
    "\tORR    r10, r7,  r8               \n"
    "\tADD    r10, r6,  r10              \n"

    "\tSTM    %[dst]!, {r9, r10}         \n" /* 2nd 2-way storing of processed dst values */

    "\tCMP    %[src], r2                 \n" /* if our current [src] pointer <= calculated marker */
    "\tBLE    0b                         \n" /* we could run 4-way processing -> go to dispatcher */
    "\tBGT    8f                         \n" /* else -> use simple one-by-one processing */

    /* END OF BLOCK OPTIMIZED FOR 0 < ALPHA < 255 */

    /* START OF BLOCK OPTIMIZED FOR ALPHA == 255 */

    "\t2:                                \n" /* ENTRY 1: LOADING [src] to registers */

    "\tLDM    %[src]!, {r3, r4, r5, r6}  \n" /* 4-way loading of source values to r3-r6 */

    "\tAND    r7, r3, r4                 \n" /* if not all alphas == 255 -> */
    "\tAND    r8, r5, r6                 \n"
    "\tAND    r9, r7, r8                 \n"
    "\tCMP    r14, r9, LSR #24           \n"
    "\tBNE    4f                         \n" /* -> go to alpha == 0 check */

    "\t3:                                \n" /* ENTRY 2: [src] already loaded by DISPATCHER */

    "\tSTM    %[dst]!, {r3, r4, r5, r6}  \n" /* all alphas == 255 -> 4-way copy [src] to [dst] */

    "\tCMP    %[src], r2                 \n" /* if our current [src] array pointer <= marker */
    "\tBLE    2b                         \n" /* we could run 4-way processing */
                                             /* because now we're in ALPHA == 255 state */
                                             /* run next cycle with priority alpha == 255 checks */

    "\tBGT    8f                         \n" /* if our current [src] array pointer > marker */
                                             /* use simple one-by-one processing */

    "\t4:                                \n"

    "\tORR    r7, r3, r4                 \n" /* if not all alphas == 0 -> */
    "\tORR    r8, r5, r6                 \n"
    "\tORR    r9, r7, r8                 \n"
    "\tLSRS   r9, #24                    \n"
    "\tBNE    1b                         \n" /* -> go to general processing mode */
                                             /* (we already checked for alpha == 255) */

    "\tADD    %[dst], %[dst], #16        \n" /* all src alphas == 0 -> do not change dst values */

    "\tCMP    %[src], r2                 \n" /* if our current [src] array pointer <= marker */
    "\tBLE    5f                         \n" /* we could run 4-way processing one more time */
                                             /* because now we're in ALPHA == 0 state */
                                             /* run next cycle with priority alpha == 0 checks */

    "\tBGT    8f                         \n" /* if our current [src] array pointer > marker */
                                             /* use simple one-by-one processing */

    /* END OF BLOCK OPTIMIZED FOR ALPHA == 255 */

    /* START OF BLOCK OPTIMIZED FOR ALPHA == 0 */

    "\t5:                                \n" /* ENTRY 1: LOADING [src] to registers */

    "\tLDM    %[src]!, {r3, r4, r5, r6}  \n" /* 4-way loading of source values to r3-r6 */

    "\tORR    r7, r3, r4                 \n" /* if not all alphas == 0 -> */
    "\tORR    r8, r5, r6                 \n"
    "\tORR    r9, r7, r8                 \n"
    "\tLSRS   r9, #24                    \n"
    "\tBNE    7f                         \n" /* -> go to alpha == 255 check */

    "\t6:                                \n" /* ENTRY 2: [src] already loaded by DISPATCHER */

    "\tADD    %[dst], %[dst], #16        \n" /* all src alphas == 0 -> do not change dst values */

    "\tCMP    %[src], r2                 \n" /* if our current [src] array pointer <= marker */
    "\tBLE    5b                         \n" /* we could run 4-way processing one more time */
                                             /* because now we're in ALPHA == 0 state */
                                             /* run next cycle with priority alpha == 0 checks */

    "\tBGT    8f                         \n" /* if our current [src] array pointer > marker */
                                             /* use simple one-by-one processing */
    "\t7:                                \n"

    "\tAND    r7, r3, r4                 \n" /* if not all alphas == 255 -> */
    "\tAND    r8, r5, r6                 \n"
    "\tAND    r9, r7, r8                 \n"
    "\tCMP    r14, r9, LSR #24           \n"
    "\tBNE    1b                         \n" /* -> go to general processing mode */
                                             /* (we already checked for alpha == 0) */

    "\tSTM    %[dst]!, {r3, r4, r5, r6}  \n" /* all alphas == 255 -> 4-way copy [src] to [dst] */

    "\tCMP    %[src], r2                 \n" /* if our current [src] array pointer <= marker */
    "\tBLE    2b                         \n" /* we could run 4-way processing one more time */
                                             /* because now we're in ALPHA == 255 state */
                                             /* run next cycle with priority alpha == 255 checks */

    "\tBGT    8f                         \n" /* if our current [src] array pointer > marker */
                                             /* use simple one-by-one processing */

    /* END OF BLOCK OPTIMIZED FOR ALPHA == 0 */

    /* START OF TAIL BLOCK */
    /* (used when array is too small to be processed with 4-way algorithm)*/

    "\t8:                                \n"

    "\tADD    r2, r2, #16                \n" /* now r2 points to the element just after array */
                                             /* we've done r2 = r2 - 16 at procedure start */

    "\tCMP    %[src], r2                 \n" /* if our current [src] array pointer > final marker */
    "\tBEQ    9f                         \n" /* goto EXIT */

    /* TAIL PROCESSING BLOCK 1 */

    "\tLDR    r3, [%[src]], #4           \n" /* r3 = *src, src++ */
    "\tLDR    r7, [%[dst]]               \n" /* r7 = *dst */

    "\tLSR    r11, r3,  #24              \n" /* extracting alpha from source */
    "\tAND    r9,  r12, r7               \n" /* r9 = br masked by r12 (0xff00ff) */
    "\tRSB    r11, r11, #256             \n" /* subtracting the alpha from 255 -> r11 = scale */
    "\tAND    r10, r12, r7, LSR #8       \n" /* r10 = ag masked by r12 (0xff00ff) */
    "\tMUL    r9,  r9,  r11              \n" /* br = br * scale */
    "\tAND    r9,  r12, r9, LSR #8       \n" /* lsr br by 8 and mask it */
    "\tMUL    r10, r10, r11              \n" /* ag = ag * scale */
    "\tAND    r10, r10, r12, LSL #8      \n" /* mask ag with reverse mask */
    "\tORR    r7,  r9,  r10              \n" /* br | ag */
    "\tADD    r7,  r3,  r7               \n" /* dst = src + calc dest(r8) */

    "\tSTR    r7, [%[dst]], #4           \n" /* *dst = r7; dst++ */

    "\tCMP    %[src], r2                 \n" /* if our current [src] array pointer > final marker */
    "\tBEQ    9f                         \n" /* goto EXIT */

    /* TAIL PROCESSING BLOCK 2 */

    "\tLDR    r3, [%[src]], #4           \n" /* see TAIL PROCESSING BLOCK 1 */
    "\tLDR    r7, [%[dst]]               \n"

    "\tLSR    r11, r3,  #24              \n"
    "\tAND    r9,  r12, r7               \n"
    "\tRSB    r11, r11, #256             \n"
    "\tAND    r10, r12, r7, LSR #8       \n"
    "\tMUL    r9,  r9,  r11              \n"
    "\tAND    r9,  r12, r9, LSR #8       \n"
    "\tMUL    r10, r10, r11              \n"
    "\tAND    r10, r10, r12, LSL #8      \n"
    "\tORR    r7,  r9,  r10              \n"
    "\tADD    r7,  r3,  r7               \n"

    "\tSTR    r7, [%[dst]], #4           \n"

    "\tCMP    %[src], r2                 \n"
    "\tBEQ    9f                         \n"

    /* TAIL PROCESSING BLOCK 3 */

    "\tLDR    r3, [%[src]], #4           \n" /* see TAIL PROCESSING BLOCK 1 */
    "\tLDR    r7, [%[dst]]               \n"

    "\tLSR    r11, r3,  #24              \n"
    "\tAND    r9,  r12, r7               \n"
    "\tRSB    r11, r11, #256             \n"
    "\tAND    r10, r12, r7, LSR #8       \n"
    "\tMUL    r9,  r9,  r11              \n"
    "\tAND    r9,  r12, r9, LSR #8       \n"
    "\tMUL    r10, r10, r11              \n"
    "\tAND    r10, r10, r12, LSL #8      \n"
    "\tORR    r7,  r9,  r10              \n"
    "\tADD    r7,  r3,  r7               \n"

    "\tSTR    r7, [%[dst]], #4           \n"

    /* END OF TAIL BLOCK */

    "\t9:                                \n" /* EXIT */

    "\tLDMIA  r13!, {r4-r12, r14}        \n" /* restoring r4-r12, lr from stack */
    "\tBX     lr                         \n" /* return */

    : [dst] "+r" (dst), [src] "+r" (src)
    :
    : "cc", "r2", "r3", "memory"

    );

}

#define	S32A_Opaque_BlitRow32_PROC S32A_Opaque_BlitRow32_arm_test_alpha
#else /* !defined(TEST_SRC_ALPHA) */

static void S32A_Opaque_BlitRow32_arm(SkPMColor* SK_RESTRICT dst,
                                  const SkPMColor* SK_RESTRICT src,
                                  int count, U8CPU alpha) {

    SkASSERT(255 == alpha);

    /* Does not support the TEST_SRC_ALPHA case */
    asm volatile (
                  "cmp    %[count], #0               \n\t" /* comparing count with 0 */
                  "beq    3f                         \n\t" /* if zero exit */

                  "mov    ip, #0xff                  \n\t" /* load the 0xff mask in ip */
                  "orr    ip, ip, ip, lsl #16        \n\t" /* convert it to 0xff00ff in ip */

                  "cmp    %[count], #2               \n\t" /* compare count with 2 */
                  "blt    2f                         \n\t" /* if less than 2 -> single loop */

                  /* Double Loop */
                  "1:                                \n\t" /* <double loop> */
                  "ldm    %[src]!, {r5,r6}           \n\t" /* load the src(s) at r5-r6 */
                  "ldm    %[dst], {r7,r8}            \n\t" /* loading dst(s) into r7-r8 */
                  "lsr    r4, r5, #24                \n\t" /* extracting the alpha from source and storing it to r4 */

                  /* ----------- */
                  "and    r9, ip, r7                 \n\t" /* r9 = br masked by ip */
                  "rsb    r4, r4, #256               \n\t" /* subtracting the alpha from 256 -> r4=scale */
                  "and    r10, ip, r7, lsr #8        \n\t" /* r10 = ag masked by ip */

                  "mul    r9, r9, r4                 \n\t" /* br = br * scale */
                  "mul    r10, r10, r4               \n\t" /* ag = ag * scale */
                  "and    r9, ip, r9, lsr #8         \n\t" /* lsr br by 8 and mask it */

                  "and    r10, r10, ip, lsl #8       \n\t" /* mask ag with reverse mask */
                  "lsr    r4, r6, #24                \n\t" /* extracting the alpha from source and storing it to r4 */
                  "orr    r7, r9, r10                \n\t" /* br | ag*/

                  "add    r7, r5, r7                 \n\t" /* dst = src + calc dest(r7) */
                  "rsb    r4, r4, #256               \n\t" /* subtracting the alpha from 255 -> r4=scale */

                  /* ----------- */
                  "and    r9, ip, r8                 \n\t" /* r9 = br masked by ip */

                  "and    r10, ip, r8, lsr #8        \n\t" /* r10 = ag masked by ip */
                  "mul    r9, r9, r4                 \n\t" /* br = br * scale */
                  "sub    %[count], %[count], #2     \n\t"
                  "mul    r10, r10, r4               \n\t" /* ag = ag * scale */

                  "and    r9, ip, r9, lsr #8         \n\t" /* lsr br by 8 and mask it */
                  "and    r10, r10, ip, lsl #8       \n\t" /* mask ag with reverse mask */
                  "cmp    %[count], #1               \n\t" /* comparing count with 1 */
                  "orr    r8, r9, r10                \n\t" /* br | ag */

                  "add    r8, r6, r8                 \n\t" /* dst = src + calc dest(r8) */

                  /* ----------------- */
                  "stm    %[dst]!, {r7,r8}           \n\t" /* *dst = r7, increment dst by two (each times 4) */
                  /* ----------------- */

                  "bgt    1b                         \n\t" /* if greater than 1 -> reloop */
                  "blt    3f                         \n\t" /* if less than 1 -> exit */

                  /* Single Loop */
                  "2:                                \n\t" /* <single loop> */
                  "ldr    r5, [%[src]], #4           \n\t" /* load the src pointer into r5 r5=src */
                  "ldr    r7, [%[dst]]               \n\t" /* loading dst into r7 */
                  "lsr    r4, r5, #24                \n\t" /* extracting the alpha from source and storing it to r4 */

                  /* ----------- */
                  "and    r9, ip, r7                 \n\t" /* r9 = br masked by ip */
                  "rsb    r4, r4, #256               \n\t" /* subtracting the alpha from 256 -> r4=scale */

                  "and    r10, ip, r7, lsr #8        \n\t" /* r10 = ag masked by ip */
                  "mul    r9, r9, r4                 \n\t" /* br = br * scale */
                  "mul    r10, r10, r4               \n\t" /* ag = ag * scale */
                  "and    r9, ip, r9, lsr #8         \n\t" /* lsr br by 8 and mask it */

                  "and    r10, r10, ip, lsl #8       \n\t" /* mask ag */
                  "orr    r7, r9, r10                \n\t" /* br | ag */

                  "add    r7, r5, r7                 \n\t" /* *dst = src + calc dest(r7) */

                  /* ----------------- */
                  "str    r7, [%[dst]], #4           \n\t" /* *dst = r7, increment dst by one (times 4) */
                  /* ----------------- */

                  "3:                                \n\t" /* <exit> */
                  : [dst] "+r" (dst), [src] "+r" (src), [count] "+r" (count)
                  :
                  : "cc", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "ip", "memory"
                  );
}
#define	S32A_Opaque_BlitRow32_PROC	S32A_Opaque_BlitRow32_arm
#endif /* !defined(TEST_SRC_ALPHA) */
#else /* ... #elif defined (__ARM_ARCH__) */
#define	S32A_Opaque_BlitRow32_PROC	NULL
#endif


#if defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN) && defined(ENABLE_OPTIMIZED_S32A_BLITTERS)

/* External function in file S32A_Blend_BlitRow32_neon.S */
extern "C" void S32A_Blend_BlitRow32_neon(SkPMColor* SK_RESTRICT dst,
                                          const SkPMColor* SK_RESTRICT src,
                                          int count, U8CPU alpha);

#define S32A_Blend_BlitRow32_PROC  S32A_Blend_BlitRow32_neon

#else

/*
 * ARM asm version of S32A_Blend_BlitRow32
 */
static void S32A_Blend_BlitRow32_arm(SkPMColor* SK_RESTRICT dst,
                                 const SkPMColor* SK_RESTRICT src,
                                 int count, U8CPU alpha) {
    asm volatile (
                  "cmp    %[count], #0               \n\t" /* comparing count with 0 */
                  "beq    3f                         \n\t" /* if zero exit */

                  "mov    r12, #0xff                 \n\t" /* load the 0xff mask in r12 */
                  "orr    r12, r12, r12, lsl #16     \n\t" /* convert it to 0xff00ff in r12 */

                  /* src1,2_scale */
                  "add    %[alpha], %[alpha], #1     \n\t" /* loading %[alpha]=src_scale=alpha+1 */

                  "cmp    %[count], #2               \n\t" /* comparing count with 2 */
                  "blt    2f                         \n\t" /* if less than 2 -> single loop */

                  /* Double Loop */
                  "1:                                \n\t" /* <double loop> */
                  "ldm    %[src]!, {r5, r6}          \n\t" /* loading src pointers into r5 and r6 */
                  "ldm    %[dst], {r7, r8}           \n\t" /* loading dst pointers into r7 and r8 */

                  /* dst1_scale and dst2_scale*/
                  "lsr    r9, r5, #24                \n\t" /* src >> 24 */
                  "lsr    r10, r6, #24               \n\t" /* src >> 24 */
                  "smulbb r9, r9, %[alpha]           \n\t" /* r9 = SkMulS16 r9 with src_scale */
                  "smulbb r10, r10, %[alpha]         \n\t" /* r10 = SkMulS16 r10 with src_scale */
                  "lsr    r9, r9, #8                 \n\t" /* r9 >> 8 */
                  "lsr    r10, r10, #8               \n\t" /* r10 >> 8 */
                  "rsb    r9, r9, #256               \n\t" /* dst1_scale = r9 = 255 - r9 + 1 */
                  "rsb    r10, r10, #256             \n\t" /* dst2_scale = r10 = 255 - r10 + 1 */

                  /* ---------------------- */

                  /* src1, src1_scale */
                  "and    r11, r12, r5, lsr #8       \n\t" /* ag = r11 = r5 masked by r12 lsr by #8 */
                  "and    r4, r12, r5                \n\t" /* rb = r4 = r5 masked by r12 */
                  "mul    r11, r11, %[alpha]         \n\t" /* ag = r11 times src_scale */
                  "mul    r4, r4, %[alpha]           \n\t" /* rb = r4 times src_scale */
                  "and    r11, r11, r12, lsl #8      \n\t" /* ag masked by reverse mask (r12) */
                  "and    r4, r12, r4, lsr #8        \n\t" /* rb masked by mask (r12) */
                  "orr    r5, r11, r4                \n\t" /* r5 = (src1, src_scale) */

                  /* dst1, dst1_scale */
                  "and    r11, r12, r7, lsr #8       \n\t" /* ag = r11 = r7 masked by r12 lsr by #8 */
                  "and    r4, r12, r7                \n\t" /* rb = r4 = r7 masked by r12 */
                  "mul    r11, r11, r9               \n\t" /* ag = r11 times dst_scale (r9) */
                  "mul    r4, r4, r9                 \n\t" /* rb = r4 times dst_scale (r9) */
                  "and    r11, r11, r12, lsl #8      \n\t" /* ag masked by reverse mask (r12) */
                  "and    r4, r12, r4, lsr #8        \n\t" /* rb masked by mask (r12) */
                  "orr    r9, r11, r4                \n\t" /* r9 = (dst1, dst_scale) */

                  /* ---------------------- */
                  "add    r9, r5, r9                 \n\t" /* *dst = src plus dst both scaled */
                  /* ---------------------- */

                  /* ====================== */

                  /* src2, src2_scale */
                  "and    r11, r12, r6, lsr #8       \n\t" /* ag = r11 = r6 masked by r12 lsr by #8 */
                  "and    r4, r12, r6                \n\t" /* rb = r4 = r6 masked by r12 */
                  "mul    r11, r11, %[alpha]         \n\t" /* ag = r11 times src_scale */
                  "mul    r4, r4, %[alpha]           \n\t" /* rb = r4 times src_scale */
                  "and    r11, r11, r12, lsl #8      \n\t" /* ag masked by reverse mask (r12) */
                  "and    r4, r12, r4, lsr #8        \n\t" /* rb masked by mask (r12) */
                  "orr    r6, r11, r4                \n\t" /* r6 = (src2, src_scale) */

                  /* dst2, dst2_scale */
                  "and    r11, r12, r8, lsr #8       \n\t" /* ag = r11 = r8 masked by r12 lsr by #8 */
                  "and    r4, r12, r8                \n\t" /* rb = r4 = r8 masked by r12 */
                  "mul    r11, r11, r10              \n\t" /* ag = r11 times dst_scale (r10) */
                  "mul    r4, r4, r10                \n\t" /* rb = r4 times dst_scale (r6) */
                  "and    r11, r11, r12, lsl #8      \n\t" /* ag masked by reverse mask (r12) */
                  "and    r4, r12, r4, lsr #8        \n\t" /* rb masked by mask (r12) */
                  "orr    r10, r11, r4               \n\t" /* r10 = (dst2, dst_scale) */

                  "sub    %[count], %[count], #2     \n\t" /* decrease count by 2 */
                  /* ---------------------- */
                  "add    r10, r6, r10               \n\t" /* *dst = src plus dst both scaled */
                  /* ---------------------- */
                  "cmp    %[count], #1               \n\t" /* compare count with 1 */
                  /* ----------------- */
                  "stm    %[dst]!, {r9, r10}         \n\t" /* copy r9 and r10 to r7 and r8 respectively */
                  /* ----------------- */

                  "bgt    1b                         \n\t" /* if %[count] greater than 1 reloop */
                  "blt    3f                         \n\t" /* if %[count] less than 1 exit */
                                                           /* else get into the single loop */
                  /* Single Loop */
                  "2:                                \n\t" /* <single loop> */
                  "ldr    r5, [%[src]], #4           \n\t" /* loading src pointer into r5: r5=src */
                  "ldr    r7, [%[dst]]               \n\t" /* loading dst pointer into r7: r7=dst */

                  "lsr    r6, r5, #24                \n\t" /* src >> 24 */
                  "and    r8, r12, r5, lsr #8        \n\t" /* ag = r8 = r5 masked by r12 lsr by #8 */
                  "smulbb r6, r6, %[alpha]           \n\t" /* r6 = SkMulS16 with src_scale */
                  "and    r9, r12, r5                \n\t" /* rb = r9 = r5 masked by r12 */
                  "lsr    r6, r6, #8                 \n\t" /* r6 >> 8 */
                  "mul    r8, r8, %[alpha]           \n\t" /* ag = r8 times scale */
                  "rsb    r6, r6, #256               \n\t" /* r6 = 255 - r6 + 1 */

                  /* src, src_scale */
                  "mul    r9, r9, %[alpha]           \n\t" /* rb = r9 times scale */
                  "and    r8, r8, r12, lsl #8        \n\t" /* ag masked by reverse mask (r12) */
                  "and    r9, r12, r9, lsr #8        \n\t" /* rb masked by mask (r12) */
                  "orr    r10, r8, r9                \n\t" /* r10 = (scr, src_scale) */

                  /* dst, dst_scale */
                  "and    r8, r12, r7, lsr #8        \n\t" /* ag = r8 = r7 masked by r12 lsr by #8 */
                  "and    r9, r12, r7                \n\t" /* rb = r9 = r7 masked by r12 */
                  "mul    r8, r8, r6                 \n\t" /* ag = r8 times scale (r6) */
                  "mul    r9, r9, r6                 \n\t" /* rb = r9 times scale (r6) */
                  "and    r8, r8, r12, lsl #8        \n\t" /* ag masked by reverse mask (r12) */
                  "and    r9, r12, r9, lsr #8        \n\t" /* rb masked by mask (r12) */
                  "orr    r7, r8, r9                 \n\t" /* r7 = (dst, dst_scale) */

                  "add    r10, r7, r10               \n\t" /* *dst = src plus dst both scaled */

                  /* ----------------- */
                  "str    r10, [%[dst]], #4          \n\t" /* *dst = r10, postincrement dst by one (times 4) */
                  /* ----------------- */

                  "3:                                \n\t" /* <exit> */
                  : [dst] "+r" (dst), [src] "+r" (src), [count] "+r" (count), [alpha] "+r" (alpha)
                  :
                  : "cc", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "memory"
                  );

}
#define	S32A_Blend_BlitRow32_PROC	S32A_Blend_BlitRow32_arm
#endif


/* Neon version of S32_Blend_BlitRow32()
 * portable version is in src/core/SkBlitRow_D32.cpp
 */
#if defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN)
static void S32_Blend_BlitRow32_neon(SkPMColor* SK_RESTRICT dst,
                                const SkPMColor* SK_RESTRICT src,
                                int count, U8CPU alpha) {
    SkASSERT(alpha <= 255);
    if (count > 0) {
        uint16_t src_scale = SkAlpha255To256(alpha);
        uint16_t dst_scale = 256 - src_scale;

	/* run them N at a time through the NEON unit */
	/* note that each 1 is 4 bytes, each treated exactly the same,
	 * so we can work under that guise. We *do* know that the src&dst
	 * will be 32-bit aligned quantities, so we can specify that on
	 * the load/store ops and do a neon 'reinterpret' to get us to
	 * byte-sized (pun intended) pieces that we widen/multiply/shift
	 * we're limited at 128 bits in the wide ops, which is 8x16bits
	 * or a pair of 32 bit src/dsts.
	 */
	/* we *could* manually unroll this loop so that we load 128 bits
	 * (as a pair of 64s) from each of src and dst, processing them
	 * in pieces. This might give us a little better management of
	 * the memory latency, but my initial attempts here did not
	 * produce an instruction stream that looked all that nice.
	 */
#define	UNROLL	2
	while (count >= UNROLL) {
	    uint8x8_t  src_raw, dst_raw, dst_final;
	    uint16x8_t  src_wide, dst_wide;

	    /* get 64 bits of src, widen it, multiply by src_scale */
	    src_raw = vreinterpret_u8_u32(vld1_u32(src));
	    src_wide = vmovl_u8(src_raw);
	    /* gcc hoists vdupq_n_u16(), better than using vmulq_n_u16() */
	    src_wide = vmulq_u16 (src_wide, vdupq_n_u16(src_scale));

	    /* ditto with dst */
	    dst_raw = vreinterpret_u8_u32(vld1_u32(dst));
	    dst_wide = vmovl_u8(dst_raw);

	    /* combine add with dst multiply into mul-accumulate */
	    dst_wide = vmlaq_u16(src_wide, dst_wide, vdupq_n_u16(dst_scale));

	    dst_final = vshrn_n_u16(dst_wide, 8);
	    vst1_u32(dst, vreinterpret_u32_u8(dst_final));

	    src += UNROLL;
	    dst += UNROLL;
	    count -= UNROLL;
	}
	/* RBE: well, i don't like how gcc manages src/dst across the above
	 * loop it's constantly calculating src+bias, dst+bias and it only
	 * adjusts the real ones when we leave the loop. Not sure why
	 * it's "hoisting down" (hoisting implies above in my lexicon ;))
	 * the adjustments to src/dst/count, but it does...
	 * (might be SSA-style internal logic...
	 */

#if	UNROLL == 2
	if (count == 1) {
            *dst = SkAlphaMulQ(*src, src_scale) + SkAlphaMulQ(*dst, dst_scale);
	}
#else
	if (count > 0) {
            do {
                *dst = SkAlphaMulQ(*src, src_scale) + SkAlphaMulQ(*dst, dst_scale);
                src += 1;
                dst += 1;
            } while (--count > 0);
	}
#endif

#undef	UNROLL
    }
}

#define	S32_Blend_BlitRow32_PROC	S32_Blend_BlitRow32_neon
#else
#define	S32_Blend_BlitRow32_PROC	NULL
#endif

///////////////////////////////////////////////////////////////////////////////

#if defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN) && defined(ENABLE_OPTIMIZED_S32A_BLITTERS)

/* This function was broken out to keep GCC from storing all registers on the stack
   even though they would not be used in the assembler code */
static __attribute__ ((noinline)) void S32A_D565_Opaque_Dither_Handle8(uint16_t * SK_RESTRICT dst,
                                                                       const SkPMColor* SK_RESTRICT src,
                                                                       int count, U8CPU alpha, int x, int y) {
    DITHER_565_SCAN(y);
    do {
        SkPMColor c = *src++;
        SkPMColorAssert(c);
        if (c) {
            unsigned a = SkGetPackedA32(c);

            // dither and alpha are just temporary variables to work-around
            // an ICE in debug.
            unsigned dither = DITHER_VALUE(x);
            unsigned alpha = SkAlpha255To256(a);
            int d = SkAlphaMul(dither, alpha);

            unsigned sr = SkGetPackedR32(c);
            unsigned sg = SkGetPackedG32(c);
            unsigned sb = SkGetPackedB32(c);
            sr = SkDITHER_R32_FOR_565(sr, d);
            sg = SkDITHER_G32_FOR_565(sg, d);
            sb = SkDITHER_B32_FOR_565(sb, d);

            uint32_t src_expanded = (sg << 24) | (sr << 13) | (sb << 2);
            uint32_t dst_expanded = SkExpand_rgb_16(*dst);
            dst_expanded = dst_expanded * (SkAlpha255To256(255 - a) >> 3);
            // now src and dst expanded are in g:11 r:10 x:1 b:10
            *dst = SkCompact_rgb_16((src_expanded + dst_expanded) >> 5);
        }
        dst += 1;
        DITHER_INC_X(x);
    } while (--count != 0);
}


static void S32A_D565_Opaque_Dither_neon(uint16_t * SK_RESTRICT dst,
                                         const SkPMColor* SK_RESTRICT src,
                                         int count, U8CPU alpha, int x, int y) {
    SkASSERT(255 == alpha);

    if (count >= 8) {
        asm volatile (
                    "pld            [%[src]]                        \n\t"   // Preload source
                    "pld            [%[dst]]                        \n\t"   // Preload destination pixels
                    "and            %[y], %[y], #0x03               \n\t"   // Mask y by 3
                    "vmov.i8        d31, #0x01                      \n\t"   // Set up alpha constant
                    "add            %[y], %[y], lsl #1              \n\t"   // and multiply with 12 to get the row offset
                    "and            %[x], %[x], #0x03               \n\t"   // Mask x by 3
                    "vmov.i16       q12, #256                       \n\t"   // Set up alpha constant
                    "add            %[y], %[matrix], %[y], lsl #2   \n\t"   //
                    "add            r7, %[x], %[y]                  \n\t"   //
                    "vld1.8         {d26}, [r7]                     \n\t"   // Load dither values
                    "add            %[x], %[count]                  \n\t"   //
                    "vmov.i16       q11, #0x3F                      \n\t"   // Set up green mask constant
                    "and            %[x], %[x], #0x03               \n\t"   // Mask x by 3
                    "vmovl.u8       q13, d26                        \n\t"   // Expand dither to 16-bit
                    "add            r7, %[x], %[y]                  \n\t"   //
                    "vmov.i16       q10, #0x1F                      \n\t"   // Set up blue mask constant
                    "vld1.8         {d28}, [r7]                     \n\t"   // Load iteration 2+ dither values
                    "ands           r7, %[count], #7                \n\t"   // Calculate first iteration increment
                    "moveq          r7, #8                          \n\t"   // Do full iteration?
                    "vmovl.u8       q14, d28                        \n\t"   // Expand dither to 16-bit
                    "vld4.8         {d0-d3}, [%[src]]               \n\t"   // Load eight source pixels
                    "vld1.16        {q3}, [%[dst]]                  \n\t"   // Load destination 565 pixels
                    "add            %[src], r7, lsl #2              \n\t"   // Increment source pointer
                    "add            %[dst], r7, lsl #1              \n\t"   // Increment destination buffer pointer
                    "subs           %[count], r7                    \n\t"   // Decrement loop counter
                    "sub            r7, %[dst], r7, lsl #1          \n\t"   // Save original destination pointer
                    "b              2f                              \n\t"
                    "1:                                             \n\t"
                    "vld4.8         {d0-d3}, [%[src]]!              \n\t"   // Load eight source pixels
                    "vld1.16        {q3}, [%[dst]]!                 \n\t"   // Load destination 565 pixels
                    "vst1.16        {q2}, [r7]                      \n\t"   // Write result to memory
                    "sub            r7, %[dst], #8*2                \n\t"   // Calculate next loop's destination pointer
                    "subs           %[count], #8                    \n\t"   // Decrement loop counter
                    "2:                                             \n\t"
                    "pld            [%[src]]                        \n\t"   // Preload destination pixels
                    "pld            [%[dst]]                        \n\t"   // Preload destination pixels
                    "vaddl.u8       q2, d3, d31                     \n\t"   // Add 1 to alpha to get 0-256
                    "vshr.u8        d16, d0, #5                     \n\t"   // Calculate source red subpixel
                    "vmul.u16       q2, q2, q13                     \n\t"   // Multiply alpha with dither value
                    "vsub.i8        d0, d16                         \n\t"   // red = (red - (red >> 5) + dither)
                    "vshrn.i16      d30, q2, #8                     \n\t"   // Shift and narrow result to 0-7
                    "vadd.i8        d0, d30                         \n\t"   //
                    "vshr.u8        d16, d2, #5                     \n\t"   // Calculate source blue subpixel
                    "vsub.i8        d2, d16                         \n\t"   // blue = (blue - (blue >> 5) + dither)
                    "vshr.u8        d16, d1, #6                     \n\t"   // Calculate source green subpixel
                    "vadd.i8        d2, d30                         \n\t"   //
                    "vsub.i8        d1, d16                         \n\t"   // green = (green - (green >> 6) + (dither >> 1))
                    "vshr.u8        d30, #1                         \n\t"   //
                    "vadd.i8        d1, d30                         \n\t"   //
                    "vsubw.u8       q2, q12, d3                     \n\t"   // Calculate inverse alpha 256-1
                    "vshr.u16       q8, q3, #5                      \n\t"   // Extract destination green pixel
                    "vshr.u16       q9, q3, #11                     \n\t"   // Extract destination red pixel
                    "vand           q8, q11                         \n\t"   // Shift green
                    "vand           q3, q10                         \n\t"   // Extract destination blue pixel
                    "vshr.u16       q2, #3                          \n\t"   // Shift alpha
                    "vshll.u8       q1, d2, #2                      \n\t"   // Calculate destination blue pixel
                    "vmla.i16       q1, q3, q2                      \n\t"   // ...and add to source pixel
                    "vshll.u8       q3, d1, #3                      \n\t"   // Calculate destination green pixel
                    "vmov.u8        q13, q14                        \n\t"   // Set dither matrix to iteration 2+ values
                    "vmla.i16       q3, q8, q2                      \n\t"   // ...and add to source pixel
                    "vshll.u8       q8, d0, #2                      \n\t"   // Calculate destination red pixel
                    "vmla.i16       q8, q9, q2                      \n\t"   // ...and add to source pixel
                    "vshr.u16       q1, #5                          \n\t"   // Pack blue pixel
                    "vand           q2, q1, q10                     \n\t"   //
                    "vshr.u16       q3, #5                          \n\t"   // Pack green pixel
                    "vsli.16        q2, q3, #5                      \n\t"   // ...and insert
                    "vshr.u16       q8, #5                          \n\t"   // Pack red pixel
                    "vsli.16        q2, q8, #11                     \n\t"   // ...and insert
                    "bne            1b                              \n\t"   // If inner loop counter != 0, loop
                    "vst1.16        {q2}, [r7]                      \n\t"   // Write result to memory
                    : [src] "+r" (src), [dst] "+r" (dst), [count] "+r" (count), [x] "+r" (x), [y] "+r" (y)
                    : [matrix] "r" (gDitherMatrix_Neon)
                    : "cc", "memory", "r7", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31"
                    );
    }
    else {
        S32A_D565_Opaque_Dither_Handle8(dst, src, count, alpha, x, y);
    }
}

#define	S32A_D565_Opaque_Dither_PROC S32A_D565_Opaque_Dither_neon

#elif defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN)

#undef	DEBUG_OPAQUE_DITHER

#if	defined(DEBUG_OPAQUE_DITHER)
static void showme8(char *str, void *p, int len)
{
	static char buf[256];
	char tbuf[32];
	int i;
	char *pc = (char*) p;
	sprintf(buf,"%8s:", str);
	for(i=0;i<len;i++) {
	    sprintf(tbuf, "   %02x", pc[i]);
	    strcat(buf, tbuf);
	}
	SkDebugf("%s\n", buf);
}
static void showme16(char *str, void *p, int len)
{
	static char buf[256];
	char tbuf[32];
	int i;
	uint16_t *pc = (uint16_t*) p;
	sprintf(buf,"%8s:", str);
	len = (len / sizeof(uint16_t));	/* passed as bytes */
	for(i=0;i<len;i++) {
	    sprintf(tbuf, " %04x", pc[i]);
	    strcat(buf, tbuf);
	}
	SkDebugf("%s\n", buf);
}
#endif

static void S32A_D565_Opaque_Dither_neon (uint16_t * SK_RESTRICT dst,
                                      const SkPMColor* SK_RESTRICT src,
                                      int count, U8CPU alpha, int x, int y) {
    SkASSERT(255 == alpha);

#define	UNROLL	8

    if (count >= UNROLL) {
	uint8x8_t dbase;

#if	defined(DEBUG_OPAQUE_DITHER)
	uint16_t tmpbuf[UNROLL];
	int td[UNROLL];
	int tdv[UNROLL];
	int ta[UNROLL];
	int tap[UNROLL];
	uint16_t in_dst[UNROLL];
	int offset = 0;
	int noisy = 0;
#endif

	const uint8_t *dstart = &gDitherMatrix_Neon[(y&3)*12 + (x&3)];
	dbase = vld1_u8(dstart);

        do {
	    uint8x8_t sr, sg, sb, sa, d;
	    uint16x8_t dst8, scale8, alpha8;
	    uint16x8_t dst_r, dst_g, dst_b;

#if	defined(DEBUG_OPAQUE_DITHER)
	/* calculate 8 elements worth into a temp buffer */
	{
	  int my_y = y;
	  int my_x = x;
	  SkPMColor* my_src = (SkPMColor*)src;
	  uint16_t* my_dst = dst;
	  int i;

          DITHER_565_SCAN(my_y);
          for(i=0;i<UNROLL;i++) {
            SkPMColor c = *my_src++;
            SkPMColorAssert(c);
            if (c) {
                unsigned a = SkGetPackedA32(c);
                
                int d = SkAlphaMul(DITHER_VALUE(my_x), SkAlpha255To256(a));
		tdv[i] = DITHER_VALUE(my_x);
		ta[i] = a;
		tap[i] = SkAlpha255To256(a);
		td[i] = d;
                
                unsigned sr = SkGetPackedR32(c);
                unsigned sg = SkGetPackedG32(c);
                unsigned sb = SkGetPackedB32(c);
                sr = SkDITHER_R32_FOR_565(sr, d);
                sg = SkDITHER_G32_FOR_565(sg, d);
                sb = SkDITHER_B32_FOR_565(sb, d);
                
                uint32_t src_expanded = (sg << 24) | (sr << 13) | (sb << 2);
                uint32_t dst_expanded = SkExpand_rgb_16(*my_dst);
                dst_expanded = dst_expanded * (SkAlpha255To256(255 - a) >> 3);
                // now src and dst expanded are in g:11 r:10 x:1 b:10
                tmpbuf[i] = SkCompact_rgb_16((src_expanded + dst_expanded) >> 5);
		td[i] = d;

            } else {
		tmpbuf[i] = *my_dst;
		ta[i] = tdv[i] = td[i] = 0xbeef;
	    }
	    in_dst[i] = *my_dst;
            my_dst += 1;
            DITHER_INC_X(my_x);
          }
	}
#endif

	    /* source is in ABGR */
	    {
		register uint8x8_t d0 asm("d0");
		register uint8x8_t d1 asm("d1");
		register uint8x8_t d2 asm("d2");
		register uint8x8_t d3 asm("d3");

		asm ("vld4.8	{d0-d3},[%4]  /* r=%P0 g=%P1 b=%P2 a=%P3 */"
		    : "=w" (d0), "=w" (d1), "=w" (d2), "=w" (d3)
		    : "r" (src)
                    );
		    sr = d0; sg = d1; sb = d2; sa = d3;
	    }

	    /* calculate 'd', which will be 0..7 */
	    /* dbase[] is 0..7; alpha is 0..256; 16 bits suffice */
#if defined(SK_BUILD_FOR_ANDROID)
	    /* SkAlpha255To256() semantic a+1 vs a+a>>7 */
	    alpha8 = vaddw_u8(vmovl_u8(sa), vdup_n_u8(1));
#else
	    alpha8 = vaddw_u8(vmovl_u8(sa), vshr_n_u8(sa, 7));
#endif
	    alpha8 = vmulq_u16(alpha8, vmovl_u8(dbase)); 
	    d = vshrn_n_u16(alpha8, 8);	/* narrowing too */
	    
	    /* sr = sr - (sr>>5) + d */
	    /* watching for 8-bit overflow.  d is 0..7; risky range of
	     * sr is >248; and then (sr>>5) is 7 so it offsets 'd';
	     * safe  as long as we do ((sr-sr>>5) + d) */
	    sr = vsub_u8(sr, vshr_n_u8(sr, 5));
	    sr = vadd_u8(sr, d);

	    /* sb = sb - (sb>>5) + d */
	    sb = vsub_u8(sb, vshr_n_u8(sb, 5));
	    sb = vadd_u8(sb, d);

	    /* sg = sg - (sg>>6) + d>>1; similar logic for overflows */
	    sg = vsub_u8(sg, vshr_n_u8(sg, 6));
	    sg = vadd_u8(sg, vshr_n_u8(d,1));

	    /* need to pick up 8 dst's -- at 16 bits each, 128 bits */
	    dst8 = vld1q_u16(dst);
	    dst_b = vandq_u16(dst8, vdupq_n_u16(0x001F));
	    dst_g = vandq_u16(vshrq_n_u16(dst8,5), vdupq_n_u16(0x003F));
	    dst_r = vshrq_n_u16(dst8,11);	/* clearing hi bits */

	    /* blend */
#if 1
	    /* SkAlpha255To256() semantic a+1 vs a+a>>7 */
	    /* originally 255-sa + 1 */
	    scale8 = vsubw_u8(vdupq_n_u16(256), sa);
#else
	    scale8 = vsubw_u8(vdupq_n_u16(255), sa);
	    scale8 = vaddq_u16(scale8, vshrq_n_u16(scale8, 7));
#endif

#if 1
	    /* combine the addq and mul, save 3 insns */
	    scale8 = vshrq_n_u16(scale8, 3);
	    dst_b = vmlaq_u16(vshll_n_u8(sb,2), dst_b, scale8);
	    dst_g = vmlaq_u16(vshll_n_u8(sg,3), dst_g, scale8);
	    dst_r = vmlaq_u16(vshll_n_u8(sr,2), dst_r, scale8);
#else
	    /* known correct, but +3 insns over above */
	    scale8 = vshrq_n_u16(scale8, 3);
	    dst_b = vmulq_u16(dst_b, scale8);
	    dst_g = vmulq_u16(dst_g, scale8);
	    dst_r = vmulq_u16(dst_r, scale8);

	    /* combine */
	    /* NB: vshll widens, need to preserve those bits */
	    dst_b = vaddq_u16(dst_b, vshll_n_u8(sb,2));
	    dst_g = vaddq_u16(dst_g, vshll_n_u8(sg,3));
	    dst_r = vaddq_u16(dst_r, vshll_n_u8(sr,2));
#endif

	    /* repack to store */
	    dst8 = vandq_u16(vshrq_n_u16(dst_b, 5), vdupq_n_u16(0x001F));
	    dst8 = vsliq_n_u16(dst8, vshrq_n_u16(dst_g, 5), 5);
	    dst8 = vsliq_n_u16(dst8, vshrq_n_u16(dst_r,5), 11);

	    vst1q_u16(dst, dst8);

#if	defined(DEBUG_OPAQUE_DITHER)
	    /* verify my 8 elements match the temp buffer */
	{
	   int i, bad=0;
	   static int invocation;

	   for (i=0;i<UNROLL;i++)
		if (tmpbuf[i] != dst[i]) bad=1;
	   if (bad) {
		SkDebugf("BAD S32A_D565_Opaque_Dither_neon(); invocation %d offset %d\n",
			invocation, offset);
		SkDebugf("  alpha 0x%x\n", alpha);
		for (i=0;i<UNROLL;i++)
		    SkDebugf("%2d: %s %04x w %04x id %04x s %08x d %04x %04x %04x %04x\n",
			i, ((tmpbuf[i] != dst[i])?"BAD":"got"),
			dst[i], tmpbuf[i], in_dst[i], src[i], td[i], tdv[i], tap[i], ta[i]);

		showme16("alpha8", &alpha8, sizeof(alpha8));
		showme16("scale8", &scale8, sizeof(scale8));
		showme8("d", &d, sizeof(d));
		showme16("dst8", &dst8, sizeof(dst8));
		showme16("dst_b", &dst_b, sizeof(dst_b));
		showme16("dst_g", &dst_g, sizeof(dst_g));
		showme16("dst_r", &dst_r, sizeof(dst_r));
		showme8("sb", &sb, sizeof(sb));
		showme8("sg", &sg, sizeof(sg));
		showme8("sr", &sr, sizeof(sr));

		/* cop out */
		return;
	   }
	   offset += UNROLL;
	   invocation++;
	}
#endif

            dst += UNROLL;
	    src += UNROLL;
	    count -= UNROLL;
	    /* skip x += UNROLL, since it's unchanged mod-4 */
        } while (count >= UNROLL);
    }
#undef	UNROLL

    /* residuals */
    if (count > 0) {
        DITHER_565_SCAN(y);
        do {
            SkPMColor c = *src++;
            SkPMColorAssert(c);
            if (c) {
                unsigned a = SkGetPackedA32(c);
                
                // dither and alpha are just temporary variables to work-around
                // an ICE in debug.
                unsigned dither = DITHER_VALUE(x);
                unsigned alpha = SkAlpha255To256(a);
                int d = SkAlphaMul(dither, alpha);
                
                unsigned sr = SkGetPackedR32(c);
                unsigned sg = SkGetPackedG32(c);
                unsigned sb = SkGetPackedB32(c);
                sr = SkDITHER_R32_FOR_565(sr, d);
                sg = SkDITHER_G32_FOR_565(sg, d);
                sb = SkDITHER_B32_FOR_565(sb, d);
                
                uint32_t src_expanded = (sg << 24) | (sr << 13) | (sb << 2);
                uint32_t dst_expanded = SkExpand_rgb_16(*dst);
                dst_expanded = dst_expanded * (SkAlpha255To256(255 - a) >> 3);
                // now src and dst expanded are in g:11 r:10 x:1 b:10
                *dst = SkCompact_rgb_16((src_expanded + dst_expanded) >> 5);
            }
            dst += 1;
            DITHER_INC_X(x);
        } while (--count != 0);
    }
}

#define	S32A_D565_Opaque_Dither_PROC S32A_D565_Opaque_Dither_neon
#else
#define	S32A_D565_Opaque_Dither_PROC NULL
#endif

///////////////////////////////////////////////////////////////////////////////

#if	defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN)
/* 2009/10/27: RBE says "a work in progress"; debugging says ok;
 * speedup untested, but ARM version is 26 insns/iteration and
 * this NEON version is 21 insns/iteration-of-8 (2.62insns/element)
 * which is 10x the native version; that's pure instruction counts,
 * not accounting for any instruction or memory latencies.
 */

#undef	DEBUG_S32_OPAQUE_DITHER

static void S32_D565_Opaque_Dither_neon(uint16_t* SK_RESTRICT dst,
                                     const SkPMColor* SK_RESTRICT src,
                                     int count, U8CPU alpha, int x, int y) {
    SkASSERT(255 == alpha);

#define	UNROLL	8
    if (count >= UNROLL) {
	uint8x8_t d;
	const uint8_t *dstart = &gDitherMatrix_Neon[(y&3)*12 + (x&3)];
	d = vld1_u8(dstart);

	while (count >= UNROLL) {
	    uint8x8_t sr, sg, sb, sa;
	    uint16x8_t dr, dg, db, da;
	    uint16x8_t dst8;

	    /* source is in ABGR ordering (R == lsb) */
	    {
		register uint8x8_t d0 asm("d0");
		register uint8x8_t d1 asm("d1");
		register uint8x8_t d2 asm("d2");
		register uint8x8_t d3 asm("d3");

		asm ("vld4.8	{d0-d3},[%4]  /* r=%P0 g=%P1 b=%P2 a=%P3 */"
		    : "=w" (d0), "=w" (d1), "=w" (d2), "=w" (d3)
		    : "r" (src)
                    );
		    sr = d0; sg = d1; sb = d2; sa = d3;
	    }
	    /* XXX: if we want to prefetch, hide it in the above asm()
	     * using the gcc __builtin_prefetch(), the prefetch will
	     * fall to the bottom of the loop -- it won't stick up
	     * at the top of the loop, just after the vld4.
	     */

	    /* sr = sr - (sr>>5) + d */
	    sr = vsub_u8(sr, vshr_n_u8(sr, 5));
	    dr = vaddl_u8(sr, d);

	    /* sb = sb - (sb>>5) + d */
	    sb = vsub_u8(sb, vshr_n_u8(sb, 5));
	    db = vaddl_u8(sb, d);

	    /* sg = sg - (sg>>6) + d>>1; similar logic for overflows */
	    sg = vsub_u8(sg, vshr_n_u8(sg, 6));
	    dg = vaddl_u8(sg, vshr_n_u8(d,1));
	    /* XXX: check that the "d>>1" here is hoisted */

	    /* pack high bits of each into 565 format  (rgb, b is lsb) */
	    dst8 = vshrq_n_u16(db, 3);
	    dst8 = vsliq_n_u16(dst8, vshrq_n_u16(dg, 2), 5);
	    dst8 = vsliq_n_u16(dst8, vshrq_n_u16(dr,3), 11);

	    /* store it */
	    vst1q_u16(dst, dst8);

#if	defined(DEBUG_S32_OPAQUE_DITHER)
	    /* always good to know if we generated good results */
	    {
		int i, myx = x, myy = y;
		DITHER_565_SCAN(myy);
		for (i=0;i<UNROLL;i++) {
		    SkPMColor c = src[i];
		    unsigned dither = DITHER_VALUE(myx);
		    uint16_t val = SkDitherRGB32To565(c, dither);
		    if (val != dst[i]) {
			SkDebugf("RBE: src %08x dither %02x, want %04x got %04x dbas[i] %02x\n",
			    c, dither, val, dst[i], dstart[i]);
		    }
		    DITHER_INC_X(myx);
		}
	    }
#endif

	    dst += UNROLL;
	    src += UNROLL;
	    count -= UNROLL;
	    x += UNROLL;		/* probably superfluous */
	}
    }
#undef	UNROLL

    /* residuals */
    if (count > 0) {
        DITHER_565_SCAN(y);
        do {
            SkPMColor c = *src++;
            SkPMColorAssert(c);
            SkASSERT(SkGetPackedA32(c) == 255);

            unsigned dither = DITHER_VALUE(x);
            *dst++ = SkDitherRGB32To565(c, dither);
            DITHER_INC_X(x);
        } while (--count != 0);
    }
}

#define	S32_D565_Opaque_Dither_PROC S32_D565_Opaque_Dither_neon
#else
#define	S32_D565_Opaque_Dither_PROC NULL
#endif

#if defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN)
static void Color32_neon(SkPMColor* dst, const SkPMColor* src, int count,
                         SkPMColor color) {
    if (count <= 0) {
        return;
    }

    if (0 == color) {
        if (src != dst) {
            memcpy(dst, src, count * sizeof(SkPMColor));
        }
        return;
    }

    unsigned colorA = SkGetPackedA32(color);
    if (255 == colorA) {
        sk_memset32(dst, color, count);
    } else {
        unsigned scale = 256 - SkAlpha255To256(colorA);

        if (count >= 8) {
            // at the end of this assembly, count will have been decremented
            // to a negative value. That is, if count mod 8 = x, it will be
            // -8 +x coming out.
            asm volatile (
                PLD128(src, 0)

                "vdup.32    q0, %[color]                \n\t"

                PLD128(src, 128)

                // scale numerical interval [0-255], so load as 8 bits
                "vdup.8     d2, %[scale]                \n\t"

                PLD128(src, 256)

                "subs       %[count], %[count], #8      \n\t"

                PLD128(src, 384)

                "Loop_Color32:                          \n\t"

                // load src color, 8 pixels, 4 64 bit registers
                // (and increment src).
                "vld1.32    {d4-d7}, [%[src]]!          \n\t"

                PLD128(src, 384)

                // multiply long by scale, 64 bits at a time,
                // destination into a 128 bit register.
                "vmull.u8   q4, d4, d2                  \n\t"
                "vmull.u8   q5, d5, d2                  \n\t"
                "vmull.u8   q6, d6, d2                  \n\t"
                "vmull.u8   q7, d7, d2                  \n\t"

                // shift the 128 bit registers, containing the 16
                // bit scaled values back to 8 bits, narrowing the
                // results to 64 bit registers.
                "vshrn.i16  d8, q4, #8                  \n\t"
                "vshrn.i16  d9, q5, #8                  \n\t"
                "vshrn.i16  d10, q6, #8                 \n\t"
                "vshrn.i16  d11, q7, #8                 \n\t"

                // adding back the color, using 128 bit registers.
                "vadd.i8    q6, q4, q0                  \n\t"
                "vadd.i8    q7, q5, q0                  \n\t"

                // store back the 8 calculated pixels (2 128 bit
                // registers), and increment dst.
                "vst1.32    {d12-d15}, [%[dst]]!        \n\t"

                "subs       %[count], %[count], #8      \n\t"
                "bge        Loop_Color32                \n\t"
                : [src] "+r" (src), [dst] "+r" (dst), [count] "+r" (count)
                : [color] "r" (color), [scale] "r" (scale)
                : "cc", "memory",
                  "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
                  "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15"
                          );
            // At this point, if we went through the inline assembly, count is
            // a negative value:
            // if the value is -8, there is no pixel left to process.
            // if the value is -7, there is one pixel left to process
            // ...
            // And'ing it with 7 will give us the number of pixels
            // left to process.
            count = count & 0x7;
        }

        while (count > 0) {
            *dst = color + SkAlphaMulQ(*src, scale);
            src += 1;
            dst += 1;
            count--;
        }
    }
}
#endif

///////////////////////////////////////////////////////////////////////////////

static const SkBlitRow::Proc platform_565_procs[] = {
    // no dither
    S32_D565_Opaque_PROC,
    S32_D565_Blend_PROC,
    S32A_D565_Opaque_PROC,
    S32A_D565_Blend_PROC,
    
    // dither
    S32_D565_Opaque_Dither_PROC,
    S32_D565_Blend_Dither_PROC,
    S32A_D565_Opaque_Dither_PROC,
    NULL,   // S32A_D565_Blend_Dither
};

static const SkBlitRow::Proc platform_4444_procs[] = {
    // no dither
    NULL,   // S32_D4444_Opaque,
    NULL,   // S32_D4444_Blend,
    NULL,   // S32A_D4444_Opaque,
    NULL,   // S32A_D4444_Blend,
    
    // dither
    NULL,   // S32_D4444_Opaque_Dither,
    NULL,   // S32_D4444_Blend_Dither,
    NULL,   // S32A_D4444_Opaque_Dither,
    NULL,   // S32A_D4444_Blend_Dither
};

static const SkBlitRow::Proc32 platform_32_procs[] = {
    NULL,   // S32_Opaque,
    S32_Blend_BlitRow32_PROC,		// S32_Blend,
    S32A_Opaque_BlitRow32_PROC,		// S32A_Opaque,
    S32A_Blend_BlitRow32_PROC		// S32A_Blend
};

SkBlitRow::Proc SkBlitRow::PlatformProcs4444(unsigned flags) {
    return platform_4444_procs[flags];
}

SkBlitRow::Proc SkBlitRow::PlatformProcs565(unsigned flags) {
    return platform_565_procs[flags];
}

SkBlitRow::Proc32 SkBlitRow::PlatformProcs32(unsigned flags) {
    return platform_32_procs[flags];
}

///////////////////////////////////////////////////////////////////////////////
SkBlitRow::ColorProc SkBlitRow::PlatformColorProc() {
#if defined(__ARM_HAVE_NEON) && defined(SK_CPU_LENDIAN)
    return Color32_neon;
#else
    return NULL;
#endif
}

SkBlitMask::ColorProc SkBlitMask::PlatformColorProcs(SkBitmap::Config dstConfig,
                                                     SkMask::Format maskFormat,
                                                     SkColor color) {
    return NULL;
}

SkBlitMask::BlitLCD16RowProc SkBlitMask::PlatformBlitRowProcs16(bool isOpaque) {
    return NULL;
}

SkBlitMask::RowProc SkBlitMask::PlatformRowProcs(SkBitmap::Config dstConfig,
                                                 SkMask::Format maskFormat,
                                                 RowFlags flags) {
    return NULL;
}
