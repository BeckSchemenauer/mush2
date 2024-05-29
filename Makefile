CC = gcc

CFLAGS = -Wall -Werror -g

LD = gcc

LDFLAGS = -Wall -Werror -g

.SILENT:

all: mush2 clean

mush2: mush2.o
	$(LD) $(LDFLAGS) -L ~pn-cs357/Given/Mush/lib64 -o mush2 mush2.o -lmush
mush2.o: mush2.c
	$(CC) $(CFLAGS) -c -I ~pn-cs357/Given/Mush/include -o mush2.o mush2.c

clean:
	rm mush2.o
