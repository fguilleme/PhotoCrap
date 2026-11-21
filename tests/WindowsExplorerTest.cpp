#include "Document.h"
#include <QDir>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>
#include <windows.h>

#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>

class WindowsExplorerTest : public QObject
{
    Q_OBJECT
private slots:
    void installedProjectHandler()
    {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        QVERIFY(SUCCEEDED(initialized));
        const auto    uninitialize = qScopeGuard([] { CoUninitialize(); });
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString name =
            QStringLiteral("Projet \u00e9clair\u00e9 %1").arg(QCoreApplication::applicationPid());
        const QString path = directory.filePath(name + ".pcr");
        Document      document;
        QVERIFY(document.newDocument(120, 60, false, 8));
        document.setColor(QColor(192, 80, 32));
        document.fillWithColor(true);
        QVERIFY(document.saveProject(QUrl::fromLocalFile(path)));

        const auto file = QDir::toNativeSeparators(path).toStdWString();
        Microsoft::WRL::ComPtr<IShellItemImageFactory> factory;
        QVERIFY(SUCCEEDED(SHCreateItemFromParsingName(file.c_str(), nullptr,
                                                      IID_PPV_ARGS(factory.GetAddressOf()))));
        HBITMAP bitmap = nullptr;
        QVERIFY(SUCCEEDED(factory->GetImage({96, 96}, SIIGBF_THUMBNAILONLY, &bitmap)));
        const QImage image = QImage::fromHBITMAP(bitmap);
        DeleteObject(bitmap);
        QVERIFY(!image.isNull());
        const QColor sample = image.pixelColor(image.width() / 2, image.height() / 2);
        QVERIFY(qAbs(sample.red() - 192) < 8 && qAbs(sample.green() - 80) < 8 &&
                qAbs(sample.blue() - 32) < 8);

        SHELLEXECUTEINFOW open{};
        open.cbSize = sizeof(open);
        open.fMask  = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        open.lpFile = file.c_str();
        open.nShow  = SW_SHOWNORMAL;
        QVERIFY(ShellExecuteExW(&open));
        QVERIFY(open.hProcess);
        const auto closeHandle = qScopeGuard([&] { CloseHandle(open.hProcess); });
        const auto title       = (name + QStringLiteral(" \u2014 PhotoCrap")).toStdWString();
        HWND       window      = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((window = FindWindowW(nullptr, title.c_str())) != nullptr, 30000);
        DWORD pid = 0;
        GetWindowThreadProcessId(window, &pid);
        QCOMPARE(pid, GetProcessId(open.hProcess));
        PostMessageW(window, WM_CLOSE, 0, 0);
        QCOMPARE(WaitForSingleObject(open.hProcess, 10000), DWORD(WAIT_OBJECT_0));
    }
};
QTEST_MAIN(WindowsExplorerTest)
#include "WindowsExplorerTest.moc"
