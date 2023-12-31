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
#include "elf_loader.h"

void* setup_stack(LoadInfo LoadInfo, void* load_address, void* interp_base, int argc, char **argv, char **envp);
void write_data(char *sp, const char *data, size_t data_len);
void write_pointer(char** sp, std::size_t value);
void write_aux_val(char** sp, uint64_t aux_id, uint64_t aux_val);