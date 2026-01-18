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

static coord_t coord;
static coord_t tmp; // temporary coordinates when copying data to user
static maze_t maze;



static int num_users = 0; // Number of users currently using the maze
struct maze_info {
	pid_t pid;
	coord_t coord;
	coord_t coord_pos;
	maze_t maze;
};
pid_t pids[_MAZE_MAXUSER]; // PIDs of the users currently using the maze
static struct maze_info* maze_user_info[_MAZE_MAXUSER];

static int error_no = 0;
static int only_for_q3 = 0;


static int who_is_user(void) { // return the index of the user
	pid_t pid = task_pid_nr(current);
	static int i;
	for (i = 0; i < _MAZE_MAXUSER; i++) {
		if (maze_user_info[i] && maze_user_info[i]->pid == pid) {
			return i;
		}
	}
	return -1;
}

static int get_empty_slot(void) {
	static int i;
	for (i = 0; i < _MAZE_MAXUSER; i++) {
		if (maze_user_info[i] == NULL) {
			return i;
		}
	}
	return -1;
}


static void del_user(void) { // release all allocated resources for a process.
	// printk("num_users: %d\n", num_users);
	int which = who_is_user();
	if (which == -1) {
		return;
	}

	mutex_lock(&maze_mutex);
	// printk("num_users: %d\n", num_users);
	num_users -= 1;
	if (maze_user_info[which] != NULL) { 
		kfree(maze_user_info[which]); 
		maze_user_info[which] = NULL;
	}
	// if (num_users > 0) {
	// 	num_users -= 1;
	// 	if (maze_user_info[which] != NULL) { 
	// 		kfree(maze_user_info[which]); 
	// 		maze_user_info[which] = NULL;
	// 	}
	// }
	printk("num_users: %d\n", num_users);
	// while (num_users > 0) {
	// 	num_users -= 1;
	// 	if (maze_user_info[num_users] != NULL) { 
	// 		kfree(maze_user_info[num_users]); 
	// 		maze_user_info[num_users] = NULL;
	// 	}
	// }

	// printk("num_users: %d\n", num_users);
	mutex_unlock(&maze_mutex);
}

static bool have_maze(void) {
	pid_t pid = task_pid_nr(current);
	static int i;
	for (i = 0; i < _MAZE_MAXUSER; i++) {
		if (maze_user_info[i] && maze_user_info[i]->pid == pid && maze_user_info[i]->maze.blk[0][0] != '\0') {
			return true;
		}
	}
	return false;
}

static int mazemod_dev_open(struct inode *i, struct file *f) {
	// printk(KERN_INFO "mazemod: device opened.\n");

	// printk("num_users: %d\n", num_users);
	// printk("user pid: %d\n", task_pid_nr(current));
	// printk(KERN_INFO "mazemod: device opened.\n");

	return 0;
}

static int mazemod_dev_close(struct inode *i, struct file *f) {
	printk(KERN_INFO "mazemod: device closed.\n");
	// printk("num_users: %d\n", num_users);
	// printk("error_no: %d\n", error_no);
	if (error_no == 12){
		// printk("do error_no\n");
		error_no = 0;
		return 0;
	}
	del_user();
	// printk("num_users: %d\n", num_users);
	return 0;
}

static ssize_t mazemod_dev_read(struct file *f, char __user *buf, size_t len, loff_t *off) {
	static int i, j, which;
	which = who_is_user();

	if (which == -1 || maze_user_info[which]->maze.blk[0][0] == '\0')
		return -EBADFD; // returned when there is not a maze associated with the current process

	unsigned char *maze_layout = (unsigned char *)kmalloc(maze_user_info[which]->coord.x * maze_user_info[which]->coord.y, GFP_KERNEL);
	// int* maze_layout = (int *)kmalloc(maze_user_info[which].coord.x * maze_user_info[which].coord.y, GFP_KERNEL);
	for (i = 0; i < maze_user_info[which]->coord.y; ++i) {
		for (j = 0; j < maze_user_info[which]->coord.x; ++j) {
			if (maze_user_info[which]->maze.blk[i][j] == '#') {
				maze_layout[i * maze_user_info[which]->coord.x + j] = 1;
			}
			else 
				maze_layout[i * maze_user_info[which]->coord.x + j] = 0;
			// printk("maze_layout[%d]: %d\n\n", (i * maze_user_info[which].coord.x + j), maze_layout[i * maze_user_info[which].coord.x + j]);
		}
	}
	if (copy_to_user(buf, maze_layout, maze_user_info[which]->coord.x * maze_user_info[which]->coord.y)) 
		return -EBUSY;
	kfree(maze_layout);
	// printk(KERN_INFO "mazemod: read %zu bytes @ %llu.\n", len, *off);
	return maze_user_info[which]->coord.x * maze_user_info[which]->coord.y;
}

static ssize_t mazemod_dev_write(struct file *f, const char __user *buf, size_t len, loff_t *off) {
	// coord_t* move_array = (coord_t *)kmalloc(len, GFP_KERNEL);
	// printk("mazemod_dev_write\n");
	static int i, which;
	which = who_is_user();

	if(which == -1 || maze_user_info[which]->maze.blk[0][0] == '\0')
		return -EBADFD; // default is that user haven't create maze.

	coord_t* move_array = (coord_t *)kmalloc(len * sizeof(coord_t), GFP_KERNEL);
	if(copy_from_user(move_array, buf, len))
		return -EBUSY;

	size_t num_moves = len / sizeof(coord_t); // 512/8 = 64
	if (num_moves % 8 != 0)
		return -EINVAL;

	for (i = 0; i < num_moves; i++){
		if (maze_user_info[which]->maze.blk[maze_user_info[which]->coord_pos.y + move_array[i].y][maze_user_info[which]->coord_pos.x + move_array[i].x] != '#'){
			maze_user_info[which]->coord_pos.x += move_array[i].x;
			maze_user_info[which]->coord_pos.y += move_array[i].y;
		}
	}
	// printk(KERN_INFO "mazemod: write %zu bytes @ %llu.\n", len, *off);
	return len;
}

static void dfs(int row, int col, int end_row, int end_col){
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
		if (next_row < 1 || next_row >= coord.y - 1 || next_col < 1 || next_col >= coord.x - 1) {
			continue;
		}
		if (maze.blk[next_row][next_col] == '#') {
			maze.blk[next_row][next_col] = '.';
			maze.blk[row + dir_x[dir_order]][col + dir_y[dir_order]] = '.';
			dfs(next_row, next_col, end_row, end_col);
		}
	}
}

static void check_wall(int start_x, int start_y, int end_x, int end_y, int fix, int depend, int up_down){
	static int i, cnt = 0;
	// int w = coord.y - 2, h = coord.x - 2;
	if (up_down){
		// printk("start_x: %d, start_y: %d, end_x: %d, end_y: %d, fix: %d, depend: %d\n", start_x, start_y, end_x, end_y, fix, depend);
		for (i = start_x; i <= end_x; i++){
			// printk("(%d, %d): %c, ", i, fix, maze.blk[fix][i]);
			if (maze.blk[fix][i] == '#'){
				cnt+=1;
			}
		}
		// printk("cnt: %d\n", cnt);
		if (cnt == coord.x-2){
			// printk("The outer wall is connected to the wall.\n");
			for (i = start_x; i < end_x; i++){
				if (maze.blk[fix+depend][i] == '.')
					maze.blk[fix][i] = '.';
			}
		}
	}
	else{
		// printk("start_x: %d, start_y: %d, end_x: %d, end_y: %d, fix: %d, depend: %d\n", start_x, start_y, end_x, end_y, fix, depend);
		for (i = start_y; i <= end_y; i++){
			// printk("(%d, %d): %c, ", fix, i, maze.blk[i][fix]);
			if (maze.blk[i][fix] == '#'){
				cnt+=1;
			}
		}
		// printk("cnt: %d\n", cnt);
		if (cnt == coord.y-2){
			// printk("The outer wall is connected to the wall.\n");
			for (i = start_y; i < end_y; i++){
				if (maze.blk[i][fix+depend] == '.')
					maze.blk[i][fix] = '.';
			}
		}
	}
	cnt = 0;
}

static void generate_maze(void){
	// randomly choose the start and end point, ensure the positions are not on the edge
	int start_row = 1 + get_random_u32() % (coord.y - 2);
    int start_col = 1 + get_random_u32() % (coord.x - 2);
    int end_row = 1 + get_random_u32() % (coord.y - 2);
    int end_col = 1 + get_random_u32() % (coord.x - 2);
	int i, j;

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
    dfs(start_row, start_col, end_row, end_col);
	while (maze.blk[end_row+1][end_col] == '#' && maze.blk[end_row-1][end_col] == '#' && maze.blk[end_row][end_col+1] == '#' && maze.blk[end_row][end_col-1] == '#'){
		end_row = 1 + get_random_u32() % (coord.y - 2);
    	end_col = 1 + get_random_u32() % (coord.x - 2);
	}
    maze.blk[end_row][end_col] = 'E';
	maze.ey = end_row; 
	maze.ex = end_col;

	// check if the outer wall is connected to the wall
	if (coord.y > 5){
		check_wall(1, 1, coord.x-2, 1, 1, 1, 1);
		check_wall(1, coord.y-2, coord.x-2, coord.y-2, coord.y-2, -1, 1);
	}
	if (coord.x > 5){
		check_wall(1, 1, 1, coord.y-2, 1, 1, 0);
		check_wall(coord.x-2, 1, coord.x-2, coord.y-2, coord.x-2, -1, 0);
	}
}

static int create_maze(void) {
	int index = get_empty_slot();
	if (index == -1) {
		return -ENOMEM;
	}	
	generate_maze();
	maze_user_info[index] = kmalloc(sizeof(struct maze_info), GFP_KERNEL);
	maze_user_info[index]->pid = task_pid_nr(current);
	maze_user_info[index]->maze = maze;
	maze_user_info[index]->coord = coord;
	maze_user_info[index]->coord_pos.x = maze.sx;
	maze_user_info[index]->coord_pos.y = maze.sy;
	num_users += 1;
	return 0;
}

static long mazemod_dev_ioctl(struct file *fp, unsigned int cmd, unsigned long arg) {
	// printk(KERN_INFO "mazemod: ioctl cmd=%u arg=%lu.\n", cmd, arg);
	static int which;
	switch(cmd) {
		case MAZE_CREATE:
			// copy_from_user(void *to, const void __user *from, unsigned long n)

			mutex_lock(&maze_mutex);
			if (copy_from_user(&coord, (coord_t *)arg, sizeof(coord_t))) {
				// get the coordinates (x, y)
				mutex_unlock(&maze_mutex);
                return -EBUSY;
			}
			if (coord.x <= 0 || coord.y <= 0 || coord.x > _MAZE_MAXX || coord.y > _MAZE_MAXY) { 
				mutex_unlock(&maze_mutex);
                return -EINVAL; // returned when any argument has an invalid value.
			}

			which = who_is_user();
			if (which != -1) {
				// this user already have a slot
				
				// if it has maze, return -EEXIST
				if (maze_user_info[which]->maze.blk[0][0] != '\0') {
					mutex_unlock(&maze_mutex);
					return -EEXIST;
				}
				// otherwise, create maze for it
				create_maze();
			} else {
				// this user doesn't have a slot
				// get a empty slot for it
				int empty_slot = get_empty_slot();
				if (empty_slot == -1) {
					mutex_unlock(&maze_mutex);
					return -ENOMEM;
				}
				create_maze();
			}
			mutex_unlock(&maze_mutex);
			break;
			// if (which != -1 && maze_user_info[which]->maze.blk[0][0] != '\0') {
			// 	mutex_unlock(&maze_mutex);
			// 	return -EEXIST; // The user has already created a maze
			// }
			// create_maze();
			// which = who_is_user();
			// if (which != -1 && maze_user_info[which] == NULL) {
			// 	return -EEXIST;
			// }
			// if (which != -1)
			// 	if (maze_user_info[which]->maze.blk[0][0] != '\0')
			// 		return -EEXIST; // The user has already created a maze

			// if (num_users >= _MAZE_MAXUSER){
			// 	error_no = 12;
			// 	return -ENOMEM; // It aleardy have 3 maze.
			// }

			// if pid is exist but no maze.
			// mutex_lock(&maze_mutex);
			// printk("num_users: %d\n", num_users);
			// if (num_users >= _MAZE_MAXUSER){
			// 	error_no = 12;
			// 	mutex_unlock(&maze_mutex);
			// 	return -ENOMEM; // It aleardy have 3 maze.
			// }

			// if (which != -1 && maze_user_info[which]->maze.blk[0][0] == '\0'){
			// 	generate_maze();

			// 	maze_user_info[which]->maze = maze;
			// 	maze_user_info[which]->coord = coord;
			// 	maze_user_info[which]->coord_pos.x = maze.sx;
			// 	maze_user_info[which]->coord_pos.y = maze.sy;
			// }

			// if (num_users < _MAZE_MAXUSER) {
			// 	generate_maze();

			// 	maze_user_info[num_users] = kmalloc(sizeof(struct maze_info), GFP_KERNEL);
			// 	maze_user_info[num_users]->pid = task_pid_nr(current);
			// 	memset(maze_user_info[num_users]->maze.blk, '\0', sizeof(maze_user_info[num_users]->maze.blk));

			// 	// maze_user_info[num_users]->pid = task_pid_nr(current);
			// 	maze_user_info[num_users]->maze = maze;
			// 	maze_user_info[num_users]->coord = coord;
			// 	maze_user_info[num_users]->coord_pos.x = maze.sx;
			// 	maze_user_info[num_users]->coord_pos.y = maze.sy;
			// 	num_users += 1;
			// }
			// // printk("num_users: %d\n", num_users);
			// // if (num_users >= _MAZE_MAXUSER){
			// // 	printk("num_users is large and equal to 3: %d\n", num_users);
			// // 	error_no = 12;
			// // 	return -ENOMEM; // It aleardy have 3 maze.
			// // }

			// mutex_unlock(&maze_mutex);
			// break;
		case MAZE_RESET:
			if(!have_maze()) return -ENOENT; // returned when no maze is created for the calling process

			which = who_is_user();
			if (which != -1){
				maze_user_info[which]->coord_pos.x = maze_user_info[which]->maze.sx;
				maze_user_info[which]->coord_pos.y = maze_user_info[which]->maze.sy;
			}
			
			break;
		case MAZE_DESTROY:
			if(!have_maze()) return -ENOENT;

			// kfree the maze and reset the maze_user_info
			which = who_is_user();
			if (which != -1){
				pid_t pid = maze_user_info[which]->pid;
				kfree(maze_user_info[which]);
				maze_user_info[which] = NULL;
				maze_user_info[which] = kmalloc(sizeof(struct maze_info), GFP_KERNEL);
				maze_user_info[which]->pid = pid;
				memset(maze_user_info[which]->maze.blk, '\0', sizeof(maze_user_info[which]->maze.blk));
			}
			break;
		case MAZE_GETSIZE:
			if(!have_maze()) return -ENOENT; // returned when no maze is created for the calling process.

			which = who_is_user();
			if (which != -1){
				tmp.x = maze_user_info[which]->maze.h;
				tmp.y = maze_user_info[which]->maze.w;
			}
			
			if (copy_to_user((coord_t *)arg, &tmp, sizeof(coord_t))) // get the coordinates (x, y)
				return -EBUSY;
			break;
		case MAZE_MOVE:
			if(!have_maze()) return -ENOENT;

			static coord_t coord_move;
			if (copy_from_user(&coord_move, (coord_t *)arg, sizeof(coord_t))) // get the coordinates (x, y)
				return -EBUSY;

			// calculate if pos can move to the new position
			which = who_is_user();
			if (which != -1){
				if (maze_user_info[which]->maze.blk[maze_user_info[which]->coord_pos.y + coord_move.y][maze_user_info[which]->coord_pos.x + coord_move.x] != '#'){
					maze_user_info[which]->coord_pos.x += coord_move.x;
					maze_user_info[which]->coord_pos.y += coord_move.y;
				}
			}
			break;
		case MAZE_GETPOS:
			if(!have_maze()) return -ENOENT;

			which = who_is_user();
			if (which != -1){
				tmp.x = maze_user_info[which]->coord_pos.x;
				tmp.y = maze_user_info[which]->coord_pos.y;
			}

			if (copy_to_user((coord_t *)arg, &tmp, sizeof(coord_t))) // get the coordinates (x, y)
				return -EBUSY;
			break;
		case MAZE_GETSTART:
			if(!have_maze()) return -ENOENT;

			which = who_is_user();
			if (which != -1) {
				tmp.x = maze_user_info[which]->maze.sx;
				tmp.y = maze_user_info[which]->maze.sy;
			}
			
			if (copy_to_user((coord_t *)arg, &tmp, sizeof(coord_t))) // get the coordinates (x, y)
				return -EBUSY;
			break;
		case MAZE_GETEND:
			if(!have_maze()) return -ENOENT;

			which = who_is_user();
			if (which != -1) {
				tmp.x = maze_user_info[which]->maze.ex;
				tmp.y = maze_user_info[which]->maze.ey;
			}

			if (copy_to_user((coord_t *)arg, &tmp, sizeof(coord_t))) // get the coordinates (x, y)
				return -EBUSY;
			break;
		case ONLY_FOR_Q3: // only for question 3: create a random maze.
			only_for_q3 = 1;
			if (copy_from_user(&coord, (coord_t *)arg, sizeof(coord_t))) // get the coordinates (x, y)
                return -EBUSY;
			if (coord.x <= 0 || coord.y <= 0 || coord.x > _MAZE_MAXX || coord.y > _MAZE_MAXY) 
                return -EINVAL; // returned when any argument has an invalid value.
			generate_maze();
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
	if (only_for_q3){
		seq_printf(m, "Only for Question3: %d %d\n", coord.x, coord.y);
		for (j = 0; j < coord.y; ++j) {
			seq_printf(m, "- %03d: ", j);
			for (k = 0; k < coord.x; ++k)
				seq_printf(m, "%c", maze.blk[j][k]);
			seq_printf(m, "\n");
		}
		seq_printf(m, "\n");
		only_for_q3 = 0;
		return 0;
	}
	
	for (i = 0; i < _MAZE_MAXUSER; ++i) {
		if(i < num_users){
			seq_printf(m, "#%02d: pid %d - [%d x %d]: (%d, %d) -> (%d, %d) @ (%d, %d)\n", 
			i, maze_user_info[i]->pid, maze_user_info[i]->coord.x, maze_user_info[i]->coord.y,
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
		else 
			seq_printf(m, "#%02d: vacancy\n\n", i);
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