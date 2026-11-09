#include "Document.h"

#include <QFileInfo>
#include <QImageReader>
#include <limits>

bool Document::canImportLayerFiles(const QList<QUrl> &_urls) const
{
    if (_urls.isEmpty() || _urls.size() > 512)
        return false;
    const auto formats = QImageReader::supportedImageFormats();
    for (const auto &url : _urls) {
        if (!url.isLocalFile())
            return false;
        const QFileInfo file(url.toLocalFile());
        if (!file.isFile() || !file.isReadable() ||
            (!isRawFile(url) && !formats.contains(file.suffix().toLower().toLatin1())))
            return false;
    }
    return true;
}

void Document::requestImportLayers(const QList<QUrl> &_urls, int _insertion)
{
    if (!canImportLayerFiles(_urls)) {
        fail(tr("Choisissez des fichiers image locaux pris en charge."));
        return;
    }
    if (rawLoading() || pixelEditBusy()) {
        fail(tr("Terminez ou annulez l’opération en cours."));
        return;
    }
    // Committing an empty provisional text layer may remove it from the stack.
    finishTextEdit();
    endStroke();
    finishLayerPreview();
    if (_insertion < -1 || _insertion > m_layers.size()) {
        fail(tr("La position d’insertion du calque n’est plus disponible."));
        return;
    }
    if (!validSize(width(), height(), int(m_layers.size() + _urls.size())) ||
        !validLayerStorage(width(), height(), int(_urls.size())))
        return;

    const int insertion = _insertion < 0 ? m_active + 1 : _insertion;
    QString   group;
    if (insertion > 0 && insertion < m_layers.size() &&
        m_layers[insertion - 1].groupId == m_layers[insertion].groupId)
        group = m_layers[insertion].groupId;

    // A file list is one transaction: decode on a private renderer, publish only
    // after every file succeeds, and retain one undo entry on the live document.
    // Always dispatch, even when the synchronous single-image test override is set.
    deferEdit(tr("Importer des images comme calques"), std::numeric_limits<qint64>::max(),
              [urls = _urls, insertion, group](Document &renderer) {
                  for (const auto &url : urls) {
                      renderer.m_active = insertion - 1;
                      if (!renderer.importLayer(url))
                          return;
                      renderer.m_layers[renderer.m_active].groupId = group;
                      renderer.m_undo.clear();
                      renderer.m_redo.clear();
                  }
                  // Keep the file-picker/drop order from top to bottom.
                  renderer.m_active      = insertion + int(urls.size()) - 1;
                  renderer.m_editingMask = false;
                  renderer.selectOnly(renderer.m_active);
              });
}
