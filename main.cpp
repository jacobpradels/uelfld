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
#include <cstring>
#include <algorithm>

#include "stack_setup.h"
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
  void* entry_point;
  if (interpreter_index != -1) {
    // Retrieve Interpreter Program Headers
    get_program_headers(interp_fd, interp_elf_hdr, interp_program_headers);

    // Load Interpreter LOAD Segments into memory
    base_interp = mmap_elf_segments(interp_elf_hdr, interp_program_headers, interp_fd, loaded_segments);
    entry_point = (void*)(base_interp + interp_elf_hdr.e_entry);
  }

  void *loaded_elf_segments[elf_header.e_phnum];
  void* base_elf = mmap_elf_segments(elf_header, program_headers, fd, loaded_elf_segments);
  if (interpreter_index == -1) {
    entry_point = (void*)base_elf + elf_header.e_entry;
  }
  void* stack_top = setup_stack(elf_header, base_elf, base_interp, argv, envp);

  asm volatile(
        "mov %0, %%rsp\n\t"
        "push %1\n\t"

        "xor %%rax, %%rax\n\t"
        "xor %%rbx, %%rbx\n\t"
        "xor %%rcx, %%rcx\n\t"
        "xor %%rdx, %%rdx\n\t"
        "xor %%rdi, %%rdi\n\t"
        "xor %%rsi, %%rsi\n\t"

        "xor %%r9, %%r9\n\t"
        "xor %%r10, %%r10\n\t"
        "xor %%r11, %%r11\n\t"
        "xor %%r12, %%r12\n\t"
        "xor %%r13, %%r13\n\t"
        "xor %%r14, %%r14\n\t"
        "xor %%r15, %%r15\n\t"

        "ret\n\t"
        :
        : "a"(stack_top), "b"(entry_point)
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

