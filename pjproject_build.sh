#!/bin/bash -x
BINDIR="/smg2016_toolchain/armv7-marvell-linux-gnueabi-softfp_i686/bin "
LIBEXECDIR="/smg2016_toolchain/armv7-marvell-linux-gnueabi-softfp_i686/libexec "
LIBDIR="/smg2016_toolchain/armv7-marvell-linux-gnueabi-softfp_i686/lib "
INCLUDEDIR="/smg2016_toolchain/armv7-marvell-linux-gnueabi-softfp_i686/include "
ARM_ARCH="--build=x86_64 --host=armv7 --target=armv7 CROSS_COMPILE=arm-marvell-linux-gnueabi-gcc CC=arm-marvell-linux-gnueabi-gcc CXX=arm-marvell-linux-gnueabi-g++"
EXEC_PARAMETR=""
TOOLCHAIN_PATH=$2
if [ $1 = "arm" ]; then
    EXEC_PARAMETR="$ARM_ARCH"
    echo "Starting for ARM"
    
fi
if [ $1 = "arm-smg" ]; then
    EXEC_PARAMETR="$ARM_ARCH --bindir=$TOOLCHAIN_PATH$BINDIR --libexecdir=$TOOLCHAIN_PATH$LIBEXECDIR --libdir=$TOOLCHAIN_PATH$LIBDIR --includedir=$TOOLCHAIN_PATH$INCLUDEDIR"
    echo "Starting for ARM"
    echo "BINDIR: $TOOLCHAIN_PATH$BINDIR"
    echo "LIBEXECDIR: $TOOLCHAIN_PATH$LIBEXECDIR"
    echo "LIBDIR: $TOOLCHAIN_PATH$LIBDIR"
    echo "INCLUDEDIR: $TOOLCHAIN_PATH$INCLUDEDIR"
fi
./configure --disable-libwebrtc --disable-speex-codec --disable-libyuv --disable-speex-aec CFLAGS=-DNDEBUG  $EXEC_PARAMETR     	
make dep -j2 
make -j2
