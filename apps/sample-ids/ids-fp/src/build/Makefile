ifndef NO_OPTIMIZE
	O_SYM := -O3
else 
	O_SYM := -g
endif

LIBS := -lm -lpthread -lpcap

all: main

clean: 
	rm *.o main

# EXECUTABLES
main: Sniffer.o
	gcc -Wall $(O_SYM) -o main Sniffer.o $(LIBS) && rm *.o

# OBJECTS
Sniffer.o: ../Sniffer/Sniffer.c ../Sniffer/Sniffer.h
	gcc -Wall $(O_SYM) -c ../Sniffer/Sniffer.c -I../
