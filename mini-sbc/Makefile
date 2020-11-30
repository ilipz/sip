PJ_FLAG = `pkg-config --cflags --libs libpjproject` 
OBJ = gcc -g -c $(PJ_FLAG)

all: cb loggers juncs threads utils main
##############################
cb: forked media_upd rx_req state_chd

juncs: free

loggers: rx_msg siprtp tx_msg

threads: console junc_controller

utils: exits inits util
###############################
forked:
	$(OBJ) ../cb/forked.c  

media_upd:
	$(OBJ) ../cb/media_upd.c

rx_req:
	$(OBJ) ../cb/rx_req.c 

state_chd:
	$(OBJ) ../cb/state_chd.c 


free:
	$(OBJ) ../juncs/free.c


rx_msg:
	$(OBJ) ../loggers/rx_msg.c 

tx_msg:
	$(OBJ) ../loggers/tx_msg.c 

console:
	$(OBJ) ../threads/console.c 

junc_controller:
	$(OBJ) ../threads/junc_controller.c 

exits:
	$(OBJ) ../utils/exits.c 

inits:
	$(OBJ) ../utils/inits.c 

util:
	$(OBJ) ../utils/util.c

main:
	$(OBJ) ../main.c


