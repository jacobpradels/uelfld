#pragma once
#include <elf.h>
#include <limits.h>
#include <iostream>
#include <unistd.h>
#include <sys/mman.h>
#include <vector>
#include <string>
#include <optional>
#include <fstream>
#include <fcntl.h>
#include <cstring>


struct Segment {
    Elf64_Phdr program_header;
    std::vector<char> buffer;
};

struct LoadInfo {
    std::size_t entry_point;
    std::size_t pheader_off;
    std::size_t pheader_num;
    void* base_address;
    Elf32_Half etype;
    std::vector<Segment> segments;
    std::optional<std::string> elf_interp;
};
std::optional<std::string> fetch_interpreter(std::vector<Segment> segments, int file_descriptor);
std::vector<Segment> fetch_segments(Elf64_Ehdr elf_header, int file_descriptor);
LoadInfo parse_elf(std::string path);
size_t get_mapping_size(std::vector<Segment> segments);
void* load_segments(LoadInfo load_info);