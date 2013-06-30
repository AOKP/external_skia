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
#ifndef SkFimgApi_DEFINED
#define SkFimgApi_DEFINED

#if defined(FIMG2D_ENABLED)

#include "SkColorPriv.h"
#include "SkBitmap.h"
#include "SkMallocPixelRef.h"
#include "SkFlattenable.h"
#include "SkUtils.h"
#include "SkXfermode.h"
#include "SkMatrix.h"

#include "FimgApi.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>

#include <linux/android_pmem.h>

//---------------------------------------------------------------------------//

#define FIMGAPI_G2D_BLOCKING            true
#define FIMGAPI_FILTERMODE_USE_SW       true
#define FIMGAPI_DITHERMODE_USE_SW       true
#define FIMGAPI_COMPROMISE_USE          true
#define FIMGAPI_HYBRID_USE              false

#if defined(SWP1_CUSTOMSIMD_ENABLE)
#define FIMGAPI_DMC_SIMD_OPT_USE        true
#else
#define FIMGAPI_DMC_SIMD_OPT_USE        false
#endif

#define FIMGAPI_FINISHED                (0x1<<0)
#define FIMGAPI_HYBRID                  (0x1<<1)

//       G2D Compromise values
#define COMP_VALUE_FILTER_SCALE 48*80

#define COPT_NANS       0
#define COPT_ANS        1
#define COPT_NAS        2
#define COPT_AS         3

struct Fimg {
    int            srcX;
    int            srcY;
    unsigned int   srcW;
    unsigned int   srcH;
    unsigned int   srcFWStride; // this is not w, just stride (w * bpp)
    unsigned int   srcFH;
    unsigned int   srcBPP;
    int            srcColorFormat;
    unsigned char *srcAddr;

    int            dstX;
    int            dstY;
    unsigned int   dstW;
    unsigned int   dstH;
    unsigned int   dstFWStride; // this is not w, just stride (w * bpp)
    unsigned int   dstFH;
    unsigned int   dstBPP;
    int            dstColorFormat;
    unsigned char *dstAddr;

    int            clipT;
    int            clipB;
    int            clipL;
    int            clipR;

    int            rotate;
    int            alpha;
    int            xfermode;
    int            isDither;
    int            isFilter;
    int            colorFilter;
    int            canusehybrid;
    int            matrixType;
    int            matrixSx;
    int            matrixSy;
    Fimg() {
    }
};

int FimgApiCheckPossibleHybrid(Fimg *fimg);
bool FimgApiCheckPossible(Fimg *fimg);
bool FimgApiCompromise(Fimg *fimg);
int FimgApiStretch(Fimg *fimg, const char *func_name);
int FimgApiSync(const char *func_name);
#endif //defined(FIMG2D_ENABLED)

#ifdef CHECK_TIME_FOR_FIMGAPI

extern "C" {
    #include <stdio.h>
    #include <sys/time.h>
    #include <time.h>
    #include <unistd.h>
}

class MyAutoTimeManager
{
private:
    const char     *mInfo;

    int             mSrcColorFormat;
    int             mSrcWidth;
    int             mSrcHeight;

    int             mDstColorFormat;
    int             mDstWidth;
    int             mDstHeight;
    int             mAlpha;

    char           *nameA;
    long long       timeA;
    char           *nameB;
    long long       timeB;

public:
    MyAutoTimeManager(const char *info,
                      int srcColorFormat,
                      int srcWidth,
                      int srcHeight,
                      int dstColorFormat,
                      int dstWidth,
                      int dstHeight,
                      int alpha)
               : mInfo(info),
                 mSrcColorFormat(srcColorFormat),
                 mSrcWidth (srcWidth),
                 mSrcHeight(srcHeight),
                 mDstColorFormat(dstColorFormat),
                 mDstWidth (dstWidth),
                 mDstHeight(dstHeight),
                 mAlpha(alpha)
    {
        nameA = NULL;
        timeA = 0;
        nameB = NULL;
        timeB = 0;
    }

    ~MyAutoTimeManager()
    {
        if (nameA && nameB
            && FimgApiCheckPossible(mSrcColorFormat, mSrcWidth, mSrcHeight,
                                    mDstColorFormat, mDstWidth, mDstHeight) == true) {
            long long gapTime;
            float     gapRate;
            long long fasterTime;
            long long slowerTime;
            char     *fasterName;
            char     *slowerName;

            if (timeA <= timeB) {
                fasterName = nameA;
                fasterTime = timeA;
                slowerName = nameB;
                slowerTime = timeB;
            } else {
                fasterName = nameB;
                fasterTime = timeB;
                slowerName = nameA;
                slowerTime = timeA;
            }

            gapTime = slowerTime - fasterTime;
            gapRate = ((float)slowerTime / (float)fasterTime) * 100.f - 100.0f;

            SkDebugf("%s::%s (%5lld) faster than %s (%5lld) (%5lld) microsec / (%6.1f ) %% : [%3d %3d (%2d)] -> [%3d %3d (%2d)] \[sizeConv : %c] [colorConv : %c] [alpha : %3d] \n",
                      mInfo,
                      fasterName,
                      fasterTime,
                      slowerName,
                      slowerTime,
                      gapTime,
                      gapRate,
                      mSrcWidth,
                      mSrcHeight,
                      mSrcColorFormat,
                      mDstWidth,
                      mDstHeight,
                      mDstColorFormat,
                      (mSrcWidth != mDstWidth || mSrcHeight != mDstHeight) ? 'O' : 'X',
                      (mSrcColorFormat != mDstColorFormat) ? 'O' : 'X',
                      mAlpha);
        }
    }

    void SetGap(char *name, long long gap)
    {
        if (nameA == NULL) {
            nameA = name;
            timeA = gap;
        } else {
            nameB = name;
            timeB = gap;
        }
    }
};

//---------------------------------------------------------------------------//

class MyAutoTime
{
private:
    struct timeval     mStartWhen;
    const char        *mMyName;
    MyAutoTimeManager *mAutoTimeManager;

public:
    MyAutoTime(const char *myName,
               MyAutoTimeManager *autoTimeManager)
               : mMyName(myName),
                 mAutoTimeManager(autoTimeManager)
    {
        gettimeofday(&mStartWhen, NULL);
    }

    ~MyAutoTime()
    {
        struct timeval endWhen;
        long long      timeGap;

        gettimeofday(&endWhen, NULL);

        long long start = ((long long) mStartWhen.tv_sec) * 1000000LL + ((long long) mStartWhen.tv_usec);
        long long stop = ((long long) endWhen.tv_sec) * 1000000LL + ((long long) endWhen.tv_usec);

        timeGap = stop - start;
        mAutoTimeManager->SetGap((char*)mMyName, timeGap);
    }
};

#endif

#endif //SkFimgApi_DEFINED
