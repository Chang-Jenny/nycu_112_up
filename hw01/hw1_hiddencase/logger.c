#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // getopt
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include "logger.h"


char* arg_check[512]; // store the arguments
char* N_config[512]; // store the config file


// parse the arguments
ARGS parse_args(int argc, char *argv[]) {
  ARGS args;
  args.config_file = "config.txt";
  args.config = NULL;
  args.output_file = NULL;
  args.so_path = "./logger.so"; // default is ./logger.so
  args.command = NULL;
  args.args = NULL;

  // for(int i = 0; i < argc; i++) {
  //   printf("argv[%d]: %s\n", i, argv[i]);
  // }

  // temp for storing the arguments
  memset(arg_check, '\0', sizeof(arg_check));
  memset(N_config, '\0', sizeof(N_config));

  int cnt = 1; // which word in the argv array and the first word is the program name
  int cnt_option_args = 0; // which temp storing the arguments
  int cnt_config = 0;
  int find_command = 0; // if the current word is the command, the next is as same as spec args

  while (cnt < argc) {
    printf("argv[%d]: %s\n", cnt, argv[cnt]);
    printf("1\n");
    // command
    if (argv[cnt][0] == '.' && argv[cnt][1] == '/' && strstr(argv[cnt], "config.txt") == NULL) {
      args.command = argv[cnt];
      cnt += 1;
      find_command = 1;
      continue;
    }

    // after command argument
    if (argv[cnt][0] != '-' && find_command == 1) {
      printf("2\n");
      arg_check[cnt_option_args] = argv[cnt];
      cnt += 1;
      cnt_option_args += 1;
      find_command = 0;
      continue;
    }

    // config.txt
    // compare if "config.txt" is in the argv[cnt]
    if (strstr(argv[cnt], "config.txt") != NULL) {
      printf("3\n");
      if (cnt_config == 0) {
        args.config_file = argv[cnt];
      }
      
      N_config[cnt_config] = argv[cnt];
      cnt += 1;
      cnt_config += 1;
      continue;
    }
    // if (strcmp(argv[cnt], "config.txt") == 0) {
    //   args.config_file = argv[cnt];
    //   cnt += 1;
    //   continue;
    // }

    // -o output_file
    if (argv[cnt][0] == '-' && argv[cnt][1] == 'o') {
      printf("4\n");
      args.output_file = argv[cnt + 1];
      cnt += 2;
      continue;
    }

    // -p so_path
    if (argv[cnt][0] == '-' && argv[cnt][1] == 'p') {
      printf("5\n");
      args.so_path = argv[cnt + 1];
      cnt += 2;
      continue;
    }
    
    // check if the next argument is out of the range
    if (cnt + 1 < argc) {
      printf("6\n");
      // option args
      if (argv[cnt][0] == '-' && argv[cnt+1][0] != '-') {
        printf("7\n");
        // combine the argv[cnt] and " " and argv[cnt+1]
        char* tmp = (char*)malloc(strlen(argv[cnt]) + strlen(argv[cnt+1]) + 2);
        memset(tmp, '\0', strlen(argv[cnt]) + strlen(argv[cnt+1]) + 2);
        strcat(tmp, argv[cnt]);
        strcat(tmp, " ");
        strcat(tmp, argv[cnt+1]);
        arg_check[cnt_option_args] = tmp;
        cnt += 2;
        cnt_option_args += 1;
        continue;
      }

      // only one option args
      if (argv[cnt][0] == '-' && argv[cnt+1][0] == '-') {
        printf("8\n");
        arg_check[cnt_option_args] = argv[cnt];
        cnt += 1;
        cnt_option_args += 1;
        continue;
      }
      else {
        printf("9\n");
        arg_check[cnt_option_args] = argv[cnt];
        cnt += 1;
        cnt_option_args += 1;
        continue;
      }
    } else { // the next word is out of the range of the argv
      printf("10\n");
      arg_check[cnt_option_args] = argv[cnt];
      cnt += 1;
      cnt_option_args += 1;
      continue;
    }
  }
  args.args = arg_check;
  args.config = N_config;
  printf("\n");

  /* original way to parse the arguments
  for (int i = 0; args.args[i] != NULL; i++) {
    printf("args.args[%d]: %s\n", i, args.args[i]);
  }
  printf("\n");

  int opt;
  while ((opt = getopt(argc, argv, "o:p:")) != -1) {
    switch (opt) {
      case 'o':
        args.output_file = optarg;
        break;
      case 'p':
        args.so_path = optarg;
        break;
      case '?':
        fprintf(stderr, "case: ?\n");
        break;
      default:
        fprintf(stderr, "Usage: %s config.txt [-o file] [-p sopath] command [arg1 arg2 ...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
  }
  if (optind >= argc)  exit(EXIT_FAILURE);

  // from argv[option index] to the end: config.txt command [arg1 arg2 ...]
  args.config_file = argv[optind];
  args.command = argv[optind + 1];
  args.args = argv + optind + 2; // + 1
  */

  return args;
}

int main(int argc, char *argv[]) {
  /*
    ./logger config.txt [-o file] [-p sopath] command [arg1 arg2 ...]
    -p: set the path to your shared object, default is ./logger.so
    -o: print the output to file if specified, else print it to stderr
    arg: maybe long/short options
  */

  // parse program usage
  args = parse_args(argc, argv);
  printf("args.config_file: %s\n", args.config_file);
  printf("args.output_file: %s\n", args.output_file);
  printf("args.so_path: %s\n", args.so_path);
  printf("args.command: %s\n", args.command);
  for (int i = 0; args.args[i] != NULL; i++){
    printf("args.args[%d]: %s\n", i, args.args[i]);
  }

  setenv("LD_PRELOAD", args.so_path, 1); // set the LD_PRELOAD environment variable
  char line[50];
  memset(line, '=', sizeof(line));
  printf("LD_PRELOAD: %s\n%s\n\n", getenv("LD_PRELOAD"), line);

  setenv("SOLVER_CONFIG", args.config_file, 1); // set the SOLVER_CONFIG environment variable
  // setenv("SOLVER_CONFIG", args.config, 1); // set the SOLVER_CONFIG environment variable


  // combind the command and the arguments
  char command[256];
  memset(command, '\0', sizeof(command));

  // if the output_file is not NULL, redirect the stderr to the output_file
  strcat(command, args.command);
  if (args.output_file != NULL) {
    strcat(command, " 2> ");
    strcat(command, args.output_file);
  }

  // add the arguments to the command, so that I can get the whole command line
  for (int i = 0; args.args[i] != NULL; i++) {
    strcat(command, " ");
    strcat(command, args.args[i]);
  }

  printf("command: %s\n\n", command);
  if ( system(command) == -1) {
    printf("system failed: %s\n", strerror(errno));
  }


  // in this way, args.args need to be {command args, NULL}
  // if (execvp(args.command, args.args) == -1) {
  //   printf("execvp failed: %s\n", strerror(errno));
  // }
  return 0;
}