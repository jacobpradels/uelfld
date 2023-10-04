#pragma once
#include <elf.h>
#include <limits.h>
#include <iostream>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <random>
#include <vector>
#include <sys/auxv.h>

void* setup_stack(Elf64_Ehdr LoadInfo, Elf64_Ehdr Interp_header, void* load_address, void* interp_base, char **argv, char **envp, char* interp_path);
void write_data(char *sp, const char *data, size_t data_len);
void write_pointer(char** sp, std::size_t value);
void write_aux_val(char** sp, uint64_t aux_id, uint64_t aux_val);