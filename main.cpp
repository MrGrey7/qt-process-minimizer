#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/icon/web/icon.png"));
    MainWindow w;
    // Check if started minimized
    if (argc > 1 && QString(argv[1]) == "--minimized") {
        w.hide(); // Start hidden, tray icon will be visible
    } else {
        w.show();
    }
    return a.exec();
}
