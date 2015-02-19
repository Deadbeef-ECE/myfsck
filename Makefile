CC = gcc
CFLAGS = -Wall -Werror -I./inc
OBJ = readwrite.o myfsck.o

SOURCE = src
VPATH = $(SOURCE)


all: myfsck

myfsck: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o myfsck