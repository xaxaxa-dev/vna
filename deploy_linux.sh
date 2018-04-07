#!/bin/bash
if [ "$QT" == "" ]; then
    QT=/persist/qt/5.10.1/gcc_64
fi
QMAKE="$QT/bin/qmake"

autoreconf --install
./configure
make -j8

pushd libxavna/xavna_mock_ui
$QMAKE
make -j8
popd

pushd vna_qt
$QMAKE
make -j8
popd

export LD_LIBRARY_PATH="$(pwd)/libxavna/.libs:$(pwd)/libxavna/xavna_mock_ui:$QT/lib"
/persist/linuxdeployqt-continuous-x86_64.AppImage vna_qt/vna_qt -qmake="$QMAKE" -appimage

