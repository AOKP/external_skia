/*
**
** Copyright 2012, Samsung Electronics Co. LTD
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

    .arch armv7-a
    .text
    .global blitAntiH_NEON
    .type   blitAntiH_NEON, %function
blitAntiH_NEON:

    .fnstart

    @r0     antialias
    @r1     device
    @r2     runs
    @r3     srcExpanded
    @r4     scale


    @r5     count
    @r6     temp
    @r7     0x7E0
    @r8     ~(0x7E0)
    @r9     scale5
    @r10    src32
    @r11    temp2
    @r12    temp3
    @r14    count

    @d16    = {0x07E0, 0x07E0, 0x07E0, 0x07E0}
    @d17    = {0xF81F, 0XF81F, 0XF81F, 0XF81F}
    @q12    = d16
    @q13    = d17


    stmfd       sp!, {r4-r12,r14}
    mov         r14, r13
    add         r14, #40
    ldr         r4, [r14]

    mov        r5, #0x7E
    vdup.16    d16, r5
    vshl.u16   d16, #4
    vmov.u16   d17, d16

    vmvn.u16   d18, d17
    vmov.u16   d19, d18

    vmovl.u16  q12, d16
    vmvn.u32  q13, q12

default:
    ldrh        r5, [r2]            @r5 = runs[0]
    cmp         r5, #0              @if (r5 <=0)
    bls         end_prog            @return

    add         r2, r5, lsl #1      @ runs += (count*2)
    ldrb        r6, [r0]            @ r6 = antialias[0]
    add         r0, r5              @ antialias += count
    cmp         r6, #0              @ if(r6 == 0)
    addeq       r1, r5, lsl #1      @ device += (count*2)
    beq         default             @ goto default

    add         r6, #1              @ antialias[0]++
    mul         r7, r6, r4          @ antialias[0] * scale = r7

    lsr         r8, r7, #11         @ SkAlpha255To256(aa) * scale >> 11
    mul         r10, r3, r8         @ r10 = scale5 * srcExpanded
    rsb         r9, r8, #32         @ r9 = 32 - scale5
    vdup.32     q10, r9             @ q10 = scale5
    vdup.32     q11, r10            @ q11 = src32

    mov         r7, #0x7E
    lsl         r7, #4              @ r7 = 0x7E0
    mvn         r8, r7              @ r8 = ~(0x7E0)

    mov         r14, r5, lsr #3
    cmp         r14, #0
    beq         start_fourbytes

eightbytes:
    vld1.16      {q0}, [r1]       @device load
    vand.u16    q1, q0, q8     @ c & 0X7E0
    vand.u16    q2, q0, q9     @ c &~(0x7E0)

    vmovl.u16   q3, d2
    vmovl.u16   q4, d4         @ conversion from 16bits to 32bits
    vshl.u32    q5, q3, #16    @ q5 = (c & 0x7E0) << 16
    vorr.u32    q6, q4, q5     @ q6 = (c & 0x7E0) << 16 | (c & ~(0x7E0))
    vmul.u32    q7, q6, q10    @ dst32(q7) = scale5 * SkExpand_rgb_16(*device)
    vadd.u32    q14, q7, q11   @ q14 = dst32 + src32
    vshr.u32    q15, q14, #5   @ q15 = (src32 + dst32) >> 5
    vshr.u32    q5, q15, #16   @ q5 = ( (src32 + dst32) >> 5 ) >> 16
    vand.u32    q6, q5, q12    @ q6 = (c >> 16) & (0x7E0)
    vand.u32    q7, q15, q13   @ q7 = (c & ~(0x7E0))
    vorr.u32    q14, q6, q7    @ q14 = ((c >> 16) & 0x7E0) | (c & ~(0x7E0))
    vmovn.u32   d0, q14

    vmovl.u16   q3, d3
    vmovl.u16   q4, d5         @ conversion from 16bits to 32bits
    vshl.u32    q5, q3, #16    @ q5 = (c & 0x7E0) << 16
    vorr.u32    q6, q4, q5     @ q6 = (c & 0x7E0) << 16 | (c & ~(0x7E0))
    vmul.u32    q7, q6, q10    @ dst32(q7) = scale5 * SkExpand_rgb_16(*device)
    vadd.u32    q14, q7, q11   @ q14 = dst32 + src32
    vshr.u32    q15, q14, #5   @ q15 = (src32 + dst32) >> 5
    vshr.u32    q5, q15, #16   @ q5 = ( (src32 + dst32) >> 5 ) >> 16
    vand.u32    q6, q5, q12    @ q6 = (c >> 16) & (0x7E0)
    vand.u32    q7, q15, q13   @ q7 = (c & ~(0x7E0))
    vorr.u32    q14, q6, q7    @ q14 = ((c >> 16) & 0x7E0) | (c & ~(0x7E0))
    vmovn.u32   d1, q14

    vst1.16     {q0}, [r1]

    add         r1, #16                  @ device++
    subs        r14, r14, #1
    bhi         eightbytes

start_fourbytes:
    and         r14, r5, #4
    cmp         r14, #0
    beq         start_onebyte

fourbytes:
    vld1.16     d0, [r1]            @ device load
    vand.u16    d1, d0, d16         @ c & 0x7E0
    vand.u16    d2, d0, d18         @ c & ~(0x7E0)
    vmovl.u16   q3, d1
    vmovl.u16   q4, d2              @ conversion from 16bits to 32bits

    vshl.u32    q5, q3, #16         @ q5 = (c & 0x7E0) << 16
    vorr.u32    q6, q4, q5          @ q6 = (c & 0x7E0) << 16 | (c & ~(0x7E0))
    vmul.u32    q7, q6, q10         @ dst32(q7) = scale5 * SkExpand_rgb_16(*device)
    vadd.u32    q3, q11, q7         @ q3 = dst32 + src32
    vshr.u32    q1, q3, #5          @ q1 = (src32 + dst32) >> 5
    vshr.u32    q2, q1, #16         @ q2 = ( (src32 + dst32) >> 5 ) >> 16

    vand.u32    q3, q2, q12         @ q3 = (c >> 16) & (0x7E0)
    vand.u32    q4, q1, q13         @ q4 = (c & ~(0x7E0))

    vorr.u32    q5, q4, q3          @ q6 = ((c >> 16) & 0x7E0) | (c & ~(0x7E0))
    vmovn.u32   d0, q5
    vst1.16     d0, [r1]
    add         r1, #8


start_onebyte:
    and         r14, r5, #3
    cmp         r14, #0
    beq         default
onebyte:
    ldrh        r6, [r1]            @ device load
    and         r5, r6, r7          @ r5 = c & 0x7E0
    and         r11, r6, r8         @ r11 = c & (~0x7E0)
    orr         r12, r11, r5, lsl #16  @ r12 = (c & (~0x7E0) ) | ((c & 0x7E0) << 16)

    mul         r6, r9, r12             @dst32(r5) = scale5 * SkExpand_rgb_16(*device)

    add         r5, r6, r10             @ src32 + dst32
    lsr         r6, r5, #5              @ (src32 + dst32) >> 5
    and         r12, r7, r6, lsr #16    @ (c>>16) & 2016
    and         r11, r8, r6             @ (c & ~(2016))
    orr         r5, r11, r12            @ (c & ~(2016) | ( (c>>16) & 2016)

    strh        r5, [r1]                @ *device = r5
    add         r1, #2                  @ device++
    subs        r14, r14, #1
    bhi         onebyte
    b           default


end_prog:
    ldmfd       sp!, {r4-r12, r14}
    mov         pc, lr

.fnend

    .global blitH_NEON
    .type   blitH_NEON, %function
blitH_NEON:

    .fnstart

    @r0     dst addr
    @r1     count
    @r2     src_expand
    @r3     scale

    @d16    = {0x07E0, 0x07E0, 0x07E0, 0x07E0}
    @d17    = {0xF81F, 0XF81F, 0XF81F, 0XF81F}
    @q12    = d16
    @q13    = d17

    stmfd       sp!, {r4-r12,r14}

    mov        r5, #0x7E
    vdup.16    d16, r5
    vshl.u16   d16, #4
    vmov.u16   d17, d16

    vmvn.u16   d18, d17
    vmov.u16   d19, d18

    vmovl.u16  q12, d16
    vmvn.u32   q13, q12

default_blitH:
    cmp         r1, #0
    bls         end_blitH

    vdup.32     q10, r3             @ q10 = scale
    vdup.32     q11, r2             @ q11 = src_expand

    mov         r7, #0x7E
    lsl         r7, #4              @ r7 = 0x7E0
    mvn         r8, r7              @ r8 = ~(0x7E0)

    mov         r14, r1, lsr #3
    cmp         r14, #0
    beq         start_fourpx

eightpx:
    vld1.16     {q0}, [r0]     @device load
    vand.u16    q1, q0, q8     @ c & 0X7E0
    vand.u16    q2, q0, q9     @ c &~(0x7E0)

    vmovl.u16   q3, d2
    vmovl.u16   q4, d4         @ conversion from 16bits to 32bits
    vshl.u32    q5, q3, #16    @ q5 = (c & 0x7E0) << 16
    vorr.u32    q6, q4, q5     @ q6 = (c & 0x7E0) << 16 | (c & ~(0x7E0))
    vmul.u32    q7, q6, q10    @ dst_expand(q7) = scale5 * SkExpand_rgb_16(*dst)
    vadd.u32    q14, q7, q11   @ q14 = dst_expand + src_expand
    vshr.u32    q15, q14, #5   @ q15 = (src_expand + dst_expand) >> 5
    vshr.u32    q5, q15, #16   @ q5 = ( (src_expand + dst_expand) >> 5 ) >> 16
    vand.u32    q6, q5, q12    @ q6 = (c >> 16) & (0x7E0)
    vand.u32    q7, q15, q13   @ q7 = (c & ~(0x7E0))
    vorr.u32    q14, q6, q7    @ q14 = ((c >> 16) & 0x7E0) | (c & ~(0x7E0))
    vmovn.u32   d0, q14

    vmovl.u16   q3, d3
    vmovl.u16   q4, d5         @ conversion from 16bits to 32bits
    vshl.u32    q5, q3, #16    @ q5 = (c & 0x7E0) << 16
    vorr.u32    q6, q4, q5     @ q6 = (c & 0x7E0) << 16 | (c & ~(0x7E0))
    vmul.u32    q7, q6, q10    @ dst_expand(q7) = scale * SkExpand_rgb_16(*device)
    vadd.u32    q14, q7, q11   @ q14 = dst_expand + src_expand
    vshr.u32    q15, q14, #5   @ q15 = (src_expand + dst_expand) >> 5
    vshr.u32    q5, q15, #16   @ q5 = ( (src_expand + dst_expand) >> 5 ) >> 16
    vand.u32    q6, q5, q12    @ q6 = (c >> 16) & (0x7E0)
    vand.u32    q7, q15, q13   @ q7 = (c & ~(0x7E0))
    vorr.32     q14, q6, q7    @ q14 = ((c >> 16) & 0x7E0) | (c & ~(0x7E0))
    vmovn.u32   d1, q14

    vst1.16     {q0}, [r0]

    add         r0, #16                  @ device++
    subs        r14, r14, #1
    bhi         eightpx

start_fourpx:
    and         r14, r1, #4
    cmp         r14, #0
    beq         start_onepx

fourpx:
    vld1.16     d0, [r0]            @ device load
    vand.u16    d1, d0, d16         @ c & 0x7E0
    vand.u16    d2, d0, d18         @ c & ~(0x7E0)
    vmovl.u16   q3, d1
    vmovl.u16   q4, d2              @ conversion from 16bits to 32bits

    vshl.u32    q5, q3, #16         @ q5 = (c & 0x7E0) << 16
    vorr.u32    q6, q4, q5          @ q6 = (c & 0x7E0) << 16 | (c & ~(0x7E0))
    vmul.u32    q7, q6, q10         @ dst32(q7) = scale5 * SkExpand_rgb_16(*device)
    vadd.u32    q3, q11, q7         @ q3 = dst32 + src32
    vshr.u32    q1, q3, #5          @ q1 = (src32 + dst32) >> 5
    vshr.u32    q2, q1, #16         @ q2 = ( (src32 + dst32) >> 5 ) >> 16

    vand.u32    q3, q2, q12         @ q3 = (c >> 16) & (0x7E0)
    vand.u32    q4, q1, q13         @ q4 = (c & ~(0x7E0))

    vorr.u32    q5, q4, q3          @ q6 = ((c >> 16) & 0x7E0) | (c & ~(0x7E0))
    vmovn.u32   d0, q5
    vst1.16     d0, [r0]
    add         r0, #8

start_onepx:
    and         r14, r1, #3
    cmp         r14, #0
    beq         end_blitH
onepx:
    ldrh        r6, [r0]            @ device load
    and         r5, r6, r7          @ r5 = c & 0x7E0
    and         r11, r6, r8         @ r11 = c & (~0x7E0)
    orr         r12, r11, r5, lsl #16  @ r12 = (c & (~0x7E0) ) | ((c & 0x7E0) << 16)

    mul         r6, r3, r12             @dst32(r5) = scale5 * SkExpand_rgb_16(*device)

    add         r5, r6, r2             @ src32 + dst32
    lsr         r6, r5, #5              @ (src32 + dst32) >> 5
    and         r12, r7, r6, lsr #16    @ (c>>16) & 2016
    and         r11, r8, r6             @ (c & ~(2016))
    orr         r5, r11, r12            @ (c & ~(2016) | ( (c>>16) & 2016)

    strh        r5, [r0]                @ *device = r5
    add         r0, #2                  @ device++
    subs        r14, r14, #1
    bhi         onepx

end_blitH:
    ldmfd       sp!, {r4-r12, r14}
    mov         pc, lr

.fnend
