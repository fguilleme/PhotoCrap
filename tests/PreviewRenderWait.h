#pragma once
#include "Document.h"
#include <QtTest>

// finishLayerPreview starts a full-resolution worker while displayComposite
// intentionally retains the previous screen image. Reference comparisons must
// wait for that worker, not assume that two 60 ms frames finish its calculation.
inline bool waitForCommittedPreview(Document &_document)
{
    const QImage expected = _document.composite();
    return QTest::qWaitFor([&] { return _document.displayComposite() == expected; }, 5000);
}
