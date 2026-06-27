#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LlamaChat");
    app.setApplicationVersion("0.1");
    app.setOrganizationName("Local");

    MainWindow w;
    w.show();
    return app.exec();
}
