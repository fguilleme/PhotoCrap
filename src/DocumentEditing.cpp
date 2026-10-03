#include "Document.h"
#include "ImageUtils.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QPainter>
#include <QTransform>

void Document::undo()
{
    endStroke();
    if (m_undo.isEmpty())
        return;
    auto state = m_undo.takeLast();
    m_redo.append(snapshot(state.label));
    restore(state);
}
void Document::redo()
{
    endStroke();
    if (m_redo.isEmpty())
        return;
    auto state = m_redo.takeLast();
    m_undo.append(snapshot(state.label));
    restore(state);
}
void Document::selectAll()
{
    endStroke();
    m_selection = QRect(QPoint{}, m_size);
    emit selectionChanged();
}
void Document::deselect()
{
    endStroke();
    m_selection = {};
    emit selectionChanged();
}
void Document::clearSelected()
{
    if (!editable())
        return;
    edit(tr("Effacer"), [&] {
        QImage *target = paintTarget();
        if (!target)
            return;
        QPainter p(target);
        if (m_editingMask)
            p.fillRect(hasSelection() ? m_selection : QRect(QPoint{}, m_size), Qt::black);
        else {
            p.setCompositionMode(QPainter::CompositionMode_Clear);
            p.fillRect(hasSelection() ? m_selection : QRect(QPoint{}, m_size), Qt::transparent);
        }
    });
}
void Document::fillWithColor(bool _foreground, bool _preserveTransparency)
{
    if (!editable())
        return;
    const QColor color = _foreground ? m_color : m_secondary;
    edit(_foreground ? tr("Remplir avec la couleur de premier plan")
                     : tr("Remplir avec la couleur d’arrière-plan"),
         [&] {
             QImage *target = paintTarget();
             if (!target)
                 return;
             QPainter painter(target);
             if (_preserveTransparency && !m_editingMask)
                 painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
             painter.fillRect(hasSelection() ? m_selection : QRect(QPoint{}, m_size),
                              m_editingMask ? QColor::fromRgb(color.red(), color.red(), color.red())
                                            : color);
         });
}
void Document::copy()
{
    endStroke();
    QGuiApplication::clipboard()->setImage(hasSelection() ? composite().copy(m_selection)
                                                          : composite());
    emit statusMessage(tr("Copie de l’image visible dans le presse-papiers"));
}
void Document::paste()
{
    auto source = QGuiApplication::clipboard()->image();
    if (source.isNull())
        return;
    if (!validSize(width(), height(), int(m_layers.size()) + 1))
        return;
    if (source.colorSpace().isValid())
        source.convertToColorSpace(QColorSpace::SRgb);
    edit(tr("Coller"), [&] {
        auto     image = ImageUtils::blank(m_size);
        QPainter p(&image);
        p.drawImage((width() - source.width()) / 2, (height() - source.height()) / 2, source);
        p.end();
        m_layers.insert(++m_active, {tr("Image collée"), image});
        selectOnly(m_active);
    });
}
void Document::cropToSelection()
{
    if (!hasSelection())
        return;
    edit(tr("Recadrer"), [&] {
        for (auto &layer : m_layers) {
            if (!layer.image.isNull())
                layer.image = layer.image.copy(m_selection);
            if (!layer.mask.isNull())
                layer.mask = layer.mask.copy(m_selection);
            if (!layer.source.isNull())
                layer.source = layer.source.copy(m_selection);
        }
        m_size      = m_selection.size();
        m_selection = {};
    });
    emit documentChanged();
    emit selectionChanged();
}
void Document::resizeImage(int _w, int _h)
{
    if (!validSize(_w, _h, int(m_layers.size())) || m_size == QSize(_w, _h))
        return;
    edit(tr("Redimensionner l’image"), [&] {
        for (auto &layer : m_layers) {
            if (!layer.image.isNull())
                layer.image =
                    layer.image.scaled(_w, _h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            if (!layer.mask.isNull())
                layer.mask =
                    layer.mask.scaled(_w, _h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            if (!layer.source.isNull())
                layer.source =
                    layer.source.scaled(_w, _h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        m_size      = {_w, _h};
        m_selection = {};
    });
    emit documentChanged();
    emit selectionChanged();
}
void Document::transformImage(const QString &_operation)
{
    if (_operation != "rotateLeft" && _operation != "rotateRight" && _operation != "flipH" &&
        _operation != "flipV")
        return;
    edit(tr("Transformer l’image"), [&] {
        for (auto &layer : m_layers) {
            auto transform = [&](QImage &_image) {
                if (_image.isNull())
                    return;
                if (_operation == "flipH" || _operation == "flipV") {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
                    _image = _image.flipped(_operation == "flipH" ? Qt::Horizontal : Qt::Vertical);
#else
                    _image = _image.mirrored(_operation == "flipH", _operation == "flipV");
#endif
                } else
                    _image = _image.transformed(
                        QTransform().rotate(_operation == "rotateRight" ? 90 : -90));
            };
            transform(layer.image);
            transform(layer.mask);
            transform(layer.source);
        }
        if (_operation.startsWith("rotate"))
            m_size.transpose();
        m_selection = {};
    });
    emit documentChanged();
    emit selectionChanged();
}
