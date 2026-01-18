#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>

#include <elf.h>
#include <link.h>
#include "libmaze.h"


const size_t alignment = 4096; // 4KB
void* got_entries_to_move_n[1201]; // from 1 to 1200 move_n functions
void* libmaze_handle;
// defind the libmaze function pointer.
typedef int (*libmaze_maze_init)();
typedef void (*libmaze_maze_set_ptr)(void*);
typedef maze_t *(*libmaze_maze_load)(const char*);

typedef enum {
    UP=0,
    DOWN,
    LEFT,
    RIGHT,
	NO_DIRECTION
} move_direction;
move_direction move_directions[1200]; // record the move direction of the move_n function.
int max_depth = 0; // record the max depth of the move_n function.

// UP(0), DOWN(1), LEFT(2), RIGHT(3)
static int _dirx[] = { 0, 0, -1, 1 };
static int _diry[] = { -1, 1, 0, 0 };
void *direction_address[4];  // the address of the move_[dir] function.

// define the function prototype
bool dfs(maze_t *, int, int, int);
void check_path();
int solver_get_move_direction_address(void*);
void* solver_get_move_n_address(int);
int readelf();

// maze init function
int
maze_init() {
	// dlopen
	libmaze_handle = dlopen("libmaze.so", RTLD_LAZY);
	if (!libmaze_handle) {
		fprintf(stderr, "dlopen failed: %s\n", dlerror());
		return -1;
	}
	// dlsym
	// Before calling the PRELOAD library function, I check the libmaze function have been called.
	libmaze_maze_init maze_init_ptr = (libmaze_maze_init)dlsym(libmaze_handle, "maze_init");
	libmaze_maze_set_ptr maze_set_ptr = (libmaze_maze_set_ptr)dlsym(libmaze_handle, "maze_set_ptr");
	libmaze_maze_load maze_load_ptr = (libmaze_maze_load)dlsym(libmaze_handle, "maze_load");

	if (!maze_init_ptr || !maze_set_ptr || !maze_load_ptr) {
		fprintf(stderr, "dlsym failed: %s\n", dlerror());
		return -1;
	}
	maze_init_ptr();
	maze_set_ptr(maze_get_ptr());
	maze_load_ptr("./maze.txt");
	fprintf(stderr, "---The libmaze function has been completed, and now we are beginning with the libsolver.---\n");
	solver_get_move_direction_address(libmaze_handle); // obtain the address of the move_[dir] function.
	
	// dlclose
	if (dlclose(libmaze_handle) != 0) {
		fprintf(stderr, "dlclose failed: %s\n", dlerror());
		return -1;
	}
	fprintf(stderr, "UP112_GOT_MAZE_CHALLENGE.\n");
	fprintf(stderr, "SOLVER: _main = %p\n", maze_get_ptr());
	fprintf(stderr, "move_up = %p\n", direction_address[0]);
	fprintf(stderr, "move_down = %p\n", direction_address[1]);
	fprintf(stderr, "move_left = %p\n", direction_address[2]);
	fprintf(stderr, "move_right = %p\n", direction_address[3]);
	readelf();
	return 0;
}

maze_t *
maze_load(const char *fn) {
	maze_t *mz = NULL;
	FILE *fp = NULL;
	int i, j, k;

	// Determine whether there is an error in reading the maze.txt file
	if((fp = fopen(fn, "rt")) == NULL) {
		fprintf(stderr, "MAZE: fopen failed - %s.\n", strerror(errno));
		return NULL;
	}
	if((mz = (maze_t*) malloc(sizeof(maze_t))) == NULL) {
		fprintf(stderr, "MAZE: alloc failed - %s.\n", strerror(errno));
		goto err_quit;
	}
	if(fscanf(fp, "%d %d %d %d %d %d", &mz->w, &mz->h, &mz->sx, &mz->sy, &mz->ex, &mz->ey) != 6) {
		fprintf(stderr, "MAZE: load dimensions failed - %s.\n", strerror(errno));
		goto err_quit;
	}

	// Initialize the current position of the maze and the content of the maze
	mz->cx = mz->sx;
	mz->cy = mz->sy;
	for(i = 0; i < mz->h; i++) {
		for(j = 0; j < mz->w; j++) {
			if(fscanf(fp, "%d", &k) != 1) {
				fprintf(stderr, "MAZE: load blk (%d, %d) failed - %s.\n", j, i, strerror(errno));
				goto err_quit;
			}
			mz->blk[i][j] = k<<20;
		}
	}
	fclose(fp);
	fprintf(stderr, "MAZE: loaded [%d, %d]: (%d, %d) -> (%d, %d)\n",
		mz->w, mz->h, mz->sx, mz->sy, mz->ex, mz->ey);

	// Using the dfs algorithm to solve the maze.
	memset(move_directions, NO_DIRECTION, sizeof(move_directions));
	dfs(mz, mz->sx, mz->sy, 0);

	// Because changing the block to 2 will affect the maze, we need to change it back to 0.
	for(i = 0; i < mz->h; i++) {
		for(j = 0; j < mz->w; j++) {
			if (mz->blk[i][j] == 2) {
				mz->blk[i][j] = 0;
			}
		}
	}
	// Check the steps of the path from the start position to the end position.
	check_path(); 
	return mz;
err_quit:
	if(mz) free(mz);
	if(fp) fclose(fp);
	return NULL;
}

// x and new_col y "next position", within the maze and the block is not a wall
bool 
isValid(maze_t *mz, int x, int y){
	return x > 0 && x < mz->w && y > 0 && y < mz->h && mz->blk[y][x] == 0;
}

// Depth-first search algorithm to solve the maze.
bool dfs(maze_t *mz, int now_x, int now_y, int depth){
	// if the current position is the end position, the maze is solved.
	if (now_x == mz->ex && now_y == mz->ey) {
		max_depth = depth;
		fprintf(stderr, "SOLVED: (%d, %d) -> (%d, %d)\n", mz->sx, mz->sy, now_y, now_x);
		return true;
	}
	// try to move (up, down, left, right) to the new position.
	mz->blk[now_y][now_x] = 2; // if the block is visited, set it to 2, avoid visit again.

	// Select four directions in sequence.
	for (int i = 0; i < 4; i++){
		int next_x = now_x + _dirx[i];
		int next_y = now_y + _diry[i];

		if (!isValid(mz, next_x, next_y)) {
			continue;
		}

		move_directions[depth] = i;
		if (dfs(mz, next_x, next_y, depth + 1)) {
			return true;
		}
	}
	return false;
}

// Change the authority of the memory to read and write.
void 
make_memory_writable(void* addr) {

	// uintptr_t uintptr_addr = (uintptr_t)current_move_n_address_in_GOT_table;
	// uintptr_t aligned_addr = (uintptr_addr + alignment - 1) & ~(alignment - 1);

	int pagesize = getpagesize();
	void* page_start = (void*)((uintptr_t)addr & ~(pagesize - 1));
	if (mprotect(page_start, pagesize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
		fprintf(stderr, "mprotect failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

// Check the path from the start position to the end position.
void check_path(){
	int pagesize = getpagesize();
	fprintf(stderr, "pagesize = %d\n", pagesize);
	if (max_depth == 0) {
		fprintf(stderr, "No path.\n");
		return;
	}

	// check the each depth of the move_n function in the GOT table.
	for (int d = 0, i = 1; d < max_depth; d++, i++) {
		void* current_move_n_address_got = got_entries_to_move_n[i];
		make_memory_writable((void*) current_move_n_address_got);

		void* dir_func_addr = NULL;
		switch (move_directions[d]) {
			case UP:
				dir_func_addr = direction_address[0];
				break;
			case DOWN:
				dir_func_addr = direction_address[1];
				break;
			case LEFT:
				dir_func_addr = direction_address[2];
				break;
			case RIGHT:
				dir_func_addr = direction_address[3];
				break;
		}

		// change the move_n function to move_[direction] function.
		*(void**) got_entries_to_move_n[i] = dir_func_addr;
	}
	fprintf(stderr, "\nCHACKPATH: got_entries_to_move_n[1] = %p\n", got_entries_to_move_n[1]);
}

// Get the address of the move_[dir] function.
int 
solver_get_move_direction_address(void* handle) {
	void (*move_up_ptr)(void *) = dlsym(libmaze_handle, "move_up");
	void (*move_down_ptr)(void *) = dlsym(libmaze_handle, "move_down");
	void (*move_left_ptr)(void *) = dlsym(libmaze_handle, "move_left");
	void (*move_right_ptr)(void *) = dlsym(libmaze_handle, "move_right");
	if (!move_up_ptr || !move_down_ptr || !move_left_ptr || !move_right_ptr) {
		fprintf(stderr, "dlsym failed: %s\n", dlerror());
		return -1;
	}
	direction_address[0] = (void*)move_up_ptr;
	direction_address[1] = (void*)move_down_ptr;
	direction_address[2] = (void*)move_left_ptr;
	direction_address[3] = (void*)move_right_ptr;
	return 0;
}

// Get the address of the move_n function.
void* 
solver_get_move_n_address(int n) {
	libmaze_handle = dlopen("libmaze.so", RTLD_LAZY);
	if (!libmaze_handle) {
		fprintf(stderr, "dlopen failed: %s\n", dlerror());
		return NULL;
	}
	char buffer[10];
	snprintf(buffer, sizeof(buffer), "move_%d", n);
	void (*move_n_ptr)(void *) = dlsym(libmaze_handle, buffer);

	if (dlclose(libmaze_handle) != 0) {
		fprintf(stderr, "dlclose failed: %s\n", dlerror());
		return NULL;
	}
	return move_n_ptr;
}

Elf64_Shdr 
get_section(FILE *fp, Elf64_Ehdr *ehdr, char *section_name) {
	Elf64_Shdr shdr;
	Elf64_Shdr shdr_section_name;
	// read the section header string table
	fseek(fp, ehdr->e_shoff + (ehdr->e_shstrndx * sizeof(Elf64_Shdr)), SEEK_SET); // locate the section header string table
	fread(&shdr_section_name, sizeof(Elf64_Shdr), 1, fp);
	
	// read the section name string table
	char *section_name_table = (char *)malloc(shdr_section_name.sh_size);
	fseek(fp, shdr_section_name.sh_offset, SEEK_SET);
	fread(section_name_table, shdr_section_name.sh_size, 1, fp);
	
	// retrive the symtab offset, iterate the section header table
	for (int i = 0; i < ehdr->e_shnum; i++) {
		Elf64_Off sh_offset = ehdr->e_shoff + (i * sizeof(Elf64_Shdr));
		fseek(fp, sh_offset, SEEK_SET);

		fread(&shdr, sizeof(Elf64_Shdr), 1, fp);
		if (strcmp(&section_name_table[shdr.sh_name], section_name) == 0) {
			break;
		}
	}
	return shdr;
}

int readelf() {
	// open the ELF file format of ./maze
    FILE *fp = fopen("./maze", "rb");
    if(!fp){
        perror("Error opening ./maze file");
        return 1;
    }
	Elf64_Ehdr ehdr;
	fread(&ehdr, sizeof(Elf64_Ehdr), 1, fp);

	// Elf64_Shdr dynstr = get_section(fp, &ehdr, ".dynstr");
	Elf64_Shdr symtab = get_section(fp, &ehdr, ".symtab");
	Elf64_Shdr got = get_section(fp, &ehdr, ".got");
	Elf64_Shdr strtab = get_section(fp, &ehdr, ".strtab");


	// obtain the strtab, using Elf64_Shdr strtab = get_section(fp, &ehdr, ".strtab");
	Elf64_Off main_offset; // 0x1b7a9; // default main function offset
	char *strtab_char = (char *)malloc(strtab.sh_size); // sizeof(char*) = 8
	fseek(fp, strtab.sh_offset, SEEK_SET);
	fread(strtab_char, 1, strtab.sh_size, fp);


	// Read the .symtab section
	Elf64_Sym *symtab_data = (Elf64_Sym *)malloc(symtab.sh_size);
	fseek(fp, symtab.sh_offset, SEEK_SET);
	fread(symtab_data, symtab.sh_size, 1, fp);
	size_t num_symbols = symtab.sh_size / sizeof(Elf64_Sym); // sizeof(Elf64_Sym) = 24 , num_symbols = 1239
	
	// Iterate through the symbols in the .symtab section and find the name equals to "main"
	for (int i = 0; i < num_symbols; i++) {
		if (i == 0) {
			continue;
		}
		// compare the symbol name with "main", if matched, get the offset of the main function
		Elf64_Sym sym = symtab_data[i];
		if (memcmp(&strtab_char[sym.st_name], "main", 4) == 0) {
			main_offset = sym.st_value;
			fprintf(stderr, "Found main symbols (main_offset) at %p\n", (void *)main_offset);
			break;
		}
	}

	// Read the .got section
	// Elf64_Addr got_offset = got.sh_offset;
	Elf64_Addr got_offset = got.sh_addr;
	Elf64_Xword got_size = got.sh_size;
	size_t got_entries_num = got_size / sizeof(Elf64_Addr);
	fprintf(stderr, "GOT size = %ld, entry num = %ld\n", got_size, got_entries_num);


	// calculate the GOT table
	size_t found_cnt = 0;
	for (int i = 0; i < got_entries_num; i++) {
		Elf64_Addr got_entry_offset = got_offset + i * sizeof(Elf64_Addr);

		// get the current GOT entry
		void* got_entry_addr = (void *)((uintptr_t)maze_get_ptr() - (uintptr_t)main_offset + (uintptr_t)got_entry_offset);
		// void* got_entries = (void *)((uintptr_t)maze_get_ptr() - (uintptr_t)main_offset + (uintptr_t)got_entry);

		// get the function address from the GOT entry
		void* function_address = *(void**)got_entry_addr;

		// check which move_n function maps to the current GOT entry
		for (int j = 1; j <= 1200; j++) {
			if (solver_get_move_n_address(j) == function_address) {
				// void* tmp = got_entries;
				got_entries_to_move_n[j] = got_entry_addr;
				found_cnt++;
			}
		}
	}
	

	// fprintf(stderr, "READELF: got_entries_to_move_n[1] = %p\n", got_entries_to_move_n[1]);
	fprintf(stderr, "READELF: *(void**) got_entries_to_move_n[1] = %p\n", * (void**)got_entries_to_move_n[1]);
	fprintf(stderr, "Found %ld functions in GOT\n", found_cnt);
	fclose(fp);
	return 0;
}