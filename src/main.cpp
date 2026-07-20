#include <QApplication>
#include <QIcon>
#include "MainWindow.h"

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <shobjidl.h>
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LlamaChat");
    app.setApplicationVersion("0.2");
    app.setOrganizationName("Local");
    app.setWindowIcon(QIcon(":/icon.ico"));

#ifdef Q_OS_WIN
    // An explicit AppUserModelID lets Windows treat this as a distinct, pinnable
    // app (without it, Qt apps can get an unpinnable/duplicate taskbar entry).
    SetCurrentProcessExplicitAppUserModelID(L"MartenBlumen.LlamaChat");
#endif

    MainWindow w;
    w.show();
    return app.exec();
}
