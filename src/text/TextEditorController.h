#pragma once
#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>
#include <QVariantMap>
class TextEditorController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
public:
    explicit TextEditorController(QObject *_parent = nullptr) : QObject(_parent)
    {
    }
    QQuickTextDocument *document() const
    {
        return m_document;
    }
    void setDocument(QQuickTextDocument *_document)
    {
        if (m_document == _document)
            return;
        m_document = _document;
        emit documentChanged();
    }
    Q_INVOKABLE QVariantMap  format(int _position) const;
    Q_INVOKABLE void         apply(const QVariantMap &_values, int _start, int _end);
    Q_INVOKABLE QVariantList selectionRects(int _start, int _end) const;
    Q_INVOKABLE QStringList  fontFamilies() const;
    Q_INVOKABLE QVariantMap  snapshot() const;
    Q_INVOKABLE void         restoreFormatting(const QVariantMap &_parameters);
    Q_INVOKABLE QStringList  fontStyles(const QString &_family) const;
    Q_INVOKABLE bool         fontAvailable(const QString &_family) const;
signals:
    void documentChanged();

private:
    QPointer<QQuickTextDocument> m_document;
};
