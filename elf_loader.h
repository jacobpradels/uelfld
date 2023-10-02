#pragma once
#include <elf.h>
#include <limits.h>
#include <iostream>
#include <unistd.h>
#include <sys/mman.h>

void* mmap_elf_segments(Elf64_Ehdr elf_hdr, Elf64_Phdr (&pheaders)[], int fd, void* (&loaded_segments)[]);