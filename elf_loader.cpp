#include "elf_loader.h"

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
    off_t elf_adjustment = pheaders[0].p_offset + mmap_offset;

    size_t file_backed_size = total_size + elf_adjustment;
  void *base_address = mmap(NULL, file_backed_size, PROT_NONE, MAP_PRIVATE, fd, pheaders[0].p_offset & ~(page_size - 1)) ;
  if (base_address == MAP_FAILED) {
    std::cout<<"Fail allocation of base address\n";
    std::cerr<<errno<<"\n";
    return (void*)1;
  }
  for (int i = 0; i < elf_hdr.e_phnum; i++) {
    auto current_prog_header = pheaders[i];
    auto starting_offset = pheaders[0].p_vaddr;
    if (current_prog_header.p_type != 1) continue;
    void* start = reinterpret_cast<void*>(base_address + current_prog_header.p_vaddr - starting_offset);
    if ((uintptr_t)start % page_size != 0) {
      start += page_size - (reinterpret_cast<uintptr_t>(start) % page_size);
    };
    loaded_segments[i] = start;
    size_t size = current_prog_header.p_filesz;
    int prot = 0;
    if (current_prog_header.p_flags & PF_R) prot |= PROT_READ;
    if (current_prog_header.p_flags & PF_W) prot |= PROT_WRITE;
    if (current_prog_header.p_flags & PF_X) prot |= PROT_EXEC;
    if (current_prog_header.p_memsz > current_prog_header.p_filesz) {
      int res = mprotect(start, current_prog_header.p_memsz, prot);
      if (res == -1) {
        std::cout<<"mprotect fail "<<errno<<"\n";
      }
      size_t bss_size = (current_prog_header.p_memsz - current_prog_header.p_filesz);
      std::cout<<start + current_prog_header.p_filesz<<"\n";
      std::cout<<bss_size<<"\n";
      std::cout<<current_prog_header.p_memsz<<"\n";
      // memset(start + current_prog_header.p_filesz, 0, bss_size);
    } else {
      int res = mprotect(start, size, prot);
      if (res == -1) {
        std::cout<<"mprotect fail "<<errno<<"\n";
      }
    }
  }
  return base_address;
}