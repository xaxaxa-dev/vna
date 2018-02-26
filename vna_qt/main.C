#include "mainwindow.H"
#include <QApplication>
#include <QtPlugin>

int main(int argc, char *argv[])
{
    qRegisterMetaType<string>("string");
    QApplication a(argc, argv);
    a.setStyle("gtk");
    MainWindow* w = new MainWindow();
    w->show();

    return a.exec();
}
