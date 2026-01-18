
obj-m     += maze.o
ccflags-y += -DEXPORT_SYMTAB
CFLAGS += -Wno-stack-usage

all: maze_q3
	make -C ../dist/modulebuild M=$(PWD) modules

maze_q3: maze_q3.c maze.h
	$(CROSS_COMPILE)gcc -o $@ $< -Wall -static

clean:
	rm -f maze_q3
	make -C ../dist/modulebuild M=$(PWD) clean

install: all
	mkdir -p ../rootfs/modules
	cp maze_q3 maze.ko ../rootfs/modules
