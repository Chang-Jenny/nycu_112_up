// implement the "fopen, fread, fwrite, fclose" functions
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <limits.h>
#include <arpa/inet.h>


#include "logger.h"

// declare the original API function
typedef FILE* (*fopen_original)(const char*, const char*);
typedef size_t (*fread_original)(void*, size_t, size_t, FILE*);
typedef size_t (*fwrite_original)(const void*, size_t, size_t, FILE*);
typedef int (*connect_original)(int, const struct sockaddr*, socklen_t);
typedef int (*getaddrinfo_original)(const char*, const char*, const struct addrinfo*, struct addrinfo**);
typedef int (*system_original)(const char*);
char config_file[256];

// check and return the blacklist
char** check_blacklist(const char* filename, const char* BEGIN, const char* END) {
  // get the original fopen function
  void* handle = NULL;
  handle = open_handle(handle);
  fopen_original fopen_orig_ptr = dlsym(handle, "fopen");
  if (!fopen_orig_ptr) {
    fprintf(stderr, "dlsym failed: %s\n", dlerror());
    return NULL;
  }
  // open the config.txt file
  FILE* fp = fopen_orig_ptr(filename, "r");
  if (!fp) {
    fprintf(stderr, "fopen failed: %s\n", strerror(errno));
    return NULL;
  }

  // compare the line between the BEGIN and END
  char line[256];
  char** blacklist = NULL;
  int in_blacklist = 0;
  int record = 0;

  // obtain the corresponding strings between the BEGIN and END
  while (fgets(line, sizeof(line), fp)) { 
    // remove the newline character
    if(line[strlen(line) - 1] == '\n')
      line[strlen(line) - 1] = '\0';

    // compare with the BEGIN and END, obtain the value between the them
    if (strcmp(line, BEGIN) == 0) {
      in_blacklist = 1;
      continue;
    }
    if (strcmp(line, END) == 0) {
      in_blacklist = 0;
      break;
    }
    // if the line is in the blacklist, add it to the blacklist
    if (in_blacklist == 1) {
      // change the allocated memory size to add a new blacklist
      char** new_list = realloc(blacklist, (record + 1) * sizeof(char *));
      if (new_list == NULL) {
        fprintf(stderr, "realloc failed: %s\n", strerror(errno));
        return NULL;
      }
      blacklist = new_list;
      blacklist[record] = strdup(line);
      // printf("blacklist[%d]: %s\n", record, blacklist[record]);
      record += 1;
    }
  }
  fclose(fp);
  blacklist[record] = NULL; // set the last element to NULL
  return blacklist;
}

// handle the symbolic link
char* handle_symbolic_link(const char* filename) {
  // check the filename is a symbolic link or not
  struct stat sb;
  lstat(filename, &sb);
  if (S_ISLNK(sb.st_mode)) { 
    // if the file is a symbolic link, find out the real path
    char *buf;
    ssize_t nbytes, bufsiz;
    bufsiz = sb.st_size + 1;
    // Some magic symlinks under (for example) /proc and /sys report 'st_size' as zero. In that case, take PATH_MAX as a "good enough" estimate.
    if (sb.st_size == 0) 
      bufsiz = PATH_MAX;

    buf = malloc(bufsiz); // contain a terminating null byte ('\0')
    if (buf == NULL) {
      perror("malloc");
      exit(EXIT_FAILURE);
    }
    // read the symbolic link
    nbytes = readlink(filename, buf, bufsiz); 
    if (nbytes == -1) {
      perror("readlink");
      exit(EXIT_FAILURE);
    }

    // Print only 'nbytes' of 'buf', as it doesn't contain a terminating null byte ('\0').
    char* realname = malloc(nbytes);
    for (int i = 0; i < nbytes; i++) {
      realname[i] = buf[i];
    }
    // printf("'%s' points to '%.*s'\n", filename, (int) nbytes, buf);
    // printf("[debug] realname: %s\n", realname);
    // printf("[symbolic link]: %s  => %s\n", filename, realname);
    // printf("[symbolic link]: %ld \n", nbytes);
    // printf("[symbolic link]: %s  => %s\n", filename, realname);

    // only get the nbytes of the realname
    realname[nbytes] = '\0'; 
    // printf("[symbolic link]: %s  => %s\n", filename, realname);


    return realname;
  } else {
    char* realname = malloc(strlen(filename));
    // printf("[debug] strlen(filename): %ld\n", strlen(filename));
    for (long unsigned int i = 0; i < strlen(filename); i++) {
      // printf("[debug] filename[%d]: %c\n", i, filename[i]);
      realname[i] = filename[i];
    }
    realname[strlen(filename)] = '\0';
    return realname;
  }
  return (char*) filename;
}

// compare the filename with the blacklist, if in the blacklist, return 1, else return 0
int compare_blacklist(const char* filename, char** blacklist) {
  // int in_blacklist = 0;
  if (blacklist != NULL) {
    for (int i = 0; blacklist[i] != NULL; i++) {  // for each blacklist
      // printf("[compare %s ] blacklist[%d]: %s\n", filename, i, blacklist[i]);
      char* path = blacklist[i];
      int cnt = 0;

      // comapre the filename with the blacklist until the end of the filename or the '*' character
      while (filename[cnt] != '\0' && path[cnt] != '*' && filename[cnt] == path[cnt]) {
        cnt++;
      }

      // if the filename is in the blacklist
      if (path[cnt] == '*' || filename[cnt] == '\0') {
        return 1;
      } 
      // else {
      //   continue;
      // }

      // if blacklist is in part of the filename
      // if the last part of path is *, remove the * and compare the filename with the path
      if (path[strlen(path) - 1] == '*') {
        char* new_path = malloc(strlen(path));
        if (new_path == NULL) {
          perror("malloc");
          return 0;
        }
        strncpy(new_path, path, strlen(path) - 1);
        new_path[strlen(path) - 1] = '\0';
        if (strstr(filename, new_path) != NULL) {
          return 1;
        }
      }

      // if is this case: file.txt compare to ./file.txt
      if (path[0] == '.' && path[1] == '/') {
        // take the last part of the filename
        char* only_filename = handle_filename(filename, 1);
        // compare the only filename with the blacklist without "./"
        if (strcmp(only_filename, path + 2) == 0) {
          return 1;
        }
      }

    }
  }
  
  return 0;
}

// handle the filename
char* handle_filename(const char* filename, int extension) {
  static char last_part[100]; // store the least directory name
  static char only_filename[100]; // store the only filename without the path and extension

  // retrieve the last part of the filename
  char* p = strrchr(filename, '/');
  if (p == NULL) {
    strcpy(last_part, filename);
    // printf("last_part: %s\n", last_part);
  } else {
    strcpy(last_part, p + 1);
    // printf("last_part: %s\n", last_part);
  }
  // printf("[debug] last_part: %s\n", last_part);
  if (extension == 1){
    return last_part;
  }

  // retrieve the only filename without the path and extension
  char* p_name = strrchr(last_part, '.');
  if (p_name == NULL) {
    strcpy(only_filename, last_part);
    // printf("only_filename: %s\n", only_filename);
  } else {
    strncpy(only_filename, last_part, p_name - last_part);
    // printf("only_filename: %s\n", only_filename);
  }

  return only_filename;
}

// get the current pid
pid_t get_process_pid(){
  return getpid();
}

// write to log file
void write_log_file(const char* filename, const char* content, const char* functionname) {
  // create the log file {pid}-{filename}-{read/write}.log
  // char* log_filename = malloc(256);
  char log_filename[256];
  char suffix_log_filename[256];
  pid_t pid = get_process_pid();
  snprintf(log_filename, sizeof(log_filename), "%d-%s-%s.log", pid, filename, functionname);
  snprintf(suffix_log_filename, sizeof(suffix_log_filename), "-%s-%s.log", filename, functionname);

  // use the original fopen function
  void* handle = NULL;
  handle = open_handle(handle);
  fopen_original fopen_orig_ptr = dlsym(handle, "fopen");
  if (!fopen_orig_ptr) {
    fprintf(stderr, "dlsym failed: %s\n", dlerror());
    return;
  }

  FILE* log_file = fopen_orig_ptr(log_filename, "a");
  if (log_file == NULL) {
    fprintf(stderr, "fopen failed: %s\n", strerror(errno));
    return;
  }
  fprintf(log_file, "%s", content);
  fclose(log_file);


  // // check the "-{filename}-{read/write}.log" file is exist or not
  // char *dirname = ".";
  // DIR *dir = opendir(dirname);
  // if (dir == NULL) {
  //   perror("opendir");
  //   return;
  // }
  // int exist = 0;
  // struct dirent *entry;
  // while ((entry = readdir(dir)) != NULL) {
  //   if (strstr(entry->d_name, suffix_log_filename) != NULL) {
  //     FILE* log_file = fopen_orig_ptr(entry->d_name, "a");
  //     if (log_file == NULL) {
  //       fprintf(stderr, "fopen failed: %s\n", strerror(errno));
  //       return;
  //     }
  //     fprintf(log_file, "%s", content);
  //     fclose(log_file);
  //     exist = 1;
  //     break;
  //   } 
  // }

  // // if the log file is not exist, create a new one
  // if (exist == 0) {
  //   FILE* log_file = fopen_orig_ptr(log_filename, "a");
  //   if (log_file == NULL) {
  //     fprintf(stderr, "fopen failed: %s\n", strerror(errno));
  //     return;
  //   }
  //   fprintf(log_file, "%s", content);
  //   fclose(log_file);
  // }
}

// obtain the total content (whether fread or fwrite)
char* get_total_content(char* ptr) {
  char* total_content = malloc(strlen(ptr) + 1); // not include the null character
  if (total_content == NULL) {
    perror("malloc");
    return NULL;
  }
  memcpy(total_content, ptr, strlen(ptr));
  total_content[strlen(ptr)] = '\0'; // add the null character at the end of the content
  char *p = total_content;


  // handle the issue of the latest symbol of total_content is '\n', need to printf it out as '\n' instead of "real newline"
  char* not_translate = malloc(strlen(ptr) * 2);
  char* not_p = not_translate;
  
  while (*p != '\0') {
    if (*p == '\n') {
      *not_p = '\\';
      not_p++;
      *not_p = 'n';
      not_p++;
      p++;
    }
    *not_p = *p;
    p++;
    not_p++;
  }
  
  not_translate[strlen(not_translate)] = '\0'; // add the null character at the end of the content
  // for (int i = 0; i < strlen(not_translate); i++) {
  //   printf("[debug] not_translate[%d]: %c\n", i, not_translate[i]);
  // }

  return not_translate;
}

// dlopen the LIBC: libc.so.6
void* open_handle(void* handle){
  handle = dlopen(LIBC, RTLD_LAZY);
	if (!handle) {
		fprintf(stderr, "dlopen failed: %s\n", dlerror());
		return NULL;
	}
  return handle;
}

// comapre the content include text
int compare_read(const char* content, char** blacklist) {
  // comapre if content include text or not.
  for (int i = 0; blacklist[i] != NULL; i++) {
    if (strstr(content, blacklist[i]) != NULL) {
      return 1;
    }
  }
  return 0;

}

// fopen function
FILE* fopen(const char* filename, const char* mode) {
  // check the filename to each blacklist
  memset(config_file, '\0', sizeof(config_file));
  strcat(config_file, "./");
  strcat(config_file, getenv("SOLVER_CONFIG"));

  // char* config_file = getenv("SOLVER_CONFIG");
  char** blacklist = check_blacklist(config_file, "BEGIN open-blacklist", "END open-blacklist");

  // handle symbolic linked files and check the filename is a symbolic link or not
  // char* tmp = "./test_link_to_pikachu.txt"; // for debug
  char* real_filename = handle_symbolic_link(filename);

  // compare the filename with the blacklist
  int in_blacklist_filename = compare_blacklist(filename, blacklist);
  // printf("[debug] in_blacklist_filename: %d\n", in_blacklist_filename);
  int in_blacklist_real_filename = compare_blacklist(real_filename, blacklist);
  // printf("[debug] in_blacklist_real_filename: %d\n", in_blacklist_real_filename);
  int in_blacklist = in_blacklist_filename || in_blacklist_real_filename;

  // run the original fopen first
  void* handle = NULL;
  handle = open_handle(handle);
  // open_handle(handle);
  fopen_original fopen_orig_ptr = dlsym(handle, "fopen");
  if (!fopen_orig_ptr) {
    fprintf(stderr, "dlsym failed: %s\n", dlerror());
    return NULL;
  }
  FILE* file = fopen_orig_ptr(filename, mode);
  // fprintf(stderr, "[original] fopen(\"%s\", \"%s\") = %p\n", filename, mode, file);

  // execute the solver "fopen" function
  if (in_blacklist) {
    file = 0x0;
    // set errno to EACCES
    errno = EACCES;
    fprintf(stderr, "[logger] fopen(\"%s\", \"%s\") = 0x%lx\n", filename, mode, (unsigned long) file);
    return NULL;
  }
  fprintf(stderr, "[logger] fopen(\"%s\", \"%s\") = 0x%lx\n", filename, mode, (unsigned long) file);
  return file;
}

// fread function
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
  // if (errno) {
  //   // exit(EXIT_FAILURE);
  //   printf("===[fread]===============\n");
  //   return 0;
  // }
  // printf("===[no fread]===============\n");
  // check the filename to each blacklist
  memset(config_file, '\0', sizeof(config_file));
  strcat(config_file, "./");
  strcat(config_file, getenv("SOLVER_CONFIG"));
  char** blacklist = check_blacklist(config_file, "BEGIN read-blacklist", "END read-blacklist");

  // compare the filename with the blacklist
  int fd = fileno(stream);
  char buf[256];
  // get the file name
  snprintf(buf, sizeof(buf), "/proc/self/fd/%d", fd);
  char* real_path = handle_symbolic_link(buf);
  // printf("[fread] path: %s\n", real_path);
  // real_path = handle_symbolic_link(real_path);
  // printf("[fread] real_path: %s\n", real_path);
  // receive the only file name without the path and extension
  // printf("[fread] real_path: %s\n", real_path);
  char* only_filename = handle_filename(real_path, 0);
  // printf("[debug] only_filename: %s\n", only_filename);

  // run the original fread first
  void* handle = NULL;
  handle = open_handle(handle);
  // open_handle(handle);
  fread_original fread_orig_ptr = dlsym(handle, "fread");
  if (!fread_orig_ptr) {
    fprintf(stderr, "dlsym failed: %s\n", dlerror());
    return 0;
  }
  size_t ret = fread_orig_ptr(ptr, size, nmemb, stream);

  // get the content of the fread function
  char* content = malloc((size * nmemb) + 1);
  if (content == NULL) {
    perror("malloc");
    return 0;
  }
  // if the content size is larger than nmemb, the content will be truncated
  memcpy(content, ptr, size * nmemb);
  content[size * nmemb] = '\0'; // add the null character at the end of the content
  // write_log_file(only_filename, content, "read");
  // char* total_content = get_total_content((char*) ptr);
  // printf("[read] total_content: %s\n", total_content);

  // int in_blacklist = compare_blacklist(content, blacklist);
  int in_blacklist = compare_read(content, blacklist);
  // fprintf(stderr, "[original] fread(%p, %zu, %zu, %p) = %zu\n", ptr, size, nmemb, stream, ret);

  // execute the solver "fread" function
  if (in_blacklist) {
    // printf("============read===========\n");
    errno = EACCES;
    ret = 0;
    memcpy(ptr, "\0", size * nmemb);
    // using fseek to set the file position indicator to the before nmemb bytes
    fseek(stream, -size * nmemb, SEEK_CUR);
  }
  fprintf(stderr, "[logger] fread(%p, %zu, %zu, %p) = %zu\n", ptr, size, nmemb, stream, ret);
  if (!in_blacklist) {
    char *next_line = "\n";
    // printf("only_filename: %s\n", only_filename);
    write_log_file(only_filename, (char*) ptr, "read");
    write_log_file(only_filename, next_line, "read");
  }
  // write_log_file(only_filename, (char*) ptr, "read");
  return ret;
}

// fwrite function
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
  // if (errno == EACCES) {
  //   printf("=============[fwrite]===========\n");
  //   // exit(EXIT_FAILURE);
  //   return 0;
  // }
  // check the filename to each blacklist
  memset(config_file, '\0', sizeof(config_file));
  strcat(config_file, "./");
  strcat(config_file, getenv("SOLVER_CONFIG"));
  char** blacklist = check_blacklist(config_file, "BEGIN write-blacklist", "END write-blacklist");
  
  // get the file name
  int fd = fileno(stream);
  char buf[256];
  snprintf(buf, sizeof(buf), "/proc/self/fd/%d", fd);
  char* real_path = handle_symbolic_link(buf);
  // printf("[fwrite] path: %s\n", real_path);
  // real_path = handle_symbolic_link(real_path);
  // printf("[fread] real_path: %s\n", real_path);
  
  // receive the only file name without the path and extension
  char* only_filename = handle_filename(real_path, 0);
  // printf("[fwrite] only_filename: %s\n", only_filename);

  // compare the filename with the blacklist
  int in_blacklist = compare_blacklist(real_path, blacklist);
  // printf("[fwrite] in_blacklist: %d\n", in_blacklist);

  // get the content of the fwrite
  char* content = malloc((size * nmemb) + 1);
  if (content == NULL) {
    perror("malloc");
    return 0;
  }
  // if the content size is larger than nmemb, the content will be truncated, but I need to printf all the content
  memcpy(content, ptr, size * nmemb);
  content[size * nmemb] = '\0'; // add the null character at the end of the content
  // write_log_file(only_filename, content, "write");

  // obtain the total content
  char* total_content = get_total_content((char*) ptr);

  // run the original fwrite first
  void* handle = NULL;
  handle = open_handle(handle);
  fwrite_original fwrite_orig_ptr = dlsym(handle, "fwrite");
  if (!fwrite_orig_ptr) {
    fprintf(stderr, "dlsym failed: %s\n", dlerror());
    return 0;
  }
  size_t ret = fwrite_orig_ptr(ptr, size, nmemb, stream);
  // fprintf(stderr, "[original] fwrite(%p, %zu, %zu, %p) = %zu\n", ptr, size, nmemb, stream, ret);

  if (in_blacklist) {
    ret = 0;
    errno = EACCES;
  }
  // execute the solver "fwrite" function
  fprintf(stderr, "[logger] fwrite(\"%s\", %zu, %zu, %p) = %zu\n", total_content, size, nmemb, stream, ret);
  if (in_blacklist) {
    content = "\0";
  }else{
    char* next_line = "\n";
    // write_log_file(only_filename, get_total_content(content), "write");

    // only receive total_content from 0 to size
    char* log_content = malloc(size * nmemb + 1);
    if (log_content == NULL) {
      perror("malloc");
      return 0;
    }
    memcpy(log_content, total_content, size * nmemb);
    log_content[size * nmemb] = '\0'; // add the null character at the end of the content
    write_log_file(only_filename, log_content, "write");
    write_log_file(only_filename, next_line, "write");
  }
  return ret;
}

// connect function
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  // check the filename to each blacklist
  memset(config_file, '\0', sizeof(config_file));
  strcat(config_file, "./");
  strcat(config_file, getenv("SOLVER_CONFIG"));
  char** blacklist = check_blacklist(config_file, "BEGIN connect-blacklist", "END connect-blacklist");
  // int in_blacklist = compare_blacklist(sockfd, blacklist);
  int in_blacklist = 0;
  // get the real address of the addr
  struct sockaddr_in *addr_in = (struct sockaddr_in *) addr;
  char* ip = inet_ntoa(addr_in->sin_addr);
  if (ip == NULL) {
    fprintf(stderr, "inet_ntoa failed: %s\n", strerror(errno));
    return 0;
  }else
  // printf("[connect] ip: %s\n", ip);
  in_blacklist = compare_blacklist(ip, blacklist);

  // run the original connect first
  void* handle = NULL;
  handle = open_handle(handle);
  connect_original connect_orig_ptr = dlsym(handle, "connect");
  // int (*connect_orig_ptr)(int, const struct sockaddr*, socklen_t) = dlsym(handle, "connect");
  if (!connect_orig_ptr) {
    fprintf(stderr, "dlsym failed: %s\n", dlerror());
    return 0;
  }
  int ret = connect_orig_ptr(sockfd, addr, addrlen);
  // fprintf(stderr, "[original] connect(%d, %p, %d) = %d\n", sockfd, addr, addrlen, ret);
  if (in_blacklist) {
    ret = -1;
    errno = ECONNREFUSED;
  }
  // execute the solver "connect" function
  fprintf(stderr, "[logger] connect(%d, \"%s\", %d) = %d\n", sockfd, ip, addrlen, ret);
  return ret;
}

// getaddrinfo function (hostname, servename, hints, res)
int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
  memset(config_file, '\0', sizeof(config_file));
  strcat(config_file, "./");
  strcat(config_file, getenv("SOLVER_CONFIG"));
  // check the filename to each blacklist
  char** blacklist = check_blacklist(config_file, "BEGIN getaddrinfo-blacklist", "END getaddrinfo-blacklist");

  // compare the addr with the blacklist
  int in_blacklist = compare_blacklist(node, blacklist);
  
  // run the original getaddrinfo first
  void* handle = NULL;
  handle = open_handle(handle);
  getaddrinfo_original getaddrinfo_orig_ptr = dlsym(handle, "getaddrinfo");
  if (!getaddrinfo_orig_ptr) {
    fprintf(stderr, "dlsym failed: %s\n", dlerror());
    return 0;
  }
  int ret = getaddrinfo_orig_ptr(node, service, hints, res);
  // fprintf(stderr, "[original] getaddrinfo(%s, %p, %p, %p) = %d\n", node, service, hints, res, ret);

  if (in_blacklist) {
    ret = EAI_NONAME;
    errno = EAI_NONAME;
  }
  // execute the solver "getaddrinfo" function
  fprintf(stderr, "[logger] getaddrinfo(%s, %p, %p, %p) = %d\n", node, service, hints, res, ret);
  return ret;
}

// system function
int system(const char *command) {
  // run the original system first
  void* handle = NULL;
  handle = open_handle(handle);
  // open_handle(handle);
  system_original system_orig_ptr = dlsym(handle, "system");
  // int (*system_orig_ptr)(const char*) = dlsym(handle, "system");
  if (!system_orig_ptr) {
    fprintf(stderr, "dlsym failed: %s\n", dlerror());
    return 0;
  }
  int ret = system_orig_ptr(command);
  // fprintf(stderr, "[original] system(%s) = %d\n", command, ret);

  // execute the solver "system" function
  fprintf(stderr, "[logger] system(\"%s\") = %d\n", command, ret);
  return ret;
}