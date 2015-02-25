CC = gcc
CFLAGS = -Wall -Werror -I./src/inc
OBJ = readwrite.o myfsck.o fsck_util.o pass2.o pass3.o pass4.o

SOURCE = src
VPATH = $(SOURCE)


all: myfsck

myfsck: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o myfsck