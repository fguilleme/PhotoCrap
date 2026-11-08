#pragma once
#include <QEvent>
#include <QFileOpenEvent>
#include <QGuiApplication>
#include <QList>
#include <QPointer>
#include <QUrl>
#include <QWindow>
#include <functional>
#include <utility>

class Application : public QGuiApplication
{
public:
    Application(int &_argc, char **_argv) : QGuiApplication(_argc, _argv)
    {
    }

    void setMainWindow(QWindow *_window)
    {
        m_mainWindow = _window;
    }
    void setFileOpenHandler(std::function<void(const QUrl &)> handler)
    {
        m_fileOpen = std::move(handler);
        if (!m_fileOpen)
            return;
        const auto pending = std::exchange(m_pendingFiles, {});
        for (const auto &url : pending)
            m_fileOpen(url);
    }

protected:
    bool event(QEvent *_event) override
    {
        if (_event->type() == QEvent::FileOpen) {
            auto *     file = static_cast<QFileOpenEvent *>(_event);
            const QUrl url =
                file->url().isEmpty() ? QUrl::fromLocalFile(file->file()) : file->url();
            if (url.isLocalFile()) {
                if (m_fileOpen)
                    m_fileOpen(url);
                else
                    m_pendingFiles.append(url);
                _event->accept();
                return true;
            }
        }
        // Native Quit closes all platform windows, including hidden tool windows.
        // Ask the document window first so a rejected close cannot partially tear
        // down the workspace before its unsaved-changes decision.
        if (_event->type() == QEvent::Quit && m_mainWindow && m_mainWindow->isVisible() &&
            !m_mainWindow->close()) {
            _event->ignore();
            return true;
        }
        return QGuiApplication::event(_event);
    }

private:
    QPointer<QWindow> m_mainWindow;
    std::function<void(const QUrl &)> m_fileOpen;
    QList<QUrl>                       m_pendingFiles;
};
