#include "Document.h"

#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <cmath>

namespace
{
QVariantMap sliceDescription(const QRect &_rect, int _index, bool _automatic)
{
    return {{"x", _rect.x()},           {"y", _rect.y()},  {"width", _rect.width()},
            {"height", _rect.height()}, {"index", _index}, {"automatic", _automatic}};
}
QRect pixelRect(const QRectF &_rect, const QSize &_size)
{
    const QRectF rect = _rect.normalized();
    const int    x = qRound(rect.left()), y = qRound(rect.top());
    return QRect(x, y, qRound(rect.right()) - x, qRound(rect.bottom()) - y)
        .intersected(QRect(QPoint{}, _size));
}
} // namespace

QVariantList Document::slices() const
{
    QVariantList list;
    QRegion      uncovered(QRect(QPoint{}, m_size));
    for (int i = 0; i < m_slices.size(); ++i) {
        list.append(sliceDescription(m_slices[i], i, false));
        if (m_sliceShowAuto)
            uncovered -= m_slices[i];
    }
    if (m_sliceShowAuto) {
        int i = int(m_slices.size());
        for (const QRect &rect : uncovered) {
            if (i >= 1024)
                break;
            list.append(sliceDescription(rect, i++, true));
        }
    }
    return list;
}
void Document::setSliceShowAuto(bool _value)
{
    m_sliceShowAuto = _value;
    emit slicesChanged();
}
void Document::commitSlices(const Snapshot &_before, const QString &_label)
{
    Snapshot before = _before;
    before.label    = _label;
    m_undo.append(before);
    m_redo.clear();
    m_stateId = m_nextId++;
    trimHistory();
    // Slice metadata does not invalidate any image or layer cache.
    emit historyChanged();
    emit slicesChanged();
}
void Document::beginSliceGesture(qreal _x, qreal _y, qreal _tolerance, bool _select)
{
    if (m_sliceBusy || m_cropBusy || !std::isfinite(_x) || !std::isfinite(_y))
        return;
    endSliceGesture();
    const QPointF point(qBound(0., _x, qreal(width())), qBound(0., _y, qreal(height())));
    m_geometryPointer = point;
    m_sliceAnchor     = point;
    m_sliceHandle     = -1;
    m_sliceCreating   = m_tool == "slice" && !_select;
    if (m_sliceCreating) {
        if (m_slices.size() >= 256) {
            fail(tr("Le document contient déjà 256 tranches."));
            return;
        }
        m_sliceDraft = QRectF(point, QSizeF(0, 0));
    } else {
        const qreal tolerance = std::isfinite(_tolerance) ? qMax(0., _tolerance) : 8;
        bool        grabbed   = false;
        if (m_activeSlice >= 0 && m_activeSlice < m_slices.size()) {
            const QRectF  rect(m_slices[m_activeSlice]);
            const QPointF handles[] = {rect.topLeft(),
                                       rect.topRight(),
                                       rect.bottomRight(),
                                       rect.bottomLeft(),
                                       QPointF(rect.center().x(), rect.top()),
                                       QPointF(rect.right(), rect.center().y()),
                                       QPointF(rect.center().x(), rect.bottom()),
                                       QPointF(rect.left(), rect.center().y())};
            for (int i = 0; i < 8; ++i)
                if (QLineF(point, handles[i]).length() <= tolerance) {
                    m_sliceHandle = i;
                    grabbed       = true;
                    break;
                }
        }
        if (!grabbed) {
            m_activeSlice = -1;
            for (int i = int(m_slices.size()) - 1; i >= 0; --i)
                if (QRectF(m_slices[i]).contains(point)) {
                    m_activeSlice = i;
                    break;
                }
        }
        if (m_activeSlice < 0) {
            emit slicesChanged();
            return;
        }
        m_sliceBefore = m_sliceDraft = QRectF(m_slices[m_activeSlice]);
    }
    m_sliceDragging = true;
    emit slicesChanged();
}
void Document::updateSliceGesture(qreal _x, qreal _y)
{
    if (!m_sliceDragging || !std::isfinite(_x) || !std::isfinite(_y))
        return;
    const QPointF point(qBound(0., _x, qreal(width())), qBound(0., _y, qreal(height())));
    if (m_sliceCreating) {
        if (m_toolSpace)
            m_sliceAnchor += point - m_geometryPointer;
        m_sliceDraft = toolRectangle(m_sliceAnchor, point, m_toolAlternate);
    } else if (m_sliceHandle == -1) {
        QPointF delta = point - m_sliceAnchor;
        delta.setX(qBound(-m_sliceBefore.left(), delta.x(), width() - m_sliceBefore.right()));
        delta.setY(qBound(-m_sliceBefore.top(), delta.y(), height() - m_sliceBefore.bottom()));
        m_sliceDraft = m_sliceBefore.translated(delta);
    } else {
        QRectF rect = m_sliceBefore;
        if (m_sliceHandle == 0 || m_sliceHandle == 3 || m_sliceHandle == 7)
            rect.setLeft(qMin(point.x(), rect.right() - 1));
        if (m_sliceHandle == 1 || m_sliceHandle == 2 || m_sliceHandle == 5)
            rect.setRight(qMax(point.x(), rect.left() + 1));
        if (m_sliceHandle == 0 || m_sliceHandle == 1 || m_sliceHandle == 4)
            rect.setTop(qMin(point.y(), rect.bottom() - 1));
        if (m_sliceHandle == 2 || m_sliceHandle == 3 || m_sliceHandle == 6)
            rect.setBottom(qMax(point.y(), rect.top() + 1));
        m_sliceDraft = rect;
    }
    m_geometryPointer = point;
    emit slicesChanged();
}
void Document::endSliceGesture(bool _cancel)
{
    if (!m_sliceDragging)
        return;
    m_sliceDragging  = false;
    const QRect rect = pixelRect(m_sliceDraft, m_size);
    if (!_cancel && !rect.isEmpty()) {
        const Snapshot before = snapshot(tr("Modifier les tranches"));
        if (m_sliceCreating) {
            m_slices.append(rect);
            m_activeSlice = int(m_slices.size()) - 1;
            commitSlices(before, tr("Créer une tranche"));
        } else if (m_activeSlice >= 0 && m_activeSlice < m_slices.size() &&
                   m_slices[m_activeSlice] != rect) {
            m_slices[m_activeSlice] = rect;
            commitSlices(before, tr("Modifier une tranche"));
        }
    }
    m_sliceDraft = {};
    emit slicesChanged();
}
void Document::deleteActiveSlice()
{
    if (m_sliceBusy || m_activeSlice < 0 || m_activeSlice >= m_slices.size())
        return;
    endSliceGesture(true);
    const Snapshot before = snapshot(tr("Supprimer une tranche"));
    m_slices.removeAt(m_activeSlice);
    m_activeSlice = qMin(m_activeSlice, int(m_slices.size()) - 1);
    commitSlices(before, tr("Supprimer une tranche"));
}
void Document::setActiveSliceRect(int _x, int _y, int _width, int _height)
{
    if (m_sliceBusy || m_activeSlice < 0 || m_activeSlice >= m_slices.size() || _width < 1 ||
        _height < 1 || _width > width() || _height > height())
        return;
    const QRect rect(qBound(0, _x, width() - _width), qBound(0, _y, height() - _height), _width,
                     _height);
    if (rect == m_slices[m_activeSlice])
        return;
    endSliceGesture(true);
    const Snapshot before   = snapshot(tr("Modifier une tranche"));
    m_slices[m_activeSlice] = rect;
    commitSlices(before, tr("Modifier une tranche"));
}
void Document::divideActiveSlice(int _columns, int _rows)
{
    if (m_sliceBusy || m_activeSlice < 0 || m_activeSlice >= m_slices.size() || _columns < 1 ||
        _rows < 1 || _columns > 32 || _rows > 32 || _columns * _rows + m_slices.size() - 1 > 256 ||
        (_columns == 1 && _rows == 1))
        return;
    const QRect rect = m_slices[m_activeSlice];
    if (_columns > rect.width() || _rows > rect.height())
        return;
    endSliceGesture(true);
    const Snapshot before = snapshot(tr("Diviser une tranche"));
    const int      index  = m_activeSlice;
    m_slices.removeAt(index);
    int offset = 0;
    for (int row = 0; row < _rows; ++row)
        for (int column = 0; column < _columns; ++column) {
            const int x      = rect.x() + column * rect.width() / _columns;
            const int y      = rect.y() + row * rect.height() / _rows;
            const int right  = rect.x() + (column + 1) * rect.width() / _columns;
            const int bottom = rect.y() + (row + 1) * rect.height() / _rows;
            m_slices.insert(index + offset++, QRect(x, y, right - x, bottom - y));
        }
    m_activeSlice = index;
    commitSlices(before, tr("Diviser une tranche"));
}
void Document::exportSlices(const QUrl &_folder)
{
    if (m_sliceBusy || m_slices.isEmpty() || !_folder.isLocalFile() ||
        !QDir(_folder.toLocalFile()).exists())
        return;
    endStroke();
    m_sliceBusy = true;
    emit  slicesChanged();
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher] {
        const QString error = watcher->result();
        watcher->deleteLater();
        m_sliceBusy = false;
        emit slicesChanged();
        if (error.isEmpty())
            emit statusMessage(tr("Tranches exportées en PNG"));
        else
            fail(error);
    });
    watcher->setFuture(QtConcurrent::run([layers = m_layers, size = m_size, slices = m_slices,
                                          folder = _folder.toLocalFile()] {
        const QImage image = renderPreviewSnapshot(layers, size, size, false);
        if (image.isNull())
            return tr("Impossible de calculer l’image des tranches.");
        QStringList written;
        for (int i = 0; i < slices.size(); ++i) {
            const QString base = QStringLiteral("tranche_%1").arg(i + 1, 2, 10, QLatin1Char('0'));
            QString       path = QDir(folder).filePath(base + ".png");
            for (int suffix = 1; QFile::exists(path) && suffix < 10000; ++suffix)
                path = QDir(folder).filePath(base + QStringLiteral("_%1.png").arg(suffix));
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) ||
                !image.copy(slices[i]).save(&file, "PNG")) {
                if (file.isOpen()) {
                    file.close();
                    QFile::remove(path);
                }
                for (const auto &created : written)
                    QFile::remove(created);
                return tr("Impossible d’exporter les tranches dans ce dossier.");
            }
            written.append(path);
        }
        return QString{};
    }));
}
