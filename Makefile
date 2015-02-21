CC = gcc
CFLAGS = -Wall -Werror -I./src/inc
OBJ = readwrite.o myfsck.o fsck_util.o

SOURCE = src
VPATH = $(SOURCE)


all: myfsck

part_1:
	./myfsck -p 10 -i disk

myfsck: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o myfsck
