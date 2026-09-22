#include "main.h"

int main(int argc, char *argv[])
{
    Filer::init();
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
