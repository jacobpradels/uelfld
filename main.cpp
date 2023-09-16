#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <elf.h>
// typedef struct {
//     unsigned char e_ident[EI_NIDENT];
//     Elf32_Half e_type;
//     Elf32_Half e_machine;
//     Elf32_Word e_version;
//     Elf32_Addr e_entry;
//     Elf32_Off e_phoff;
//     Elf32_Off e_shoff;
//     Elf32_Word e_flags;
//     Elf32_Half e_ehsize;
//     Elf32_Half e_phentsize;
//     Elf32_Half e_phnum;
//     Elf32_Half e_shentsize;
//     Elf32_Half e_shnum;
//     Elf32_Half e_shstrndx;
// } Elf32_Ehdr;
// using namespace std;
int main(int argc, char** argv) {
    std::ifstream file("elf", std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open the file!" << std::endl;
        return 1;
    }

    // Retrieve ELF Header
    Elf64_Ehdr elf_header;
    file.read(reinterpret_cast<char*>(&elf_header), sizeof(Elf64_Ehdr));

    // Retrieve Program Headers
    Elf64_Off program_header_offset = elf_header.e_phoff;
    file.seekg(elf_header.e_phoff, std::ios::beg);
    Elf64_Phdr program_headers[elf_header.e_phnum];
    // Elf64_Phdr program_header_1;
    for (int i = 0; i < elf_header.e_phnum; i++) {
        file.read(reinterpret_cast<char*>(&program_headers[i]), sizeof(Elf64_Phdr));
    }
    for (auto a : program_headers) {
        std::cout<<a.p_type<<"\n";
    }
    file.close();
    return 0;
}