#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <elf.h>
#include <capstone/capstone.h>

#include "sdb.h"
#define BUFFER_SIZE 256
#define NUM_INTRUCTIONS 5
#define MAX_BREAKPOINTS 10
#define INT3 0xcc
#define NOP 0x90
#define SI_TRAP 2

size_t total_count = 0; // total number of instructions in the text section
uint64_t end_address = 0; // end address of the text section
int bp_again = 0; // 1 means need to set the breakpoint again
int which_bp = 0; // record the breakpoint id that need to be set again
uint64_t pre_orig_rax = 0; // record the original rax previous syscall
int next_si_code = 0; // record the next si_code

typedef struct {
  int id;
  uint64_t addr;
  long original_code;
  long int3_code;
  int active;
  int ignore_rip_equal_break; // 1 means ignore the breakpoint when regs.rip is equal to the breakpoint
} bp_t;

bp_t breakpoints[MAX_BREAKPOINTS]; // store array of all breakpoints
int num_breakpoints = 0; // record the number of breakpoints
int enter = 0x01; // syscall enter or leave

// TODO () when breakpoint is as the same as regs.rip, dont need to hit the bp and directly run the next instruction
//  (v) si. (v) cont, () syscall

// TODO () check if breakpoint need to be int3 again after hit the breakpoint
//  (v) at rip. (v) not at rip


// TODO (v) patch memory: the value need to separate into bytes and patch into the memory

// TODO () rip=401005 break 401005 break 401008 si/cont/syscall

// errquit
void errquit(char *msg) {
  perror(msg);
  exit(1);
}


struct user_regs_struct get_regs(pid_t child_pid) {
  struct user_regs_struct regs;
  if (ptrace(PTRACE_GETREGS, child_pid, 0, &regs) < 0) {
    errquit("ptrace GETREGS@get_regs");
  }
  return regs;
}


void set_breakpoints(pid_t pid, uint64_t curr_addr) {
  struct user_regs_struct regs;
  regs = get_regs(pid);

  // check if curr_addr is already a breakpoint
  for (int i = 0; i < num_breakpoints; i++) {
    if (breakpoints[i].addr == curr_addr && breakpoints[i].active) {
      printf("** already set a breakpoint at 0x%lx.\n", curr_addr);
      return;
    }
  }

  // set the breakpoint
  long data = ptrace(PTRACE_PEEKTEXT, pid, (void *)curr_addr, NULL); // get a word from a memory address of the tracee.
  if (data == -1 && errno != 0) {
    errquit("ptrace PEEKTEXT@set_breakpoints");
  }

  long data_trap = (data & 0xffffffffffffff00) | INT3;
  if (ptrace(PTRACE_POKETEXT, pid, (void *)curr_addr, (void *)data_trap) < 0) {
    errquit("ptrace POKETEXT@set_breakpoints");
  }
  // add the breakpoint to the array
  breakpoints[num_breakpoints].id = num_breakpoints;
  breakpoints[num_breakpoints].addr = curr_addr;
  breakpoints[num_breakpoints].original_code = data;
  breakpoints[num_breakpoints].int3_code = data_trap;
  breakpoints[num_breakpoints].active = 1;
  breakpoints[num_breakpoints].ignore_rip_equal_break = (regs.rip == curr_addr);
  if (breakpoints[num_breakpoints].ignore_rip_equal_break) {
    printf("** rip and bp has the same addr at 0x%lx.\n", curr_addr);
  }
  num_breakpoints++;

  printf("** set a breakpoint at 0x%lx.\n", curr_addr);
}


void print_breakpoints() {
  if (num_breakpoints == 0) {
    printf("** no breakpoints.\n");
    return;
  }
  int bp = 0;
  for (int i = 0; i< num_breakpoints; i++) {
    if (breakpoints[i].active) {
      bp = 1;
      break;
    }
  }
  if (bp) {
    printf("%-6s Address\n", "Num");
    for (int i = 0; i < num_breakpoints; i++) {
      if (breakpoints[i].active){
        printf("%-6d 0x%lx\n", breakpoints[i].id, breakpoints[i].addr);
      }
    }
  }
  else {
    printf("** no breakpoints.\n");
  }
  
}


void delete_breakpoint(pid_t pid, int id) {
  if (id >= num_breakpoints) {
    printf("** breakpoint %d does not exist.\n", id);
    return;
  }
  long data = ptrace(PTRACE_PEEKTEXT, pid, (void *)breakpoints[id].addr, NULL);
  if (data == -1 && errno != 0) {
    errquit("ptrace PEEKTEXT@delete_breakpoint");
  }
  if (ptrace(PTRACE_POKETEXT, pid, (void *)breakpoints[id].addr, (void *)breakpoints[id].original_code) < 0) {
    errquit("ptrace POKETEXT@delete_breakpoint");
  }
  if (breakpoints[id].active == 0) {
    printf("** breakpoint %d does not exist.\n", id);
    return;
  }
  breakpoints[id].active = 0;
  printf("** delete breakpoint %d.\n", id);
  return;
}


void handle_breakpoints(pid_t pid, struct user_regs_struct regs, char* command) {
  printf("(hb) regs.rip: %llx\n", regs.rip);
  // 0. RIP - 1 and store the regs.rip to ptrace SETREGS
  regs.rip-=1;
  // printf("(handle_breakpoints) regs.rip: %llx\n", regs.rip);
  ptrace(PTRACE_SETREGS, pid, 0, &regs);
  printf("(hb) regs.rip: %llx\n", regs.rip);

  for (int i = 0; i < num_breakpoints; i++) {
    if (breakpoints[i].active && breakpoints[i].addr == regs.rip) { // hit the alive breakpoint

      // hit the breakpoint but the breakpoint is at the same addr as regs.rip
      if (breakpoints[i].ignore_rip_equal_break) {
        printf("(hb) at the same addr.\n");
        breakpoints[i].ignore_rip_equal_break = 0;

        
        // 1. PTRACE_POKETEXT: restore the original instruction
        long bp_data = ptrace(PTRACE_PEEKTEXT, pid, (void *)breakpoints[i].addr, NULL);
        bp_data = (bp_data & 0xffffffffffffff00);
        long origiinal_data = breakpoints[i].original_code & 0x00000000000000ff;
        long handle_data = bp_data | origiinal_data;

        if (ptrace(PTRACE_POKETEXT, pid, (void *)breakpoints[i].addr, (void *)handle_data) < 0){
          errquit("ptrace POKETEXT@handle_breakpoints");
        }

        // check: 目前的 rip 和 bp 位址相同
        long data = ptrace(PTRACE_PEEKTEXT, pid, (void *)breakpoints[i].addr, NULL);
        printf("(hb) (1) -----> data: 0x%lx\n", data);
        regs = get_regs(pid);
        printf("(hb) (1) -----> regs.rip: %llx\n", regs.rip);

        // 分成三種情況：si, cont, syscall
        // (a) si: single step 到下一個指令
        if (strncmp(command, "si", 2) == 0) {
          // 2. PTRACE_SINGLESTEP: single step
          ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL); // need to wait for the child process to stop, then can get regs.
          
          // 3. waitpid
          waitpid(pid, NULL, 0);

          // 4. PTRACE_GETREGS: set the regs
          regs = get_regs(pid);
          printf("(hb) (4) -----> regs.rip: %llx\n", regs.rip);
          ptrace(PTRACE_SETREGS, pid, 0, &regs);


          // 5. PTRACE_POKETEXT: set the breakpoint again -> check the rip address!!! -> directly use (void *)breakpoints[i].addr
          long original_data = ptrace(PTRACE_PEEKTEXT, pid, (void *)breakpoints[i].addr, NULL);
          original_data = (original_data & 0xffffffffffffff00);
          long int3_code = breakpoints[i].int3_code & 0x00000000000000ff;
          long handle_data = original_data | int3_code;

          if (ptrace(PTRACE_POKETEXT, pid, (void *)breakpoints[i].addr, (void *)(handle_data)) == -1) {
            errquit("ptrace POKETEXT@handle_breakpoints");
          }
          data = ptrace(PTRACE_PEEKTEXT, pid, (void *)breakpoints[i].addr, NULL);
          printf("(hb) (5) -----> data: 0x%lx\n", data);

          // 6. need to check if the new rip is equal to the breakpoint address
          // if equal, need to handle the breakpoint again
          regs = get_regs(pid);
          printf("(hb) (6) -----> regs.rip: %llx\n", regs.rip);
          if (regs.rip == breakpoints[i+1].addr && breakpoints[i+1].active && (i+1) < num_breakpoints) {
            regs.rip += 1; // 假設 rip 已經踩到 bp 位址
            handle_breakpoints(pid, regs, "si");
          }
        }

        // (b) cont: PTRACE_CONT
        else if (strncmp(command, "cont", 4) == 0) {
          int status;

          // 2. 執行到下一個 bp 的位址：eg. bp=401008, after runed rip=401009
          ptrace(PTRACE_CONT, pid, NULL, NULL);

          // 3. waitpid
          waitpid(pid, &status, 0);
          struct user_regs_struct r = get_regs(pid);
          printf("(hb) (bp-CONT) (3) regs.rip: %llx\n", r.rip);
          bp_again = 1; // 紀錄要還原 rip 的 bp
          which_bp = i; // 是哪個 bp


          // 4. 減一存回去 regs.rip，因爲已經踩到 bp 了
          r.rip -= 1;
          ptrace(PTRACE_SETREGS, pid, 0, &r);

          // 5. need to check if the next rip is equal to the breakpoint address
          // if equal, need to handle the breakpoint again.
          r = get_regs(pid);
          printf("(hb) (bp-CONT) (5) regs.rip: %llx\n", r.rip);
          if (r.rip == breakpoints[i+1].addr && breakpoints[i+1].active && (i+1) < num_breakpoints) {
            r.rip+=1;
            handle_breakpoints(pid, r, "cont");
          }
        }

        // (c) syscall: syscall to next syscall
        else if (strncmp(command, "syscall", 7) == 0) {
          struct user_regs_struct regs = get_regs(pid);
          printf("(hb) (bp-SYS) regs.rip: %llx\n", regs.rip);

          // 2. PTRACE_SYSCALL: 
          int status;
          ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_EXITKILL|PTRACE_O_TRACESYSGOOD); 
          ptrace(PTRACE_SYSCALL, pid, NULL, NULL);

          // 3. waitpid
          waitpid(pid, &status, 0);
          regs = get_regs(pid);
          printf("(hb) (bp-SYS) regs.rip: %llx\n", regs.rip);
          bp_again = 1;
          which_bp = i;

          // 4. 確認目前停下是因為 syscall 還是 bp
          // 先取得 siginfo：si_code=133: syscall, si_code=128: bp
          siginfo_t siginfo;
          memset(&siginfo, 0, sizeof(siginfo));
          ptrace(PTRACE_GETSIGINFO, pid, NULL, &siginfo);
          next_si_code = siginfo.si_code;
          printf("(bp-SYS) siginfo.si_code: %d\n", siginfo.si_code);

          // 5. need to check if the next instruction is syscall or breakpoint
          // if the next instruction is syscall, rip-2
          // if the next instruction is breakpoint, rip-1
          if (next_si_code == 128) {
            printf("(hb) (bp-SYS) BP\n");
            regs.rip -= 1;
            // 6. set the regs.rip back
            ptrace(PTRACE_SETREGS, pid, 0, &regs);
          }
          else if (next_si_code == 133) {
            printf("(hb) (bp-SYS) SYSCALL\n");
            regs.rip -= 2;
            // 6. set the regs.rip back
            ptrace(PTRACE_SETREGS, pid, 0, &regs);
          }

          // 7. need to check if the next rip is equal to the breakpoint address
          // if equal, need to handle the breakpoint again
          regs = get_regs(pid);
          printf("(nb) (bp-SYS) regs.rip: %llx\n", regs.rip);
          if (regs.rip == breakpoints[i+1].addr && breakpoints[i+1].active && (i+1) < num_breakpoints) {
            regs.rip+=1;
            handle_breakpoints(pid, regs, "syscall");
          }
        }
        return;
      }
    
      // rip and bp arent the same addr, means hit the breakpoint
      else {
        printf("** hit a breakpoint at 0x%lx.\n", breakpoints[i].addr);
        bp_again = 1;
        which_bp = i;

        // 1. PTRACE_POKETEXT: restore the original instruction
        // might original_code cover the next breakpoint(s) -> need to get the newest data in the memory.
        long bp_data = ptrace(PTRACE_PEEKTEXT, pid, (void *)breakpoints[i].addr, NULL);
        bp_data = (bp_data & 0xffffffffffffff00);
        long origiinal_data = breakpoints[i].original_code & 0x00000000000000ff;
        long handle_data = bp_data | origiinal_data;

        if (ptrace(PTRACE_POKETEXT, pid, (void *)breakpoints[i].addr, (void *)handle_data) < 0){
          errquit("ptrace POKETEXT@handle_breakpoints");
        }
        // NEW: need to set bp again after single step, or might stop again.



        // 2. PTRACE_SINGLESTEP: single step
        // ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);

        // 3 . waitpid
        // waitpid(pid, NULL, 0);

        // 4. PTRACE_GETREGS: get the regs
        // regs = get_regs(pid);
        // printf("(4) -----> regs.rip: %llx\n", regs.rip);
        // ptrace(PTRACE_SETREGS, pid, 0, &regs);


        // 5. PTRACE_POKETEXT: set the breakpoint again
        // if (ptrace(PTRACE_POKETEXT, pid, (void *)breakpoints[i].addr, (void *)(breakpoints[i].int3_code)) == -1) {
          // errquit("ptrace POKETEXT@handle_breakpoints");
        // }


        // regs.rip = breakpoints[i].addr;
        // regs.rdx = regs.rax;
        // if (ptrace(PTRACE_SETREGS, pid, 0, &regs) < 0) {
        //   errquit("ptrace SETREGS@handle_breakpoints");
        //   // perror("ptrace SETREGS");
        //   // return;
        // }

        // single step
        // if (ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL) < 0) {
        //   perror("ptrace SINGLESTEP");
        //   return;
        // }

        // int status;
        // waitpid(pid, &status, 0);

        // set the breakpoint again
        // But in the spec, the program should not stop at the same breakpoint twice!!!
        // if (ptrace(PTRACE_POKETEXT, pid, (void *)breakpoints[i].addr, (void *)((breakpoints[i].original_code & 0xffffffffffffff00) | INT3)) == -1) {
        //   perror("ptrace POKETEXT");
        // }

        return;
      }
    }

    // 該 bp 沒有 hit 也不是在 rip 的位址
    else {
      continue;
    }
  }  // for loop end.
}


Elf64_Shdr get_section(FILE *fp, Elf64_Ehdr *ehdr, char *section_name) {
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


int readelf(char *program_name, uint64_t *end_addr) {
  FILE* fp = fopen(program_name, "rb");
  if (fp == NULL) {
    errquit("fopen@readelf");
  }

	Elf64_Ehdr ehdr;
	fread(&ehdr, sizeof(Elf64_Ehdr), 1, fp);
	Elf64_Shdr text = get_section(fp, &ehdr, ".text");
  // read text session
  uint8_t *text_section = (uint8_t *)malloc(text.sh_size);
  fseek(fp, text.sh_offset, SEEK_SET);
  // printf("text.sh_size: %ld\n", text.sh_size);
  // printf("text.sh_addr: 0x%0lx\n", text.sh_addr);
  // printf("text.sh_offset: %ld\n", text.sh_offset);

  *end_addr = text.sh_addr + text.sh_size - 2;
  // printf("end_addr: 0x%0lx\n", *end_addr);
  fread(text_section, text.sh_size, 1, fp);
  // disassemble the text section
  csh handle;
  cs_insn *insn;
  if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK) {
    fprintf(stderr, "Failed to initialize capstone\n");
    return 0;
  }
  size_t count;
  count = cs_disasm(handle, text_section, text.sh_size, text.sh_addr, 0, &insn);
  // printf("count: %ld\n", count);
  fclose(fp);
  return count;
}


char* print_prompt() {
  printf("(sdb) ");
  fflush(stdout);

  static char command[BUFFER_SIZE];
  if (fgets(command, BUFFER_SIZE, stdin) == NULL) {
    return NULL;
  }
  return command;
}


void disassemble(pid_t child_pid, uint64_t address) {
  csh csh_handle = 0;
  cs_insn *insn;
  size_t count;
  unsigned char code[16];
  uint64_t current_intruction_addr = address;

  // print the current instruction address
  printf("(disassemble) current_address (next instruction): 0x%lx\n", current_intruction_addr);

  if (cs_open(CS_ARCH_X86, CS_MODE_64, &csh_handle) != CS_ERR_OK) {
    fprintf(stderr, "Failed to initialize capstone\n");
    return;
  }

  // code equals to the first 5 instructions in the program.
  for (int NUM = 0; NUM < NUM_INTRUCTIONS; NUM++) {
    if (current_intruction_addr > end_address) {
      printf("** the address is out of the range of the text section.\n");
      break;
    }
    // get the line of program code
    memset(code, 0, sizeof(code));
    for (long unsigned int i = 0; i < sizeof(code); i += sizeof(long)) {
      long word = ptrace(PTRACE_PEEKTEXT, child_pid, current_intruction_addr + i, NULL);
      // if word is breakpoint, need to restore original data to print
      for (int j = 0; j < num_breakpoints; j++) {
        if (breakpoints[j].addr == current_intruction_addr + i && breakpoints[j].active) {
          word = breakpoints[j].original_code;
          break;
        }
      }
      if (word == -1 && errno != 0) {
          cs_close(&csh_handle);
          errquit("ptrace PEEKTEXT@disassemble");
      }
      memcpy(code + i, &word, sizeof(word));
    }

    // disassemble the code
    count = cs_disasm(csh_handle, code, sizeof(code), current_intruction_addr, 1, &insn);
    if (count > 0) {
      for (size_t i = 0; i < count; i++) {
        char instruction_BYTES[256] = "";
        for (size_t j = 0; j < insn[i].size; j++) { 
          // combine the bytes to form the instruction
          sprintf(instruction_BYTES + strlen(instruction_BYTES), "%02x", insn[i].bytes[j]);
          if (j < (size_t) insn[i].size - 1) { // insert a space between the bytes execpt the last byte
            sprintf(instruction_BYTES + strlen(instruction_BYTES), " ");
          }
        }
        printf("\t%0lx: %-24s %-8s %s\n", 
                insn[i].address, // print the address of the instruction
                instruction_BYTES, // raw instructions in a grouping of 1 byte
                insn[i].mnemonic, // mnemonic
                insn[i].op_str); // operands
        current_intruction_addr += insn[i].size;
      }
      cs_free(insn, count);
    } else {
      printf("** Failed to disassemble code at entry point\n");
    }
  }
  cs_close(&csh_handle);
}


pid_t load_program(char *program_path) {
  total_count = readelf(program_path, &end_address);
  pid_t child_pid = fork();
  if (child_pid == 0) {
    // Child process
    if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0) {
      errquit("ptrace@load_program");
      // perror("ptrace");
      // exit(1);
    }
    if (execl(program_path, program_path, NULL)) {
      errquit("execl@load_program");
      // perror("execl");
      // exit(1);
    }
  } 
  else if (child_pid > 0) {
    // Parent process
    int status;
    waitpid(child_pid, &status, 0);
    if (WIFEXITED(status)) {
      printf("Child process exited with status %d\n", WEXITSTATUS(status));
      exit(0);
    }
    // obtaine the entry point of the program
    ptrace(PTRACE_SETOPTIONS, child_pid, 0, PTRACE_O_EXITKILL);
    struct user_regs_struct regs = get_regs(child_pid);
    printf("** program '%s' loaded. entry point 0x%llx.\n", program_path, regs.rip);

    // reassemble the 5 instructions at the entry point
    disassemble(child_pid, regs.rip);
  }
  // if fork failed
  else {
    errquit("fork@load_program");
  }
  return child_pid;
}


void print_regs(pid_t child_pid){
  // ptrace(PTRACE_SETOPTIONS, child_pid, 0, PTRACE_O_EXITKILL);
  struct user_regs_struct regs = get_regs(child_pid);
  printf("$rax 0x%016llx\t\t", regs.rax);
  printf("$rbx 0x%016llx\t\t", regs.rbx);
  printf("$rcx 0x%016llx\n", regs.rcx);
  printf("$rdx 0x%016llx\t\t", regs.rdx);
  printf("$rsi 0x%016llx\t\t", regs.rsi);
  printf("$rdi 0x%016llx\n", regs.rdi);
  printf("$rbp 0x%016llx\t\t", regs.rbp);
  printf("$rsp 0x%016llx\t\t", regs.rsp);
  printf("$r8  0x%016llx\n", regs.r8);
  printf("$r9  0x%016llx\t\t", regs.r9);
  printf("$r10 0x%016llx\t\t", regs.r10);
  printf("$r11 0x%016llx\n", regs.r11);
  printf("$r12 0x%016llx\t\t", regs.r12);
  printf("$r13 0x%016llx\t\t", regs.r13);
  printf("$r14 0x%016llx\n", regs.r14);
  printf("$r15 0x%016llx\t\t", regs.r15);
  printf("$rip 0x%016llx\t\t", regs.rip);
  printf("$eflags: 0x%016llx\n", regs.eflags);
}


void command_SI(pid_t pid) {
  int status;
  printf("(SI) bp_again: %d\n", bp_again);
  if (bp_again) {
    // 先執行一次 singlestep 再 set breakpoint 回去
    // TODO: 連續兩個 breakpoint 會有問題

    struct user_regs_struct regs = get_regs(pid);
    printf("(SI) (1) regs.rip: %llx\n", regs.rip);
    
    ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
    waitpid(pid, &status, 0);

    regs = get_regs(pid);
    printf("(SI) (1-1) regs.rip: %llx\n", regs.rip);

    // 1. 先拿可能後來有被設定中斷點的新資料出來
    long bp_data = ptrace(PTRACE_PEEKTEXT, pid, (void *)breakpoints[which_bp].addr, NULL);
    bp_data = (bp_data & 0xffffffffffffff00);
    long origiinal_data = breakpoints[which_bp].int3_code & 0x00000000000000ff;
    long handle_data = bp_data | origiinal_data;

    // 2. 再放回去原本的中斷點
    if (ptrace(PTRACE_POKETEXT, pid, (void *)breakpoints[which_bp].addr, (void *)(handle_data)) == -1) {
      errquit("ptrace POKETEXT@handle_breakpoints");
    }

    regs = get_regs(pid);
    printf("(SI) (2) regs.rip: %llx\n", regs.rip);

    // check the memory data is correct.
    long data = ptrace(PTRACE_PEEKTEXT, pid, (void *)breakpoints[which_bp].addr, NULL);
    printf("(SI) (3) data: 0x%lx\n", data);

    // 3. 如果 (1) = (1-1)，表示踩到 bp 需要再執行一次 single step
    if (regs.rip == breakpoints[which_bp].addr) {
      ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
      waitpid(pid, &status, 0); // wait for the child process to stop
      regs = get_regs(pid);
      printf("(SI) (4) regs.rip: %llx\n", regs.rip);
    }
  }

  // 如果不是從 bp 踩下去的，需要再執行一次 single step
  if (!bp_again) {
    printf("(SI) single step\n");
    ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
    waitpid(pid, &status, 0); // wait for the child process to stop
    // bp_again = 0;
  }
  else {
    bp_again = 0;
  }
  // bp_again = 0;
  
  
  if (WIFEXITED(status)) {
    printf("** the target program terminated. -> status: %d\n", WIFEXITED(status));
    exit(0);
  }

  // 做完 single step 後，確認是否有 hit breakpoint
  if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
    siginfo_t siginfo;
    memset(&siginfo, 0, sizeof(siginfo));
    ptrace(PTRACE_GETSIGINFO, pid, NULL, &siginfo);
    printf("(SI) siginfo.si_code: %d\n", siginfo.si_code);
    struct user_regs_struct regs = get_regs(pid);
    printf("(SI) regs.rip: %llx\n", regs.rip);

    // (a) bp 設定在 rip 的位址時，會執行完 bp 後停下，因此 rip 會在 bp 位址的下一個
    // (b) 不是的話，則為正常的 bp 位址，rip 會剛好停在 bp 位址
    // (c) 所以如果 bp_addr == 1，表示是需要出現 hit 的 bp，需要假設已踩到 bp 位址
    int bp_addr = 0;
    // 確認目前的 rip 是否有被設定成 bp 的位址：有的話不能算 hit
    for (int i = 0; i < num_breakpoints; i++) {
      if (breakpoints[i].addr == regs.rip) {
        bp_addr = 1;
        break;
      }
    }

    // 如果是一般的 single step，不需要去處理 bp
    if ((siginfo.si_code != SI_TRAP && siginfo.si_code != CLD_EXITED) || bp_addr) { // check bp isnt hit, just single step
      printf("(SI -> hb) regs.rip: %llx\n", regs.rip);
      if (bp_addr) { // 不是在 rip 的 bp，需要假設已經踩到 bp 位址，所以位址要加一，再讓 hb function 扣一
        regs.rip += 1; // assume the instruction at bp is executed.
      }
      handle_breakpoints(pid, regs, "si");
    }
  }

  struct user_regs_struct regs = get_regs(pid);
  disassemble(pid, regs.rip);
}


void command_CONT(pid_t pid) {
  /* You can only use two ptrace (PTRACE_SINGLE_STEP) and two int3 at most when encounter breakpoint. */
  int status;
  printf("(CONT) bp_again: %d\n", bp_again);
  if (bp_again) {
    // 先執行一次 singlestep 再 set breakpoint 回去
    struct user_regs_struct regs = get_regs(pid);
    printf("(CONT) (1) regs.rip: %llx\n", regs.rip);
    ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
    waitpid(pid, &status, 0);
    regs = get_regs(pid);
    printf("(CONT) (1-1) regs.rip: %llx\n", regs.rip);


    // 1. 先拿可能後來有被設定中斷點的新資料出來
    long bp_data = ptrace(PTRACE_PEEKTEXT, pid, (void *)breakpoints[which_bp].addr, NULL);
    bp_data = (bp_data & 0xffffffffffffff00);
    long origiinal_data = breakpoints[which_bp].int3_code & 0x00000000000000ff;
    long handle_data = bp_data | origiinal_data;

    // 2. 再放回去原本的中斷點
    if (ptrace(PTRACE_POKETEXT, pid, (void *)breakpoints[which_bp].addr, (void *)(handle_data)) == -1) {
      errquit("ptrace POKETEXT@handle_breakpoints");
    }
    
    regs = get_regs(pid);
    printf("(CONT) (2) regs.rip: %llx\n", regs.rip);

    // check the memory data is correct.
    long data = ptrace(PTRACE_PEEKTEXT, pid, (void *)breakpoints[which_bp].addr, NULL);
    printf("(CONT) data: 0x%lx\n", data);

    // 3. 如果 (1) = (1-1)，表示踩到 bp 需要再執行一次 single step
    if (regs.rip == breakpoints[which_bp].addr) {
      ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
      waitpid(pid, &status, 0); // wait for the child process to stop
      regs = get_regs(pid);
      printf("(SI) (4) regs.rip: %llx\n", regs.rip);
    }
    bp_again = 0;
  }


  // if (!bp_again) {
      // ptrace(PTRACE_CONT, pid, NULL, NULL);
      // waitpid(pid, &status, 0);
    // bp_again = 0;
  // }
    ptrace(PTRACE_CONT, pid, NULL, NULL);
    waitpid(pid, &status, 0);


  if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
    siginfo_t siginfo;
    memset(&siginfo, 0, sizeof(siginfo));
    ptrace(PTRACE_GETSIGINFO, pid, NULL, &siginfo);
    printf("(CONT) siginfo.si_code: %d\n", siginfo.si_code);


    struct user_regs_struct regs;
    regs = get_regs(pid);
    printf("(CONT-main) regs.rip: %llx\n", regs.rip);
    handle_breakpoints(pid, regs, "cont");
  }

  // if the child process is terminated
  // 如果 tracee 還沒結束就輸出 disassemble code
  if (WIFEXITED(status)) {
    printf("** the target program terminate. -> status: %d\n", WIFEXITED(status));
    exit(0);
  }
  else {
    if (errno == ESRCH) { // the child pid is terminated.
      printf("** the target program terminate. -> status: %d\n", WIFEXITED(status));
      exit(0);
    }
    else {
      struct user_regs_struct regs = get_regs(pid);
      disassemble(pid, regs.rip);
    }
  }
}


// TODO 檢查syscall 的執行條件：
  // 如果中斷點設在 syscall 的下一個指令
  // 如果中斷點設在 rip 會踩下去，但下次停應該會是下個 syscall 的進入
  // 中斷點後下一個停下來是因為中斷點還是 syscall

// TODO (v) 如果 enter 之後不是繼續 syscall，直到下次 syscall 時要回到 enter 才對
void command_syscall(pid_t pid) {
  int status;
  printf("(SYS) bp_again: %d\n", bp_again);
  if (bp_again) {
    struct user_regs_struct regs = get_regs(pid);
    printf("(SYS) (1) regs.rip: %llx\n", regs.rip);

    // 先執行一次 singlestep 再 set breakpoint 回去
    ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
    waitpid(pid, &status, 0);

    // 1. 先拿可能後來有被設定中斷點的新資料出來
    long bp_data = ptrace(PTRACE_PEEKTEXT, pid, (void *)breakpoints[which_bp].addr, NULL);
    bp_data = (bp_data & 0xffffffffffffff00);
    long origiinal_data = breakpoints[which_bp].int3_code & 0x00000000000000ff;
    long handle_data = bp_data | origiinal_data;

    // 2. 再放回去原本的中斷點
    if (ptrace(PTRACE_POKETEXT, pid, (void *)breakpoints[which_bp].addr, (void *)(handle_data)) == -1) {
      errquit("ptrace POKETEXT@handle_breakpoints");
    }

    regs = get_regs(pid);
    printf("(SYS) (2) regs.rip: %llx\n", regs.rip);

    // check the memory data is correct.
    long data = ptrace(PTRACE_PEEKTEXT, pid, (void *)breakpoints[which_bp].addr, NULL);
    printf("(SYS) data: 0x%lx\n", data);

    // 3. 如果 (1) = (1-1)，表示踩到 bp 需要再執行一次 single step
    if (regs.rip == breakpoints[which_bp].addr) {
      ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL);
      waitpid(pid, &status, 0); // wait for the child process to stop
      regs = get_regs(pid);
      printf("(SYS) (4) regs.rip: %llx\n", regs.rip);
    }

    bp_again = 0;
  }


  // start to judge the syscall
  struct user_regs_struct regs;
  ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_EXITKILL|PTRACE_O_TRACESYSGOOD); // ptrace sig has 0x80 bit marked
  if (ptrace(PTRACE_SYSCALL, pid, NULL, NULL) != 0) {
    errquit("ptrace SYSCALL@command_syscall");
  }
  if (waitpid(pid, &status, 0) < 0) {
    errquit("waitpid@command_syscall");
  }


  // 如果停下時，需要判斷是因為 syscall 還是遇到 bp
  // 1. 0x80 is the bit for syscall
  if (WIFSTOPPED(status) && WSTOPSIG(status) & 0x80) {
    siginfo_t siginfo;
    memset(&siginfo, 0, sizeof(siginfo));
    ptrace(PTRACE_GETSIGINFO, pid, NULL, &siginfo);
    next_si_code = siginfo.si_code;
    printf("(SYS 0x80) siginfo.si_code: %d\n", siginfo.si_code);

    regs = get_regs(pid);
    // 現在應該要是離開，但是因為中間插入執行 SI 或是 CONT 等，導致下一次輸出是另外一個 syscall 的進入
    // 需要調整 enter 的狀態值
    if (!enter && pre_orig_rax != regs.orig_rax) {
      enter ^= 0x01;
    }

    if (enter) {
      printf("** enter a syscall(%lld) at 0x%llx.\n", regs.orig_rax, regs.rip - 2);
    }
    else {
      printf("** leave a syscall(%lld) = %lld at 0x%llx.\n", regs.orig_rax, regs.rax, regs.rip - 2);
    }
    enter ^= 0x01;
    pre_orig_rax = regs.orig_rax;
    disassemble(pid, regs.rip - 2); // syscall instruction is 2 bytes
  }

  // 2. if hit breakpoints
  else if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
    siginfo_t siginfo;
    memset(&siginfo, 0, sizeof(siginfo));
    ptrace(PTRACE_GETSIGINFO, pid, NULL, &siginfo);
    next_si_code = siginfo.si_code;
    printf("(SYS bp) siginfo.si_code: %d\n", siginfo.si_code);

    regs = get_regs(pid);
    printf("(SYS-main) regs.rip: %llx\n", regs.rip);
    handle_breakpoints(pid, regs, "syscall");

    // after handle the bp，表示有踩到 bp，需要再繼續執行 trace
    if (bp_again) {
      struct user_regs_struct regs = get_regs(pid);
      disassemble(pid, regs.rip);
    }
  }

  // if the child process is terminated
  if (errno == ESRCH || WIFEXITED(status)) {
    printf("** the target program terminated. -> status: %d\n", WIFEXITED(status));
    exit(0);
  }
  // regs.orig_rax == 60 is the end of the program.
}


void patch_memory(pid_t pid, uint64_t addr, uint64_t value, int len_byte) {
  printf("** patch memory at 0x%lx.\n", addr);
  // divide the value into bytes
  uint64_t *code_byte = (uint64_t *)malloc(len_byte);
  for (int i = 0; i < len_byte; i++) {
    code_byte[i] = (value >> (i * 8)) & 0x00000000000000ff;  // 0xffffffffffffffff
    // printf("code_byte[%d]: %lx\n", i, code_byte[i]);
  }


  for (int i = 0; i < len_byte; i++) {
    // printf("i: %d\n", i);
    // printf("addr + i: %lx\n", addr + i);
    long data = ptrace(PTRACE_PEEKTEXT, pid, (void *)(addr + i), NULL);
    if (data == -1 && errno != 0) {
      errquit("ptrace PEEKTEXT@patch_memory");
    }
    // printf("data: 0x%lx\n", data);

    // patch (addr + i) with new data
    long new_data = (data & 0xffffffffffffff00) | code_byte[i];
    if (ptrace(PTRACE_POKETEXT, pid, (void *)(addr + i), (void *)(new_data)) < 0) {
      errquit("ptrace POKETEXT@patch_memory");
    }

    // check if patch memory is at the breakpoint
    for (int j = 0; j < num_breakpoints; j++) {
      if (breakpoints[j].addr == addr + i && breakpoints[j].active) {
        breakpoints[j].original_code = new_data;
        // hold the breakpoint if the breakpoint is at the patch memory
        long data = ptrace(PTRACE_PEEKTEXT, pid, (void *)breakpoints[j].addr, NULL); // get a word from a memory address of the tracee.
        if (data == -1 && errno != 0) {
          errquit("ptrace PEEKTEXT@patch_memory");
        }

        long data_trap = (data & 0xffffffffffffff00) | INT3;
        if (ptrace(PTRACE_POKETEXT, pid, (void *)breakpoints[j].addr, (void *)data_trap) < 0) {
          errquit("ptrace POKETEXT@patch_memory");
        }
        break;
      }
    }
    // check after patch memory's data
    long check_data = ptrace(PTRACE_PEEKTEXT, pid, (void *)(addr + i), NULL);
    printf("check_data: 0x%lx\n", check_data);
  }
}


int main(int argc, char *argv[]) {
  // char* command[BUFFER_SIZE];
  static char program_path[BUFFER_SIZE] = "";
  pid_t child_pid;

  // printf("0x12345678&0xffffffffffffff00: %lx\n", 0x12345678&0xffffffffffffff00);
  // DEAL: In the first time to run ./sdb. if the program is loaded from the command line
  if (argc == 2) {
    strncpy(program_path, argv[1], sizeof(program_path));
    child_pid = load_program(program_path);
  } else { // if the program is not loaded from the command line
    int loaded = 0;
    while (!loaded) {
      
      char* command = print_prompt();
      if (strncmp(command, "load", 4) == 0) {
        sscanf(command + 5, "%s", program_path); // skip the command "load " and get the program path(name) 
        child_pid = load_program(program_path);
        loaded = 1;
      }
      if (program_path[0] == '\0'){
        printf("** please load a program first.\n");
      }
    }
  }

  printf("total instruction counts: %ld\n", total_count);
  while (true) {
    char* command = print_prompt();
    if (strncmp(command, "q", 1) == 0) {
      exit(0);
    }
    if (strncmp(command ,"readelf", 7) == 0) {
      readelf(program_path, &end_address);
    }


    if (strncmp(command, "si", 2) == 0) {
      command_SI(child_pid);
    }


    else if (strncmp(command, "cont", 4) == 0) {
      command_CONT(child_pid);
    }


    else if (strncmp(command, "info reg", 8) == 0) {
      print_regs(child_pid);
    }


    else if (strncmp(command, "break", 5) == 0) {
      uint64_t addr;
      sscanf(command + 6, "%lx", &addr);
      set_breakpoints(child_pid, addr);
    }


    else if (strncmp(command, "info break", 10) == 0) {
      print_breakpoints();
    }


    else if (strncmp(command, "delete", 6) == 0) {
      int id;
      sscanf(command + 7, "%d", &id);
      delete_breakpoint(child_pid, id);
    }


    else if (strncmp(command, "syscall", 7) == 0) {
      // The program execution should break at every system call instruction unless it hits a breakpoint.
      // The program should print the system call number and the return value of the system call.
      command_syscall(child_pid);
      // syscall_trace(child_pid);
    }

    // patch [hex address] [hex value] [len]
    else if (strncmp(command, "patch", 5) == 0) {
      uint64_t addr;
      uint64_t value;
      int len_byte;
      sscanf(command + 6, "%lx %lx %d", &addr, &value, &len_byte);
      // printf("(main) addr: 0x%lx, value: 0x%lx, len_byte: %d\n", addr, value, len_byte);
      patch_memory(child_pid, addr, value, len_byte);
      // for (int i = 0; i < len; i++) {
      //   long data = ptrace(PTRACE_PEEKTEXT, child_pid, (void *)(addr + i), NULL);
      //   if (data == -1 && errno != 0) {
      //     perror("ptrace PEEKTEXT");
      //     return 0;
      //   }
      //   if (ptrace(PTRACE_POKETEXT, child_pid, (void *)(addr + i), (void *)value) < 0) {
      //     perror("ptrace POKETEXT");
      //     return 0;
      //   }
      // }
    }

    else if (strncmp(command, "help", 4) == 0) {
      printf("sdb commands:\n");
      printf("load [program] - load a program\n");
      printf("readelf - read the elf file\n");
      printf("si - step into\n");
      printf("cont - continue\n");
      printf("info reg - print the registers\n");
      printf("break [address] - set a breakpoint\n");
      printf("info break - print the breakpoints\n");
      printf("delete [breakpoint id] - delete a breakpoint\n");
      printf("syscall - trace the system call\n");
      printf("patch [hex address] [hex value] [len byte] - patch the memory\n");
      printf("q - quit\n");
    }


    else {
      printf("** unknown command\n");
    }
  }



  return 0;
}