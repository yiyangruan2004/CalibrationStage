#include <QApplication>

#include "mainwindow.h"
#include "filer.h"

int main(int argc, char *argv[])
{
    Filer::installLogger();
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
