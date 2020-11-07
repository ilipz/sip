#!/bin/bash
set CC=''
./configure --disable-libwebrtc --disable-speex-codec --disable-speex-aec CFLAGS=-DNDEBUG --build=x86_64 --host=arm CROSS_COMPILE=$CC CC=$CC
make dep -j2
make -j2
