#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/icon/web/icon.png"));
    MainWindow w;
    // Check if started minimized
    QStringList args = QCoreApplication::arguments();
    if (args.size() > 1 && args[1] == "--minimized") {
        w.hide(); // Start hidden, tray icon will be visible
    } else {
        w.show();
    }
    return a.exec();
}
