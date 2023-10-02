#include <elf.h>
#include <fstream>
#include <iostream>
#include <string>
#include <string.h>
#include <vector>
#include <sys/mman.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <algorithm>

#include "elf_loader.h"

void get_program_headers(int fd, Elf64_Ehdr header, Elf64_Phdr (&pheader)[]) {
  lseek(fd, header.e_phoff, SEEK_SET);
  for (int i = 0; i < header.e_phnum; i++) {
    read(fd, &pheader[i], sizeof(Elf64_Phdr));
  }
}



int main(int argc, char **argv, char **envp) {

  int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        std::cerr << "Error opening file." << std::endl;
        return 1;
    }

  // Retrieve ELF Header
  Elf64_Ehdr elf_header;
  if (read(fd, &elf_header, sizeof(Elf64_Ehdr)) == -1) {
        std::cerr << "Error reading file." << std::endl;
        return 1;
  }

  // Retrieve Program Headers
  Elf64_Phdr program_headers[elf_header.e_phnum];
  get_program_headers(fd, elf_header, program_headers);

  // Load file descriptor of interpreter
  int interp_fd;
  int interpreter_index = -1;
  for (int i = 0; i < elf_header.e_phnum; i++) {
    if (program_headers[i].p_type == 3) {
      interpreter_index = i;
    }
  }
  Elf64_Ehdr interp_elf_hdr;
  void* base_interp;
  char interp[program_headers[interpreter_index].p_filesz];
  if (interpreter_index != -1) {

    // Read value of interpeter path
    lseek(fd, program_headers[interpreter_index].p_offset, SEEK_SET);
    read(fd, &interp, program_headers[interpreter_index].p_filesz);
    // Read Interpreter ELF file
    interp_fd = open(interp, O_RDONLY);
    std::cout<<interp<<"\n";
    if (interp_fd == -1) {
      std::cerr << "Error opening file." << std::endl;
      return 1;
    }

    // Interpreter ELF Header
    if (read(interp_fd, &interp_elf_hdr, sizeof(Elf64_Ehdr)) == -1) {
          std::cerr << "Error reading file." << std::endl;
          return 1;
    }
  }
  void *loaded_segments[interp_elf_hdr.e_phnum];// I was here
  Elf64_Phdr interp_program_headers[interp_elf_hdr.e_phnum];
  if (interpreter_index != -1) {
    // Retrieve Interpreter Program Headers
    get_program_headers(interp_fd, interp_elf_hdr, interp_program_headers);

    // Load Interpreter LOAD Segments into memory
    base_interp = mmap_elf_segments(interp_elf_hdr, interp_program_headers, interp_fd, loaded_segments);
  }

  void *loaded_elf_segments[elf_header.e_phnum];
  void* base_elf = mmap_elf_segments(elf_header, program_headers, fd, loaded_elf_segments);

  // Set up the stack
  size_t stack_size = 32 * 1024 * 1024;
  void* stack_base = mmap(
      nullptr, 
      stack_size, 
      PROT_READ | PROT_WRITE, 
      MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN, 
      -1, 
      0
  );

  if (stack_base == MAP_FAILED) {
      std::cerr<<"Failed to allocate memory\n";
      std::cerr << "Error code: " << errno << std::endl;
  }
  void* entry_point;
  if (interpreter_index != -1) {
    entry_point = reinterpret_cast<void*>(base_interp + interp_elf_hdr.e_entry);
  } else {
    entry_point = reinterpret_cast<void*>(base_elf + elf_header.e_entry - 0x400000);
  }
  std::cout<<std::hex<<entry_point<<"\n";
  void** stack_top = (void**)((char*)stack_base + stack_size);

  *--stack_top = 0;
  // push envp strings backwards
  std::vector<void*> envp_pointers;
  stack_top--;
  for (int i = 0; envp[i] != nullptr; i++) {
    if (strlen(envp[i]) < sysconf(_SC_PAGESIZE) * 32) {
      envp_pointers.push_back(std::memcpy(stack_top, envp[i], strlen(envp[i])));
      stack_top -= strlen(envp[i]) + 1;
    }
  }

  *--stack_top = 0;
  stack_top--;
  // push argv strings backwards
  std::vector<void*> argv_pointers;
  argv[0] = interp;
  for (int i = 0; i < argc; i++) {
    argv_pointers.push_back(std::memcpy(stack_top, argv[i], strlen(argv[i])));
    stack_top -= strlen(argv[i]) + 1;
  }

  *--stack_top = 0;

  Elf64_auxv_t at_entry = { AT_ENTRY,  {.a_val = reinterpret_cast<uint64_t>(entry_point)}};
  std::memcpy(stack_top, &at_entry, sizeof(Elf64_auxv_t));
  stack_top -= sizeof(Elf64_auxv_t) / sizeof(void*);

  Elf64_auxv_t at_base = { AT_BASE,  {.a_val = (uint64_t)loaded_segments[1] }};
  std::memcpy(stack_top, &at_base, sizeof(Elf64_auxv_t));
  std::cout<<"at base:"<<std::hex<<loaded_segments[1]<<"\n";
  stack_top -= sizeof(Elf64_auxv_t) / sizeof(void*);

  Elf64_auxv_t at_pagesz = { AT_PAGESZ,  {.a_val = sysconf(_SC_PAGESIZE) }};
  std::memcpy(stack_top, &at_pagesz, sizeof(Elf64_auxv_t));
  stack_top -= sizeof(Elf64_auxv_t) / sizeof(void*);

  Elf64_auxv_t at_phnum = { AT_PHNUM,  {.a_val = interp_elf_hdr.e_phnum }};
  std::memcpy(stack_top, &at_phnum, sizeof(Elf64_auxv_t));
  stack_top -= sizeof(Elf64_auxv_t) / sizeof(void*);

  Elf64_auxv_t at_phent = { AT_PHENT, {.a_val = (uint64_t)elf_header.e_phentsize}};
  std::memcpy(stack_top, &at_phent, sizeof(Elf64_auxv_t));
  stack_top -= sizeof(Elf64_auxv_t) / sizeof(void*);

  Elf64_auxv_t at_phdr = { AT_PHDR, {.a_val = (uint64_t)&program_headers[0]} };
  std::memcpy(stack_top, &at_phdr, sizeof(Elf64_auxv_t));
  stack_top -= sizeof(Elf64_auxv_t) / sizeof(void*);

  // auxv
  Elf64_auxv_t at_null = { AT_NULL, {.a_val = NULL}};
  std::memcpy(stack_top, &at_null, sizeof(Elf64_auxv_t));
  stack_top -= sizeof(Elf64_auxv_t) / sizeof(void*);

  void* start_of_auxv = stack_top;
  std::reverse(envp_pointers.begin(), envp_pointers.end());
  for (auto e: envp_pointers) {
    *--stack_top = e;
  }
  std::cout<<"Start of envp"<<std::hex<<stack_top<<"\n";
  std::reverse(argv_pointers.begin(), argv_pointers.end());
  for (auto a: argv_pointers) {
    *--stack_top = a;
  }

  // push argc
  *(int*)--stack_top = argc;
  
  *--stack_top = entry_point;
  // Set up registers and jump to execution
  asm volatile(
      "mov %0, %%rdx\n"      // Load the pointer to the start of auxv into rdx
      "mov %1, %%rsp\n"      // Load the stack_top into rsp
      "xor %%rbp, %%rbp\n"
      "xor %%rbx, %%rbx\n"
      "xor %%r12, %%r12\n"
      "xor %%rdx, %%rdx\n"
      "xor %%rax, %%rax\n"
      "ret"
      :
      : "m"(start_of_auxv), "r"(stack_top)
      : "memory", "rdx"
  );



  close(fd);
  close(interp_fd);

  // Clean up memory
  for (int i = 0; i < elf_header.e_phnum; i++) {
    auto current_prog_header = program_headers[i];
    if (current_prog_header.p_type != 1) {
        continue;
    }
    munmap(reinterpret_cast<void*>(loaded_elf_segments[i]), current_prog_header.p_memsz);
  }
  for (int i = 0; i < elf_header.e_phnum; i++) {
    auto current_prog_header = interp_program_headers[i];
    if (current_prog_header.p_type != 1) {
        continue;
    }
    munmap(reinterpret_cast<void*>(loaded_segments[i]), current_prog_header.p_memsz);
  }
  return 0;
}

