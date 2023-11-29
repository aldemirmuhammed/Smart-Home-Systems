CC=gcc
LINK=gcc
CFLAGS=-c -Wall -I.
OBJECT_FILES= homeServer.o gpioDIO.o 

.SUFFIXES: .c .o

.c .o:
	$(CC) $^ -o $@ $<

# -lm for Math.h library.
appHomeServer: $(OBJECT_FILES)
	$(LINK) $(OBJECT_FILES) -o $@ -lpthread -lm 


gpioDIO.o: gpioDIO.c gpioDIO.h gpioTypes.h
homeServer.o: homeServer.c gpioDIO.h  Configuration.h
