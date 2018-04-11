#!/bin/bash

# please edit env.cfg and set the correct paths before running

cd "$(dirname $0)"
. env.cfg
if [ ! -e "$QT" ]; then
    echo "please edit env.cfg and set \$QT"
    exit 1
fi
if [ ! -e "$LINUXDEPLOYQT" ]; then
    echo "please edit env.cfg and set \$LINUXDEPLOYQT"
    exit 1
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
cp -a vna_qt/vna_qt appimage/
"$LINUXDEPLOYQT" appimage/vna_qt -qmake="$QMAKE" -appimage

