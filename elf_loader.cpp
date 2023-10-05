#include "elf_loader.h"
std::optional<std::string> fetch_interpreter(std::vector<Segment> segments, int file_descriptor) {
  for (const auto& segment : segments) {
    if (segment.program_header.p_type == PT_INTERP) {
      if (::lseek(file_descriptor, segment.program_header.p_offset, SEEK_SET) == -1) {
        return std::nullopt;
      }
      std::vector<char> interp(segment.program_header.p_filesz, '\0');
      ssize_t bytesRead = read(file_descriptor, interp.data(), segment.program_header.p_filesz);
      if (bytesRead <= 0) {
        return std::nullopt;
      }
      return std::string(interp.begin(), interp.end());
    }
  }
  return std::nullopt;
}
std::vector<Segment> fetch_segments(Elf64_Ehdr elf_header, int file_descriptor) {
  std::vector<Segment> segments;

  // Move the file offset to the start of the program headers
  // Read each program header and store it in the vector
  for (int i = 0; i < elf_header.e_phnum; ++i) {
      if (lseek(file_descriptor, elf_header.e_phoff + elf_header.e_phentsize * i, SEEK_SET) == -1) {
        std::cerr<<"Failed to seek file while reading program headers.\n";
      }
      Elf64_Phdr program_header;
      ssize_t bytesRead = read(file_descriptor, &program_header, sizeof(Elf64_Phdr));
      // Check for read error or end-of-file
      if (bytesRead <= 0) {
          // Handle error
          break;
      }
      if (program_header.p_filesz <= 0) continue;
      if (lseek(file_descriptor, program_header.p_offset, SEEK_SET) == -1) {
        std::cerr<<"Failed to seek file while reading program headers.\n";
        exit(1);
      }
      Segment r_segment;
      r_segment.buffer = std::vector<char>(program_header.p_filesz, '\0');
      bytesRead = read(file_descriptor, r_segment.buffer.data(), program_header.p_filesz);
      if (bytesRead <= 0) {
        std::cerr<<"Failed to read segment data\n";
        exit(1);
      }
      r_segment.program_header = program_header;
      segments.push_back(r_segment);
  }
  return segments;
}

LoadInfo parse_elf(std::string path) {
  int file_descriptor = open(path.c_str(), O_RDONLY);
  if (file_descriptor == -1) {
      std::cerr << "Error opening file." << std::endl;
      exit(1);
  }

  // Retrieve ELF Header
  Elf64_Ehdr elf_header;
  if (read(file_descriptor, &elf_header, sizeof(Elf64_Ehdr)) == -1) {
        std::cerr << "Error reading file." << std::endl;
        exit(1);
  }
  LoadInfo elf_info;
  LoadInfo interp_info;
  elf_info.entry_point = elf_header.e_entry;
  elf_info.pheader_off = elf_header.e_phoff;
  elf_info.pheader_num = elf_header.e_phnum;
  elf_info.etype = elf_header.e_type;
  elf_info.segments = fetch_segments(elf_header, file_descriptor);
  elf_info.elf_interp = fetch_interpreter(elf_info.segments, file_descriptor);
  
  close(file_descriptor);
  return elf_info;
}

size_t get_mapping_size(std::vector<Segment> segments) {
  auto last_idx = segments.size() - 1;
  return segments[last_idx].program_header.p_vaddr + segments[last_idx].program_header.p_memsz - (segments[0].program_header.p_vaddr & ~15);
}

void* load_segments(LoadInfo load_info) {
  const std::vector<Segment> all_segments = load_info.segments;
  std::vector<Segment> segments;
  for (const auto& seg: all_segments) {
    if (seg.program_header.p_type == PT_LOAD) {
      segments.push_back(seg);
    }
  }
  size_t mapping_size = get_mapping_size(segments);
  int prot = PROT_READ | PROT_WRITE;
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  void* mmap_addr;
  switch (load_info.etype) {
    case ET_EXEC:
      flags |= MAP_FIXED;
      mmap_addr = (void*)(reinterpret_cast<uint64_t>(segments[0].program_header.p_vaddr) - reinterpret_cast<uint64_t>(segments[0].program_header.p_offset));
      break;
    case ET_DYN:
      mmap_addr = nullptr;
      break;
  }
  void* load_address = mmap(mmap_addr, mapping_size, prot, flags, -1, 0);
  if ((uint64_t)load_address == -1) {
    std::cerr<<"Failed to map memory chunk"<<std::endl;
    exit(1);
  }

  uint64_t load_base = mmap_addr == 0 ? (uint64_t)load_address : 0;
  for (const auto& segment: segments) {
    uint64_t page_size = sysconf(_SC_PAGESIZE);
    auto addr = load_base + segment.program_header.p_vaddr;
    auto size = segment.program_header.p_filesz + (addr & page_size - 1);

    addr = addr & ~(page_size - 1);
    size = (size + page_size - 1) & ~(page_size - 1);

    void* result = memcpy((void*)addr, segment.buffer.data(), segment.buffer.size());
    if ((uint64_t)result == -1) {
      std::cerr<<"Failed to copy memory segment"<<std::endl;
      exit(1);
    }
    prot = 0;
    if (segment.program_header.p_flags & PF_R) prot |= PROT_READ;
    if (segment.program_header.p_flags & PF_W) prot |= PROT_WRITE;
    if (segment.program_header.p_flags & PF_X) prot |= PROT_EXEC;
    int protect_result = mprotect((void*)addr, size, prot);
    if (protect_result == -1) {
      std::cerr<<"Failed to set protections on memory"<<std::endl;
      std::cerr<<"Error number: "<<errno<<"\n";
      exit(1);
    }
  }
  return load_address;
}