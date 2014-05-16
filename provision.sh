#!/bin/sh

WORKING_DIR=$(pwd)

echo "this script installs liquid-dsp, librtlsdr, and libwebsockets from source"
read -p "continue? [yn] " yn 

[ ! "$yn" = "y" ] && exit 1;

mkdir build && cd build

BUILD_DIR=$(pwd)

wget https://github.com/jgaeddert/liquid-dsp/archive/fpm.zip -O liquid-dsp-fpm.zip
unzip liquid-dsp-fpm.zip
cd liquid-dsp-fpm
./bootstrap.sh
./configure
make
sudo make install
sudo install -m 644 -p include/liquidfpm.h /usr/local/include

cd $BUILD_DIR

echo "building librtlsdr"
wget https://github.com/steve-m/librtlsdr/archive/master.zip -O librtlsdr.zip
unzip librtlsdr.zip
cd librtlsdr-master
mkdir build && cd build
cmake ..
make
sudo make install

cd $BUILD_DIR

echo "building libwebsockets"
git clone --depth=1 git://git.libwebsockets.org/libwebsockets
cd libwebsockets
mkdir build && cd build
cmake ..
make
sudo make install

echo "make sure /usr/local/lib is used by ld (check /etc/ld.so.conf.d/)"
echo "then run sudo ldconfig"

cd $WORKING_DIR
