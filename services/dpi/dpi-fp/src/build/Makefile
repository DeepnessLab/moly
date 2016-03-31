ifndef NO_OPTIMIZE
	O_SYM := -g
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
main: ACBuilder.o NodeQueue.o BitArray.o HashMap.o PatternTable.o StateTable.o TableStateMachine.o TableStateMachineGenerator.o Sniffer.o json.o PacketBuffer.o
	gcc -Wall $(O_SYM) -o main ACBuilder.o NodeQueue.o BitArray.o HashMap.o PatternTable.o StateTable.o TableStateMachine.o TableStateMachineGenerator.o Sniffer.o json.o PacketBuffer.o $(LIBS) && rm *.o

# OBJECTS

ACBuilder.o: ../AhoCorasick/ACBuilder.c ../AhoCorasick/ACBuilder.h
	gcc -Wall $(O_SYM) $(V_SYM) -c ../AhoCorasick/ACBuilder.c -I../

NodeQueue.o: ../AhoCorasick/NodeQueue.c ../AhoCorasick/NodeQueue.h
	gcc -Wall $(O_SYM) $(V_SYM) -c ../AhoCorasick/NodeQueue.c -I../

BitArray.o: ../Common/BitArray/BitArray.c ../Common/BitArray/BitArray.h
	gcc -Wall $(O_SYM) $(V_SYM) -c ../Common/BitArray/BitArray.c -I../

HashMap.o: ../Common/HashMap/HashMap.c ../Common/HashMap/HashMap.h
	gcc -Wall $(O_SYM) $(V_SYM) -c ../Common/HashMap/HashMap.c -I../

json.o: ../Common/json.c ../Common/json.h
	gcc -Wall $(O_SYM) $(V_SYM) -c ../Common/json.c -I../

PatternTable.o: ../Common/PatternTable.c ../Common/PatternTable.h
	gcc -Wall $(O_SYM) $(V_SYM) -c ../Common/PatternTable.c -I../

StateTable.o: ../Common/StateTable.c ../Common/StateTable.h
	gcc -Wall $(O_SYM) $(V_SYM) -c ../Common/StateTable.c -I../

TableStateMachine.o: ../StateMachine/TableStateMachine.c ../StateMachine/TableStateMachine.h
	gcc -Wall $(O_SYM) $(V_SYM) -c ../StateMachine/TableStateMachine.c -I../

TableStateMachineGenerator.o: ../StateMachine/TableStateMachineGenerator.c ../StateMachine/TableStateMachineGenerator.h
	gcc -Wall $(O_SYM) $(V_SYM) -c ../StateMachine/TableStateMachineGenerator.c -I../

Sniffer.o: ../Sniffer/Sniffer.c ../Sniffer/Sniffer.h
	gcc -Wall $(O_SYM) $(V_SYM) -c ../Sniffer/Sniffer.c -I../

PacketBuffer.o: ../Common/PacketBuffer.c ../Common/PacketBuffer.h
	gcc -Wall $(O_SYM) $(V_SYM) -c ../Common/PacketBuffer.c -I../
