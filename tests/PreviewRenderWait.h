#pragma once
#include "Document.h"
#include <QtTest>

// The cache can already match the reference while a draft is queued.
// Wait for the renderer to finish before comparing pixels.
inline bool waitForCommittedPreview(Document &_document)
{
    const QImage expected = _document.composite();
    return QTest::qWaitFor(
        [&] {
            return !_document.m_previewRenderActive.load() &&
                   _document.displayComposite() == expected;
        },
        5000);
}
