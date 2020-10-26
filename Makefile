PJ_FLAG =  `pkg-config --cflags --libs libpjproject`
ARM_CC = arm-linux-gnueabihf-gcc-5 -c 
CC = gcc  -c -g
IP = 0
CALLS_PER_SEC = 1

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
	sipp $(IP):5060 -s 666 -i $(IP) -d 19s -l 20 -aa -mi $(IP) -rtp_echo -nd -r $(CALLS_PER_SEC)
.PHONY: clean
	
clean:
	rm *.o 
	rm task1 task1-arm task2 task2-arm task3 task3-arm task4 task4-arm

