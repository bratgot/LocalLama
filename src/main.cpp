#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Grammar Refine");
    app.setOrganizationName("Local");

    MainWindow w;
    w.show();
    return app.exec();
}
