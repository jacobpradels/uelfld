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

  // Load Segments
  void *loaded_segments[elf_header.e_phnum];
  for (int i = 0; i < elf_header.e_phnum; i++) {
    auto current_prog_header = program_headers[i];
    if (current_prog_header.p_type == 3) {
      lseek(fd, current_prog_header.p_offset, SEEK_SET);
      char interp[current_prog_header.p_filesz];
      read(fd, &interp, current_prog_header.p_filesz);
      std::cout<<interp<<"\n";
    }
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

    loaded_segments[i] = mmap(NULL, total_size, prot, MAP_PRIVATE, fd, aligned_offset);
    if (loaded_segments[i] == (void*)-1) {
        std::cerr<<"Failed to allocate memory\n";
        std::cerr << "Error code: " << errno << std::endl;
        continue;
    }
    
  }

  // Close file
  close(fd);

  // Clean up memory
  for (int i = 0; i < elf_header.e_phnum; i++) {
    auto current_prog_header = program_headers[i];
    if (current_prog_header.p_type != 1) {
        continue;
    }
    std::cout<<loaded_segments[i]<<"\n";
    // munmap(reinterpret_cast<void*>(current_prog_header.p_vaddr), current_prog_header.p_memsz);
  }
  return 0;
}