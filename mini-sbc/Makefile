PJ_FLAG = `pkg-config --cflags --libs libpjproject` 
OBJ = gcc -g -c 

all: cb loggers juncs threads utils main
	gcc bin/*.o -o ms $(PJ_FLAG)
##############################
cb: forked media_upd rx_req state_chd

juncs: free

loggers: rx_msg  tx_msg

threads: console junc_controller

utils: exits inits util
###############################
forked:
	$(OBJ) cb/forked.c  -o bin/forked.o

media_upd:
	$(OBJ) cb/media_upd.c -o bin/media_upd.o

rx_req:
	$(OBJ) cb/rx_req.c -o bin/rx_req.o

state_chd:
	$(OBJ) cb/state_chd.c -o bin/state_chd.o

free:
	$(OBJ) juncs/free.c -o bin/free.o

rx_msg:
	$(OBJ) loggers/rx_msg.c -o bin/rx_msg.o

tx_msg:
	$(OBJ) loggers/tx_msg.c -o bin/tx_msg.o

console:
	$(OBJ) threads/console.c -o bin/console.o

junc_controller:
	$(OBJ) threads/junc_controller.c -o bin/junc_controller.o

exits:
	$(OBJ) utils/exits.c -o bin/exits.o

inits:
	$(OBJ) utils/inits.c -o bin/inits.o

util:
	$(OBJ) utils/util.c -o bin/util.o

main:
	$(OBJ) main.c -o bin/main.o

.PHONY: clean

clean:
	rm bin/*.o 
	rm ms


