/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkDocument.h"
#include "SkPDFDocument.h"
#include "SkPDFDeviceFlattener.h"

class SkDocument_PDF : public SkDocument {
public:
    SkDocument_PDF(SkWStream* stream, void (*doneProc)(SkWStream*))
            : SkDocument(stream, doneProc) {
        fDoc = SkNEW(SkPDFDocument);
        fCanvas = NULL;
        fDevice = NULL;
    }

    virtual ~SkDocument_PDF() {
        // subclasses must call close() in their destructors
        this->close();
    }

protected:
    virtual SkCanvas* onBeginPage(SkScalar width, SkScalar height,
                                  const SkRect& trimBox) SK_OVERRIDE {
        SkASSERT(NULL == fCanvas);
        SkASSERT(NULL == fDevice);

        SkSize mediaBoxSize;
        mediaBoxSize.set(width, height);

        fDevice = SkNEW_ARGS(SkPDFDeviceFlattener, (mediaBoxSize, &trimBox));
        fCanvas = SkNEW_ARGS(SkCanvas, (fDevice));
        return fCanvas;
    }

    virtual void onEndPage() SK_OVERRIDE {
        SkASSERT(fCanvas);
        SkASSERT(fDevice);

        fCanvas->flush();
        fDoc->appendPage(fDevice);

        fCanvas->unref();
        fDevice->unref();

        fCanvas = NULL;
        fDevice = NULL;
    }

    virtual void onClose(SkWStream* stream) SK_OVERRIDE {
        SkASSERT(NULL == fCanvas);
        SkASSERT(NULL == fDevice);

        fDoc->emitPDF(stream);
        SkDELETE(fDoc);
        fDoc = NULL;
    }

private:
    SkPDFDocument*  fDoc;
    SkPDFDeviceFlattener* fDevice;
    SkCanvas*       fCanvas;
};

///////////////////////////////////////////////////////////////////////////////

SkDocument* SkDocument::CreatePDF(SkWStream* stream, void (*done)(SkWStream*)) {
    return stream ? SkNEW_ARGS(SkDocument_PDF, (stream, done)) : NULL;
}

static void delete_wstream(SkWStream* stream) {
    SkDELETE(stream);
}

SkDocument* SkDocument::CreatePDF(const char path[]) {
    SkFILEWStream* stream = SkNEW_ARGS(SkFILEWStream, (path));
    if (!stream->isValid()) {
        SkDELETE(stream);
        return NULL;
    }
    return SkNEW_ARGS(SkDocument_PDF, (stream, delete_wstream));
}
