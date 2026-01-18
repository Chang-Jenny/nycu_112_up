#ifndef SDB_H
#define SDB_H


// prototypes of functions
char* print_prompt();

// prototypes of the commands
pid_t load_program(char *);
void disassemble(pid_t, uint64_t);

// powerful functions
void print_regs(pid_t);
#endif