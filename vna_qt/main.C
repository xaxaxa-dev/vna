#include "mainwindow.H"
#include "calkitsettings.H"
#include <QApplication>
#include <QtPlugin>

int main(int argc, char *argv[])
{
    qRegisterMetaType<string>("string");
    qRegisterMetaType<CalKitSettings>("CalKitSettings");
    qRegisterMetaTypeStreamOperators<CalKitSettings>("CalKitSettings");
    QApplication a(argc, argv);
    a.setStyle("fusion");
    MainWindow* w = new MainWindow();
    w->show();

    return a.exec();
}
