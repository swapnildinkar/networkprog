
CC = gcc

#LIBS =  /home/courses/cse533/Stevens/unpv13e/libunp.a
LIBS =  ./Stevens/unpv13e/libunp.a

FLAGS = -g 

#CFLAGS = -I/home/courses/cse533/Stevens/unpv13e/lib
CFLAGS = -I./Stevens/unpv13e/lib

all: tour_and arp_and

tour_and: tour_and.o utils.o
	${CC} ${FLAGS} -o tour_and tour_and.o utils.o ${LIBS}
tour_and.o: tour_and.c
	${CC} ${CFLAGS} -c tour_and.c

arp_and: arp_and.o utils.o 
	${CC} ${FLAGS} -o arp_and arp_and.o utils.o ${LIBS}
arp_and.o: arp_and.c
	${CC} ${CFLAGS} -c arp_and.c

tour_functions.o: tour_functions.c 
	${CC} ${CFLAGS} -c tour_functions.c

utils.o: utils.c
	${CC} ${CFLAGS} -c utils.c

clean: 
	rm tour_and.o tour_and arp_and.o arp_and utils.o

