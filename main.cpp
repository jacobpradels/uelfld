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

#define INIT_AND_JUMP(stack, rdi, rsi, rdx, rbx, rcx, target) \
    asm volatile( \
        "movq %0, %%rsp\n\t" \
        "movq %1, %%rdi\n\t" \
        "movq %2, %%rsi\n\t" \
        "movq %3, %%rdx\n\t" \
        "movq %4, %%rbx\n\t" \
        "movq %5, %%rcx\n\t" \
        "mov %6, %%rsp\n\t" \
        "ret" \
        : \
        : "g"(stack), "g"(rdi), "g"(rsi), "g"(rdx), "g"(rbx), "g"(rcx), "g"(target) \
        : "rsp", "rdi", "rsi", "rdx", "rbx", "rcx" \
    )



void get_program_headers(int fd, Elf64_Ehdr header, Elf64_Phdr (&pheader)[]) {
  lseek(fd, header.e_phoff, SEEK_SET);
  for (int i = 0; i < header.e_phnum; i++) {
    read(fd, &pheader[i], sizeof(Elf64_Phdr));
  }
}

void* mmap_elf_segments(Elf64_Ehdr elf_hdr, Elf64_Phdr (&pheaders)[], int fd, void* (&loaded_segments)[]) {
  uint64_t start_addr = UINT64_MAX;
  uint64_t end_addr = 0;
  uint64_t total_size;
  for (int i = 0; i < elf_hdr.e_phnum; i++) {
    auto current_prog_header = pheaders[i];
    // if (current_prog_header.p_filesz != current_prog_header.p_memsz) continue;
    if (current_prog_header.p_type != 1) continue;
    if (current_prog_header.p_vaddr < start_addr) {
      start_addr = current_prog_header.p_vaddr;
    }
    end_addr = std::max(current_prog_header.p_vaddr + current_prog_header.p_memsz, end_addr);
  }
  total_size = end_addr - start_addr;
  // 1. Calculate the mmap offset, ensuring it aligns with the page size.
    off_t page_size = sysconf(_SC_PAGESIZE);
    off_t mmap_offset = pheaders[0].p_offset & ~(page_size - 1);
    off_t elf_adjustment = pheaders[0].p_offset - mmap_offset;

    size_t file_backed_size = total_size + elf_adjustment;
  void *base_address = mmap(NULL, file_backed_size, PROT_NONE, MAP_PRIVATE, fd, pheaders[0].p_offset & ~(page_size - 1)) ;
  if (base_address == MAP_FAILED) {
    std::cout<<"Fail allocation of base address\n";
    std::cerr<<errno<<"\n";
    return (void*)1;
  }
  for (int i = 0; i < elf_hdr.e_phnum; i++) {
    auto current_prog_header = pheaders[i];
    if (current_prog_header.p_type != 1) continue;
    // if (current_prog_header.p_memsz != current_prog_header.p_filesz) {
    //   memset(reinterpret_cast<void*>(base_address + current_prog_header.p_filesz), 0, current_prog_header.p_filesz - current_prog_header.p_memsz);
    // }
    void* start = reinterpret_cast<void*>(base_address + current_prog_header.p_vaddr);
    if ((uintptr_t)start % 0x1000 != 0) {
      start += page_size - (reinterpret_cast<uintptr_t>(start) % page_size);
    };
    loaded_segments[i] = start;
    size_t size = current_prog_header.p_memsz;
    int prot = 0;
    if (current_prog_header.p_flags & PF_R) prot |= PROT_READ;
    if (current_prog_header.p_flags & PF_W) prot |= PROT_WRITE;
    if (current_prog_header.p_flags & PF_X) prot |= PROT_EXEC;
    int res = mprotect(start, size, prot);
    if (res == -1) {
      std::cout<<"mprotect fail "<<errno<<"\n";
    }
    if (current_prog_header.p_filesz > current_prog_header.p_memsz) {
      memset(start + current_prog_header.p_filesz, 0, current_prog_header.p_memsz - current_prog_header.p_filesz);
    }
  }
  return base_address;
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
  int interpreter_index;
  for (int i = 0; i < elf_header.e_phnum; i++) {
    if (program_headers[i].p_type == 3) {
      interpreter_index = i;
    }
  }
  // Read value of interpeter path
  char interp[program_headers[interpreter_index].p_filesz];
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
  Elf64_Ehdr interp_elf_hdr;
  if (read(interp_fd, &interp_elf_hdr, sizeof(Elf64_Ehdr)) == -1) {
        std::cerr << "Error reading file." << std::endl;
        return 1;
  }

  // Retrieve Interpreter Program Headers
  Elf64_Phdr interp_program_headers[interp_elf_hdr.e_phnum];
  get_program_headers(interp_fd, interp_elf_hdr, interp_program_headers);

  // Load Interpreter LOAD Segments into memory
  void *loaded_segments[interp_elf_hdr.e_phnum];
  void* base_interp = mmap_elf_segments(interp_elf_hdr, interp_program_headers, interp_fd, loaded_segments);

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

  void* entry_point = reinterpret_cast<void*>(base_interp + interp_elf_hdr.e_entry);
  // char* envp[] = {(char*)"PATH=/bin:/usr/bin", nullptr};
  void** stack_top = (void**)((char*)stack_base + stack_size);

  *--stack_top = 0;
  // push envp strings backwards
  std::vector<void*> envp_pointers;
  stack_top--;
  for (int i = 0; envp[i] != nullptr; i++) {
    envp_pointers.push_back(std::memcpy(stack_top, envp[i], strlen(envp[i])));
    stack_top -= strlen(envp[i]) + 1;
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

  // auxv
  *--stack_top = (void*)0;  // AT_NULL value (terminator)
  *--stack_top = (void*)AT_NULL; // AT_NULL type (terminator)

  *--stack_top = (void*)program_headers; // AT_PHDR value
  *--stack_top = (void*)AT_PHDR;         // AT_PHDR type
  std::cout<<"AT_PHDR: "<<std::hex<<(void*)program_headers<<"\n";

  *--stack_top = (void*)elf_header.e_phentsize; // AT_PHENT value
  *--stack_top = (void*)AT_PHENT;          // AT_PHENT type
  std::cout<<"AT_PHENT: "<<std::dec<<elf_header.e_phentsize<<"\n";

  *--stack_top = (void*)AT_PHNUM;           // AT_PHNUM type
  *--stack_top = (void*)elf_header.e_phnum; // AT_PHNUM value
  std::cout<<"AT_PHNUM: "<<std::dec<<elf_header.e_phnum<<"\n";

  *--stack_top = (void*)sysconf(_SC_PAGESIZE);
  *--stack_top = (void*)AT_PAGESZ;      // AT_PAGESZ

  *--stack_top = loaded_segments[0];
  *--stack_top = (void*)AT_BASE;     // AT_ENTRY type
  std::cout<<"AT_BASE: "<<std::hex<<loaded_segments[0]<<"\n";

  *--stack_top = (void*)AT_ENTRY;     // AT_ENTRY type
  *--stack_top = reinterpret_cast<void*>(elf_header.e_entry + base_elf);  // AT_ENTRY value
  std::cout<<"AT_ENTRY: "<<std::hex<<elf_header.e_entry + base_elf<<"\n";

  *--stack_top = (void*)0;
  for (auto e: envp_pointers) {
    *--stack_top = e;
  }
  *--stack_top = (void*)0;
  for (auto a: argv_pointers) {
    *--stack_top = a;
  }

  // push argc
  *(int*)--stack_top = argc;
  
  // Set up registers and jump to execution
  INIT_AND_JUMP(stack_top, &argc, &argv, &envp, &envp, &argv[0], entry_point);

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

