CC = gcc
CFLAGS = -Wall -Wextra -g

sdb: sdb.c sdb.h
	$(CC) $(CFLAGS) -o sdb sdb.c -lc -lcapstone

clean:
	rm -f sdb