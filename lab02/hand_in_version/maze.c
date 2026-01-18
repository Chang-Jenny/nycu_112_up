#include <linux/module.h>	// included for all kernel modules
#include <linux/kernel.h>	// included for KERN_INFO
#include <linux/init.h>		// included for __init and __exit macros
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/errno.h>
#include <linux/sched.h>	// task_struct requried for current_uid()
#include <linux/cred.h>		// for current_uid();
#include <linux/slab.h>		// for kmalloc/kfree
#include <linux/uaccess.h>	// copy_to_user
#include <linux/string.h>
#include <linux/device.h>
#include <linux/cdev.h>

#include "maze.h"
// #define MODULE_NAME "maze"
DEFINE_MUTEX(maze_mutex);

static dev_t devnum;
static struct cdev c_dev;
static struct class *clazz;

struct maze_info {
	coord_t coord;
	coord_t coord_pos;
	maze_t maze;
};

static pid_t pids[_MAZE_MAXUSER]; // PIDs of the users currently using the maze
static struct maze_info* maze_user_info[_MAZE_MAXUSER];

static int error_no = 0;
static int only_for_q3 = 0;

static pid_t get_current_pid(void) {
	return task_pid_nr(current);
}

static int get_user_index(void) {
	pid_t pid = get_current_pid();
	static int i;
	for (i = 0; i < _MAZE_MAXUSER; i++) {
		if (pids[i] == pid) {
			return i;
		}
	}
	return -1;
}

static int get_empty_index(void) { // get the index of the empty slot
	static int i;
	for (i = 0; i < _MAZE_MAXUSER; i++) {
		if (pids[i] == 0) {
			return i;
		}
	}
	return -1;
}

static void del_user(void) { // release all allocated resources for a process.
	int index = get_user_index(); // get the index of the current process
	// pid_t pid = get_current_pid();
	if (index == -1) {
		return;
	}
	mutex_lock(&maze_mutex);
	if (maze_user_info[index] != NULL) { 
		kfree(maze_user_info[index]); 
		maze_user_info[index] = NULL;
	}
	pids[index] = 0;
	mutex_unlock(&maze_mutex);
}

static bool have_maze(void) {
	// pid_t pid = task_pid_nr(current);
	int index = get_user_index();
	return index != -1 && maze_user_info[index] != NULL;
}

static int mazemod_dev_open(struct inode *i, struct file *f) {
	// printk(KERN_INFO "mazemod: device opened.\n");

	// printk("num_users: %d\n", num_users);
	// printk("user pid: %d\n", task_pid_nr(current));
	// printk(KERN_INFO "mazemod: device opened.\n");

	return 0;
}

static int mazemod_dev_close(struct inode *i, struct file *f) {
	if (error_no == 12){ // for multiple processes, if there is no empty slot, return -ENOMEM
		error_no = 0;
		return 0;
	}
	del_user();
	return 0;
}

static ssize_t mazemod_dev_read(struct file *f, char __user *buf, size_t len, loff_t *off) {
	static int i, j, index;
	index = get_user_index();

	if (index == -1 || maze_user_info[index] == NULL) {
		return -EBADFD; // returned when there is not a maze associated with the current process
	}

	unsigned char *maze_layout = (unsigned char *)kmalloc(maze_user_info[index]->coord.x * maze_user_info[index]->coord.y, GFP_KERNEL);
	// int* maze_layout = (int *)kmalloc(maze_user_info[index].coord.x * maze_user_info[index].coord.y, GFP_KERNEL);
	for (i = 0; i < maze_user_info[index]->coord.y; ++i) {
		for (j = 0; j < maze_user_info[index]->coord.x; ++j) {
			if (maze_user_info[index]->maze.blk[i][j] == '#') {
				maze_layout[i * maze_user_info[index]->coord.x + j] = 1;
			}
			else 
				maze_layout[i * maze_user_info[index]->coord.x + j] = 0;
			// printk("maze_layout[%d]: %d\n\n", (i * maze_user_info[index].coord.x + j), maze_layout[i * maze_user_info[index].coord.x + j]);
		}
	}
	if (copy_to_user(buf, maze_layout, maze_user_info[index]->coord.x * maze_user_info[index]->coord.y)) 
		return -EBUSY;
	kfree(maze_layout);

	return maze_user_info[index]->coord.x * maze_user_info[index]->coord.y;
}

static ssize_t mazemod_dev_write(struct file *f, const char __user *buf, size_t len, loff_t *off) {
	// coord_t* move_array = (coord_t *)kmalloc(len, GFP_KERNEL);
	// printk("mazemod_dev_write\n");
	static int i, index;
	index = get_user_index();

	if (index == -1 || maze_user_info[index] == NULL) {
		return -EBADFD; // returned when there is not a maze associated with the current process
	}

	coord_t* move_array = (coord_t *)kmalloc(len * sizeof(coord_t), GFP_KERNEL);
	if(copy_from_user(move_array, buf, len))
		return -EBUSY;

	size_t num_moves = len / sizeof(coord_t); // 512/8 = 64
	if (num_moves % 8 != 0)
		return -EINVAL;

	for (i = 0; i < num_moves; i++){
		if (maze_user_info[index]->maze.blk[maze_user_info[index]->coord_pos.y + move_array[i].y][maze_user_info[index]->coord_pos.x + move_array[i].x] != '#'){
			maze_user_info[index]->coord_pos.x += move_array[i].x;
			maze_user_info[index]->coord_pos.y += move_array[i].y;
		}
	}
	// printk(KERN_INFO "mazemod: write %zu bytes @ %llu.\n", len, *off);
	return len;
}

static void dfs(int row, int col, int end_row, int end_col, const coord_t *coord, maze_t *maze){
	int i, j, dir_order, next_row, next_col, tmp;
	int dir_x[4] = { -1, 1, 0, 0 };
	int dir_y[4] = { 0, 0, -1, 1 };
	int dir_seq[4] = { 0, 1, 2, 3 };

	// i = 0, 1, 2, 3 -> j = [0, 1, 2, 3], [1, 2, 3], [2, 3], [3]
	for (i = 0; i < 4; i++) { // randomize the direction sequence
		j = i + get_random_u32() % (4 - i);
		tmp = dir_seq[j];
		dir_seq[j] = dir_seq[i];
		dir_seq[i] = tmp;
	}

	for (i = 0; i < 4; i++) {
		dir_order = dir_seq[i]; 
		next_row = row + dir_x[dir_order] * 2;
		next_col = col + dir_y[dir_order] * 2;
		if (next_row < 1 || next_row >= coord->y - 1 || next_col < 1 || next_col >= coord->x - 1) {
			continue;
		}
		if (maze->blk[next_row][next_col] == '#') {
			maze->blk[next_row][next_col] = '.';
			maze->blk[row + dir_x[dir_order]][col + dir_y[dir_order]] = '.';
			dfs(next_row, next_col, end_row, end_col, coord, maze);
		}
	}
}

static int get_odd_number(int min, int max) { // ensure the coordinate (sx, sy) and (ex, ey) are both odd numbers.
	int num = 0;
	while (num % 2 == 0) {
		// get_random_u32();
		num = min + get_random_u32() % (max - min);
	}
	return num;
}

static void generate_maze(int rows, int cols, struct maze_info *m){
	static int i, j;
	static coord_t coord;
	static maze_t maze;
	int start_row = 0, start_col = 0, end_row = 0, end_col = 0;
	coord.y = rows;
	coord.x = cols;

	// randomly choose the start and end point, ensure the positions are not on the edge and both odd numbers.
	while (start_row == end_row && start_col == end_col) {
		// printk(KERN_INFO "start_row: %d, start_col: %d, end_row: %d, end_col: %d\n", start_row, start_col, end_row, end_col);
		start_row = get_odd_number(1, coord.y - 1);
		start_col = get_odd_number(1, coord.x - 1);
		end_row = get_odd_number(1, coord.y - 1);
		end_col = get_odd_number(1, coord.x - 1);
	}

	// initialize the maze that guarantees the begining layout is all the wall.
	maze.w = coord.y;
	maze.h = coord.x;
	for (i = 0; i < coord.y; ++i){
		for(j = 0; j < coord.x; ++j){
			maze.blk[i][j] = '#';
		}
	}

	// generate the start & end position of the maze_layout
	maze.blk[start_row][start_col] = 'S';
	maze.blk[start_row][start_col] = '*'; // initial position
	maze.sy = start_row;
	maze.sx = start_col;

	// using dfs to generate the maze path
    dfs(start_row, start_col, end_row, end_col, &coord, &maze);

    maze.blk[end_row][end_col] = 'E';
	maze.ey = end_row; 
	maze.ex = end_col;
	
	// set the user's maze info
	m->coord = coord;
	m->coord_pos.x = maze.sx;
	m->coord_pos.y = maze.sy;
	m->maze = maze;
}

static int create_maze(coord_t coord) {
	int index = get_empty_index();
	if (index == -1) {
		return -ENOMEM;
	}

	pids[index] = get_current_pid();
	maze_user_info[index] = kmalloc(sizeof(struct maze_info), GFP_KERNEL);
	generate_maze(coord.y, coord.x, maze_user_info[index]);
	return 0;
}

static long mazemod_dev_ioctl(struct file *fp, unsigned int cmd, unsigned long arg) {
	// printk(KERN_INFO "mazemod: ioctl cmd=%u arg=%lu.\n", cmd, arg);
	static int index;
	static coord_t tmp;
	switch(cmd) {
		case MAZE_CREATE:
			coord_t coord;
			if (copy_from_user(&coord, (coord_t *)arg, sizeof(coord_t))) // get the coordinates (x, y)
                return -EBUSY;
			if (coord.x <= 0 || coord.y <= 0 || coord.x > _MAZE_MAXX || coord.y > _MAZE_MAXY) // returned when any argument has an invalid value.
                return -EINVAL;

			mutex_lock(&maze_mutex);
			index = get_user_index();
			
			if (index != -1) { // the user is existed.
				// this user already have a slot
				if (maze_user_info[index] != NULL) {
					mutex_unlock(&maze_mutex);
					return -EEXIST;	// returned when a maze has already been created for the calling process.
				}
				// otherwise, create maze for it
				create_maze(coord);
			} else {
				// this user doesn't have a slot
				// get a empty slot for it
				int empty_slot = get_empty_index();
				if (empty_slot == -1){
					error_no = 12;
					mutex_unlock(&maze_mutex);
					return -ENOMEM;
				}
				create_maze(coord);
			}
			mutex_unlock(&maze_mutex);
			break;
		case MAZE_RESET:
			if(!have_maze()) return -ENOENT; // returned when no maze is created for the calling process

			index = get_user_index();
			maze_user_info[index]->coord_pos.x = maze_user_info[index]->maze.sx;
			maze_user_info[index]->coord_pos.y = maze_user_info[index]->maze.sy;			
			break;
		case MAZE_DESTROY:
			if(!have_maze()) return -ENOENT;

			// kfree the maze and reset the maze_user_info
			index = get_user_index();
			kfree(maze_user_info[index]);
			maze_user_info[index] = NULL;
			break;
		case MAZE_GETSIZE:
			if(!have_maze()) return -ENOENT; // returned when no maze is created for the calling process.
			index = get_user_index();
			tmp.x = maze_user_info[index]->maze.h;
			tmp.y = maze_user_info[index]->maze.w;
			
			if (copy_to_user((coord_t *)arg, &tmp, sizeof(coord_t))) // get the coordinates (x, y)
				return -EBUSY;
			break;
		case MAZE_MOVE:
			if(!have_maze()) return -ENOENT;

			static coord_t coord_move;
			if (copy_from_user(&coord_move, (coord_t *)arg, sizeof(coord_t))) // get the coordinates (x, y)
				return -EBUSY;

			// calculate if pos can move to the new position
			index = get_user_index();
			if (maze_user_info[index]->maze.blk[maze_user_info[index]->coord_pos.y + coord_move.y][maze_user_info[index]->coord_pos.x + coord_move.x] != '#'){
				maze_user_info[index]->coord_pos.x += coord_move.x;
				maze_user_info[index]->coord_pos.y += coord_move.y;
			}
			break;
		case MAZE_GETPOS:
			if(!have_maze()) return -ENOENT;

			index = get_user_index();
			tmp.x = maze_user_info[index]->coord_pos.x;
			tmp.y = maze_user_info[index]->coord_pos.y;

			if (copy_to_user((coord_t *)arg, &tmp, sizeof(coord_t))) // get the coordinates (x, y)
				return -EBUSY;
			break;
		case MAZE_GETSTART:
			if(!have_maze()) return -ENOENT;

			index = get_user_index();
			tmp.x = maze_user_info[index]->maze.sx;
			tmp.y = maze_user_info[index]->maze.sy;
			
			if (copy_to_user((coord_t *)arg, &tmp, sizeof(coord_t))) // get the coordinates (x, y)
				return -EBUSY;
			break;
		case MAZE_GETEND:
			if(!have_maze()) return -ENOENT;

			index = get_user_index();
			tmp.x = maze_user_info[index]->maze.ex;
			tmp.y = maze_user_info[index]->maze.ey;

			if (copy_to_user((coord_t *)arg, &tmp, sizeof(coord_t))) // get the coordinates (x, y)
				return -EBUSY;
			break;
		case ONLY_FOR_Q3: // only for question 3: create a random maze.
			only_for_q3 = 1;
			static coord_t coord_q3;
			if (copy_from_user(&coord_q3, (coord_t *)arg, sizeof(coord_t))) // get the coordinates (x, y)
                return -EBUSY;
			if (coord_q3.x <= 0 || coord_q3.y <= 0 || coord_q3.x > _MAZE_MAXX || coord_q3.y > _MAZE_MAXY) 
                return -EINVAL; // returned when any argument has an invalid value.
			create_maze(coord_q3);
			break;
		default:
            return -ENOTTY; // Inappropriate ioctl for device
	}
	return 0;
}

static const struct file_operations mazemod_dev_fops = { // file operations: system call, device driver
    .owner = THIS_MODULE,
    .open = mazemod_dev_open,
    .read = mazemod_dev_read,
    .write = mazemod_dev_write,
    .unlocked_ioctl = mazemod_dev_ioctl,
	.release = mazemod_dev_close
};

static int mazemod_proc_read(struct seq_file *m, void *v) {
	// printk(KERN_INFO "mazemod: proc read.\n");
	// char buf[] = "`hello, world!` in /proc.\n";
	// seq_printf(m, buf);
	static int i, j, k;
	if (only_for_q3) {
		seq_printf(m, "Only for Question3: %d %d\n", maze_user_info[0]->coord.x, maze_user_info[0]->coord.y);
		for (j = 0; j < maze_user_info[0]->coord.y; ++j) {
			seq_printf(m, "- %03d: ", j);
			for (k = 0; k < maze_user_info[0]->coord.x; ++k)
				seq_printf(m, "%c", maze_user_info[0]->maze.blk[j][k]);
			seq_printf(m, "\n");
		}
		seq_printf(m, "\n");
		only_for_q3 = 0;
		return 0;
	}
	
	for (i = 0; i < _MAZE_MAXUSER; ++i) {
		if (pids[i] == 0) {
			seq_printf(m, "#%02d: vacancy\n\n", i);
			continue;
		}
		seq_printf(m, "#%02d: pid %d - [%d x %d]: (%d, %d) -> (%d, %d) @ (%d, %d)\n", 
			i, pids[i], maze_user_info[i]->coord.x, maze_user_info[i]->coord.y,
			maze_user_info[i]->maze.sx, maze_user_info[i]->maze.sy, maze_user_info[i]->maze.ex, maze_user_info[i]->maze.ey,
			maze_user_info[i]->coord_pos.x, maze_user_info[i]->coord_pos.y);

			for (j = 0; j < maze_user_info[i]->coord.y; ++j) {
				seq_printf(m, "- %03d: ", j);
				for (k = 0; k < maze_user_info[i]->coord.x; ++k)
					seq_printf(m, "%c", maze_user_info[i]->maze.blk[j][k]);
				seq_printf(m, "\n");
			}
			seq_printf(m, "\n");
    }
	return 0;
}

static int mazemod_proc_open(struct inode *inode, struct file *file) {
	// printk(KERN_INFO "mazemod: proc opened.\n");
	return single_open(file, mazemod_proc_read, NULL);
}

static int maze_proc_release(struct inode *inode, struct file *file) {
	// printk(KERN_INFO "mazemod: proc released.\n");
    return single_release(inode, file);
}

static const struct proc_ops mazemod_proc_fops = { // change data between kernel and user space
	.proc_open = mazemod_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	// .proc_release = single_release,
	.proc_release = maze_proc_release
};

static char *mazemod_devnode(const struct device *dev, umode_t *mode) { // create device node
	if(mode == NULL) return NULL;
	*mode = 0666; // permission
	return NULL;
}


static int __init mazemod_init(void) // init module when insert it into kernel
{
	// create char dev
	if(alloc_chrdev_region(&devnum, 0, 1, "updev") < 0) // allocate device number
		return -1;

	if((clazz = class_create("upclass")) == NULL) // create class
		goto release_region;
		
	clazz->devnode = mazemod_devnode;

	if(device_create(clazz, NULL, devnum, NULL, "maze") == NULL)
		goto release_class;

	// char device init 
	cdev_init(&c_dev, &mazemod_dev_fops); // void cdev_init(struct cdev *cdev, const struct file_operations *fops)
	if(cdev_add(&c_dev, devnum, 1) == -1)
		goto release_device;

	// create proc
	proc_create("maze", 0, NULL, &mazemod_proc_fops);
	printk(KERN_INFO "mazemod: initialized.\n");
	return 0;    // Non-zero return means that the module couldn't be loaded.

release_device:
	device_destroy(clazz, devnum);
release_class:
	class_destroy(clazz);
release_region:
	unregister_chrdev_region(devnum, 1); // unregister device number
	return -1;
}

static void __exit mazemod_exit(void)
{
	remove_proc_entry("maze", NULL);

	cdev_del(&c_dev);
	device_destroy(clazz, devnum);
	class_destroy(clazz);
	unregister_chrdev_region(devnum, 1);
	
	printk(KERN_INFO "mazemod: exit.\n");
}

module_init(mazemod_init); // when load driver module, call init function
module_exit(mazemod_exit); // when unload driver module, call exit function

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jenny Chang");
MODULE_DESCRIPTION("lab02: Random Walk in the Kernel");