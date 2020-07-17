CC=gcc
LINK=gcc
CFLAGS=-c -Wall -I.
OBJECT_FILES=CommunicationAPI.o homeClientTest.o

.SUFFIXES: .c .o

.c .o:
	$(CC) $^ -o $@ $<

appHomeClientTest: $(OBJECT_FILES)
	$(LINK) $(OBJECT_FILES) -o $@

#Dependances
CommunicationAPI.o: CommunicationAPI.c CommunicationAPI.h MessageHolderStruct.h CommunicationInfoStruct.h Configuration.h
homeClientTest.o: homeClientTest.c CommunicationAPI.h
