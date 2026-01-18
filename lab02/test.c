#include "maze.h"

static long hellomod_dev_ioctl(struct file *fp, unsigned int cmd, unsigned long arg) {
	printk(KERN_INFO "hellomod: ioctl cmd=%u arg=%lu.\n", cmd, arg);
	return 0;
}

// 1. maze.c maze.h -> maze.ko
// 2. insmod maze.ko
//   - /dev/maze
//   - /proc/maze
// 3. mazetest.c -> call `ioctl`
//   - maze.ko should handle the call from `iotcl`

/**
 * @brief handle 
 */
static long maze_op_dev_ioctl(struct file*fp, unsigned int cmd, unsigned long arg) {
    switch(cmd) {
        case MAZE_CREATE:
            // use `copy_from_user`
            coord_t coord = *(coord_t*) arg;
            
            break;
        case MAZE_DESTROY:
            // ...
            break;
        case MAZE_RESET:
            // ...
            break;
        case MAZE_GETSIZE:
            // ...
            break;
        case MAZE_MOVE:
            // ...
            break;
        case MAZE_GETPOS:
            // ...
            break;
        case MAZE_GETSTART:
            // ...
            break;
        case MAZE_GETEND:
            // ...
            break;
    
    }
}

// 1. write init function, check create /proc/maze and /dev/maze
// 2. write interface for
//   - open
//   - close
//   - read
//   - write
//   - ioctl
//      check each function and called successful, use printk for checking
// 3. start creating maze and implement maze operation
//   - start with ONE maze!!!
//   - then deal with two or three mazes
//     - use mutex
// 