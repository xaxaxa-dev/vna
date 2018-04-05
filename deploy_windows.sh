MXE=/persist/mxe
export PATH="$MXE/usr/bin:$PATH"
HOST="i686-w64-mingw32.shared"
QMAKE="$HOST-qmake-qt5"

./configure --host "$HOST"
make -j8

pushd libxavna/xavna_mock_ui
"$QMAKE"
make -j8
popd

pushd vna_qt
"$QMAKE"
make -j8
for x in Qt5Charts Qt5Core Qt5Gui Qt5Widgets Qt5Svg; do
    cp "$MXE/usr/$HOST/qt5/bin/$x.dll" release/
done
mkdir -p release/platforms
mkdir -p release/styles
cp "$MXE/usr/$HOST/qt5/plugins/platforms/qwindows.dll" release/platforms/
cp "$MXE/usr/$HOST/qt5/plugins/styles/qwindowsvistastyle.dll" release/styles/

for x in libgcc_s_sjlj-1 libstdc++-6 libpcre2-16-0 zlib1 libharfbuzz-0 \
            libpng16-16 libfreetype-6 libglib-2.0-0 libbz2 libintl-8 libpcre-1\
            libiconv-2 libwinpthread-1 libjasper libjpeg-9 libmng-2 libtiff-5\
            libwebp-5 libwebpdemux-1 liblcms2-2 liblzma-5; do
    cp "$MXE/usr/$HOST/bin/$x.dll" release/
done
cp ../libxavna/.libs/libxavna-0.dll release/
cp ../libxavna/xavna_mock_ui/release/xavna_mock_ui.dll release/
