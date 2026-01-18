#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <sys/types.h>


#ifndef LIBC
#define LIBC "libc.so.6"
#endif

typedef struct {
  char *config_file;
  char **config;
  char *output_file;
  char *so_path;
  char *command;
  char **args;
} ARGS;
ARGS args;

// declare the prototype of the functions
void* open_handle(void*);
char** check_blacklist(const char*, const char*, const char*);
int compare_blacklist(const char*, char**);
char* handle_filename(const char*, int);
pid_t get_process_pid();
void write_log_file(const char*, const char*, const char*);
char* get_total_content(char*);
void* open_handle(void*);
int compare_read(const char*, char**);

// FILE* fopen(const char*, const char*);
// int getaddrinfo(const char*, const char*, const void*, void*);

#endif

