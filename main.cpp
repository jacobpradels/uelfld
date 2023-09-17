#include <elf.h>
#include <fstream>
#include <iostream>
#include <string>
#include <string.h>
#include <vector>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv) {
// 

  int fd = open("elf", O_RDONLY);  // Open for reading only
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

  lseek(fd, elf_header.e_phoff, SEEK_SET);
  for (int i = 0; i < elf_header.e_phnum; i++) {
    read(fd, &program_headers[i], sizeof(Elf64_Phdr));
  }

  // Load file descriptor of interpreter
  int interp_fd;
  for (int i = 0; i < elf_header.e_phnum; i++) {
    if (program_headers[i].p_type == 3) {
      char interp[program_headers[i].p_filesz];
      lseek(fd, program_headers[i].p_offset, SEEK_SET);
      read(fd, &interp, program_headers[i].p_filesz);
      interp_fd = open(interp, O_RDONLY);
      std::cout<<interp<<"\n";
    }
  }
  if (interp_fd == -1) {
    std::cerr << "Error opening file." << std::endl;
    return 1;
  }

  // Interpreter ELF Header
  Elf64_Ehdr interp_elf_hdr;
  if (read(interp_fd, &interp_elf_hdr, sizeof(Elf64_Ehdr)) == -1) {
        std::cerr << "Error reading file." << std::endl;
        return 1;
  }

  // Load Interpreter LOAD Segments into memory
  void *loaded_segments[interp_elf_hdr.e_phnum];
  for (int i = 0; i < interp_elf_hdr.e_phnum; i++) {
    auto current_prog_header = program_headers[i];
    if (current_prog_header.p_type != 1) {
        continue;
    }
    std::cout << "type " << current_prog_header.p_type << " range=["<< current_prog_header.p_offset <<"-"<< current_prog_header.p_offset+current_prog_header.p_memsz<<"]\n";
    int prot = 0;

    if (current_prog_header.p_flags & PF_R) {
        prot |= PROT_READ;
    }
    if (current_prog_header.p_flags & PF_W) {
        prot |= PROT_WRITE;
    }
    if (current_prog_header.p_flags & PF_X) {
        prot |= PROT_EXEC;
    }

    off_t aligned_offset = current_prog_header.p_offset & ~(sysconf(_SC_PAGESIZE) - 1);
    size_t adjustment = current_prog_header.p_offset - aligned_offset;
    size_t total_size = current_prog_header.p_memsz + adjustment;

    loaded_segments[i] = mmap(nullptr, total_size, prot, MAP_PRIVATE, interp_fd, aligned_offset);
    std::cout<<prot<<"\n";
    if (loaded_segments[i] == (void*)-1) {
        std::cerr<<"Failed to allocate memory\n";
        std::cerr << "Error code: " << errno << std::endl;
        continue;
    }
    std::cout<<i<<" "<<loaded_segments[i]<<"\n";
    
  }

  // Set up the stack
  size_t stack_size = 8 * 1024 * 1024;  // 8 MB, for example
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

  // Populate the stack
  char* argve[] = {(char*)"./elf", nullptr};
  char* envp[] = {(char*)"PATH=/bin:/usr/bin", nullptr};

  void** stack_top = (void**)((char*)stack_base + stack_size);

  // push null terminator for auxv (if you're using it)
  *--stack_top = nullptr;

  // push envp strings backwards
  int i;
  for (i = 0; envp[i] != nullptr; i++);
  for (; i >= 0; i--) {
      *--stack_top = envp[i];
  }

  // push argv strings backwards
  for (i = 0; argve[i] != nullptr; i++);
  for (; i >= 0; i--) {
      *--stack_top = argve[i];
  }

  // push argc
  *--stack_top = (void*)3;  // example: 3 arguments, adjust accordingly

  __asm__ (
    "mov %[stack_top], %%rsp\n\t"
    :
    : [stack_top] "r" (stack_top)
  );


  void* entry_point = loaded_segments[2] - interp_elf_hdr.e_entry;
  std::cout<<std::hex<<"entry_point: "<<entry_point<<"\n";
  std::cout<<"stack_base: "<<stack_base<<"\n";
  std::cout<<"stack_top: "<<stack_top<<"\n";

  // Convert the address to a function pointer of the appropriate type
  void (*entry_function)() = (void (*)())entry_point;
  // Execute the function
  entry_function();
  // Close file
  close(fd);

  // Clean up memory
  // for (int i = 0; i < elf_header.e_phnum; i++) {
  //   auto current_prog_header = program_headers[i];
  //   if (current_prog_header.p_type != 1) {
  //       continue;
  //   }
  //   // munmap(reinterpret_cast<void*>(current_prog_header.p_vaddr), current_prog_header.p_memsz);
  // }
  return 0;
}