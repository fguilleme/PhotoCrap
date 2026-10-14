#pragma once
#include <QEvent>
#include <QGuiApplication>
#include <QPointer>
#include <QWindow>

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

protected:
    bool event(QEvent *_event) override
    {
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
};
