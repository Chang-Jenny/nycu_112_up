
obj-m     += mazemod.o
ccflags-y += -DEXPORT_SYMTAB
CFLAGS += -Wno-stack-usage

all: maze
	make -C ../dist/modulebuild M=$(PWD) modules

maze: maze.c maze.h
	$(CROSS_COMPILE)gcc -o $@ $< -Wall -static

clean:
	rm -f maze
	make -C ../dist/modulebuild M=$(PWD) clean

install: all
	mkdir -p ../rootfs/modules
	cp maze mazemod.ko ../rootfs/modules
