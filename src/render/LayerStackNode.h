#pragma once
#include "LayerStack.h"
#include <QSGNode>
#include <functional>
#include <memory>
class QQuickWindow;

// Owns GPU-only render targets. Sources are shallow immutable tile snapshots;
// all QRhi resources and pixel staging stay on the scene graph thread.
class LayerStackNode final : public QSGNode
{
public:
    LayerStackNode(QQuickWindow *window, std::function<void()> failure,
                   std::shared_ptr<PhotoCrap::Render::StackStatistics> statistics);
    ~LayerStackNode() override;
    static bool supported(QQuickWindow *window, int bits);
    bool        update(const PhotoCrap::Render::LayerStack &stack, const QRectF &item,
                       const QRectF &visible, qint64 cacheBytes);
    quint64     uploads() const;
    quint64     passes() const;
    quint64     bytes() const;

private:
    struct Data;
    std::unique_ptr<Data> d;
};
