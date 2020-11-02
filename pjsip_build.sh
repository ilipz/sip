#!/bin/bash
./configure --disable-libwebrtc --disable-speex-codec --disable-speex-aec CFLAGS=-DNDEBUG
make dep -j2
make -j2
