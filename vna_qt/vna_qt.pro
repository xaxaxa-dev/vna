#-------------------------------------------------
#
# Project created by QtCreator 2017-12-16T02:35:03
#
#-------------------------------------------------

QT       += core gui charts

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets


CONFIG += static
#CONFIG -= import_plugins


#QTPLUGIN = xcb
#QTPLUGIN.imageformats = -

#QMAKE_LFLAGS += --static -lexpat -lz -lXext -lXau -lbsd -lXdmcp
#QMAKE_LFLAGS += -L../lib -lxavna
#QMAKE_CXXFLAGS += --std=c++11

TARGET = vna_qt
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES +=\
    polarview.C \
    mainwindow.C \
    main.C \
    markerslider.C \
    impedancedisplay.C \
    frequencydialog.C \
    graphpanel.C \
    configureviewdialog.C

HEADERS  += \
    polarview.H \
    mainwindow.H \
    markerslider.H \
    impedancedisplay.H \
    utility.H \
    frequencydialog.H \
    graphpanel.H \
    configureviewdialog.H

FORMS    += mainwindow.ui \
    markerslider.ui \
    impedancedisplay.ui \
    frequencydialog.ui \
    graphpanel.ui \
    configureviewdialog.ui

RESOURCES += \
    resources.qrc

unix|win32: LIBS += -L$$PWD/../lib/ -lxavna

INCLUDEPATH += $$PWD/../include
DEPENDPATH += $$PWD/../include
