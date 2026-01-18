#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include "maze.h"
#define	DEVFILE	"/dev/maze"

static int mz_create(int fd, int x, int y) {
	coord_t c = { x, y };
	fprintf(stderr, "OP: ONLY_FOR_Q3 %d %d\n", x, y);
	return ioctl(fd, ONLY_FOR_Q3, &c);
}

int main() {
	int x, y, fd;
	srand((unsigned)time(NULL));
	
	if((fd = open(DEVFILE, O_RDWR)) < 0) {
		perror("open");
		return -1;
	}
    // printf("fd: %d\n", fd);
	x = 3 + rand() % 40;
	y = 3 + rand() % 20;
	if(x % 2 == 0) x++;
	if(y % 2 == 0) y++;
	if(mz_create(fd, x, y) < 0) return -1;
	// return fd;
	fprintf(stderr, "- Only for Question3 - Create a random Maze.\n");
	system("cat /proc/maze");
	close(fd);

	return 0;
}
