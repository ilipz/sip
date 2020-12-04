PJ_FLAG =  `pkg-config --cflags --libs libpjproject`
ARM_CC = arm-marvell-linux-gnueabi-gcc 
CC = gcc  -c -g
IP = 127.0.0.1
CALLS_PER_SEC = 20
CALL_LENGTH = 30
TEL = 666
LOCAL_IP = 127.0.0.1
CALLS = 1
PER_TIME = 1500
PJPATH = '/home/user3/eltex/test-work/smg2016_toolchain/armv7-marvell-linux-gnueabi-softfp_i686/include'
all: task1 task2 task3 task4 task1-arm task2-arm task3-arm task4-arm
PATH1 = '/home/user3/eltex/test-work/smg2016_toolchain/armv7-marvell-linux-gnueabi-softfp_i686/arm-marvell-linux-gnueabi/libc/usr/include'
PATH2 = '/home/user3/eltex/test-work/smg2016_toolchain/armv7-marvell-linux-gnueabi-softfp_i686/lib' 
# For current platform: x86_64

task1: 
	$(CC) task1.c 
	gcc task1.o -o task1 $(PJ_FLAG)

task2:
	$(CC) task2.c 
	gcc task2.o  -o task2 $(PJ_FLAG)

task3: 
	$(CC) task3.c 
	gcc task3.o -o task3 $(PJ_FLAG)

task4:
	make -f mini-sbc/Makefile -j2

task1-arm:
	$(ARM_CC) task1.c 
	gcc task1-arm.o -o task1-arm $(PJ_FLAG)

task2-arm:
	$(ARM_CC) task2.c 
	gcc task2-arm.o -o task2-arm $(PJ_FLAG)

task3-arm:
	$(ARM_CC)  -nostdinc -Wall -std=c99 task3.c -o task3-arm -I$(PJPATH) $(PJ_FLAG)  -I'/home/user3/eltex/test-work/smg2016_toolchain/armv7-marvell-linux-gnueabi-softfp_i686/lib/gcc/arm-marvell-linux-gnueabi/4.6.2/include' -I$(PATH1) -I$(PATH2)

task4-arm:
	$(ARM_CC)  -nostdinc -Wall -std=c99 mini_sbc.c -o ms_arm -I$(PJPATH) $(PJ_FLAG)  -I'/home/user3/eltex/test-work/smg2016_toolchain/armv7-marvell-linux-gnueabi-softfp_i686/lib/gcc/arm-marvell-linux-gnueabi/4.6.2/include' -I$(PATH1) -I$(PATH2)


task3-test:
	sipp $(IP):7060 -s $(TEL)  -d $(CALL_LENGTH) -l 20 -aa -mi $(LOCAL_IP) -rtp_echo -nd -r $(CALLS) -rp $(PER_TIME) 
.PHONY: clean
# make task3-test CALLS=1 PER_TIME=1500 IP=127.0.0.1 LOCAL_IP=127.0.0.1 CALL_LENGTH=30s
clean:
	rm *.o 
	rm task1 task1-arm task2 task2-arm task3 task3-arm ms ms_arm

