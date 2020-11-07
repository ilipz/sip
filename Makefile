PJ_FLAG =  `pkg-config --cflags --libs libpjproject`
ARM_CC = arm-marvell-linux-gnueabi-gcc -c 
CC = gcc  -c -g
IP = 127.0.0.1
CALLS_PER_SEC = 20
CALL_LENGTH = 30
TEL = 666
LOCAL_IP = 127.0.0.1
CALLS = 1
PER_TIME = 1500

all: task1 task2 task3 task4 task1-arm task2-arm task3-arm task4-arm


# For current platform: x86_64

task1: 
	$(CC) task1.c 
	gcc task1.o -o task1 $(PJ_FLAG)

task2:
	$(CC) task2.c 
	gcc task2.o -o task2 $(PJ_FLAG)

task3: 
	$(CC) task3.c 
	gcc task3.o -o task3 $(PJ_FLAG)

task4:
	$(CC) task4.c 
	gcc task4.o -o task4 $(PJ_FLAG)

task1-arm:
	$(ARM_CC) task1.c 
	gcc task1-arm.o -o task1-arm $(PJ_FLAG)

task2-arm:
	$(ARM_CC) task2.c 
	gcc task2-arm.o -o task2-arm $(PJ_FLAG)

task3-arm:
	$(ARM_CC) task3.c 
	gcc task3-arm.o -o task3-arm $(PJ_FLAG)

task4-arm:
	$(ARM_CC) task4.c 
	gcc task4-arm.o -o task4-arm $(PJ_FLAG)

task3-test:
	sipp $(IP) -s $(TEL)  -d $(CALL_LENGTH) -l 20 -aa -mi $(LOCAL_IP) -rtp_echo -nd -r $(CALLS) -rp $(PER_TIME) 
.PHONY: clean
# make task3-test CALLS=1 PER_TIME=1500 IP=127.0.0.1 LOCAL_IP=127.0.0.1 CALL_LENGTH=30s
clean:
	rm *.o 
	rm task1 task1-arm task2 task2-arm task3 task3-arm task4 task4-arm

