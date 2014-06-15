ifndef NO_OPTIMIZE
	O_SYM := -O3
else 
	O_SYM := -g
endif

ifdef VERBOSE
	V_SYM := -DVERBOSE
endif

LIBS := -lm -lpthread -lpcap

all: main

clean: 
	rm *.o main

# EXECUTABLES
main: Sniffer.o PacketBuffer.o
	gcc -Wall $(O_SYM) -o main Sniffer.o PacketBuffer.o $(LIBS) && rm *.o

# OBJECTS
PacketBuffer.o: ../Common/PacketBuffer.c ../Common/PacketBuffer.h
	gcc -Wall $(O_SYM) $(V_SYM) -c ../Common/PacketBuffer.c -I../

Sniffer.o: ../Sniffer/Sniffer.c ../Sniffer/Sniffer.h
	gcc -Wall $(O_SYM) $(V_SYM) -c ../Sniffer/Sniffer.c -I../
