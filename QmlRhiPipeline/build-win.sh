#!/bin/bash

# export QTDIR=C:/projects/Qt/6.10.1/mingw_64

rm -rf dist
rm -rf build
mkdir dist
mkdir build 
cd build

cmake -DCMAKE_PREFIX_PATH="$QTDIR/lib/cmake" ..
ninja
cd ../dist
cp ../build/qmlrhipipeline.exe .
cp /mingw64/bin/libassimp-5.dll .
cp /mingw64/bin/zlib1.dll .
cp /mingw64/bin/libminizip-1.dll .
cp /mingw64/bin/libbz2-1.dll .

$QTDIR/bin/windeployqt --qmldir ../qml qmlrhipipeline.exe