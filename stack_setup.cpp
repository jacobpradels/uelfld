#include "stack_setup.h"

void write_data(char *sp, const char *data, size_t data_len) {
    memcpy(sp, data, data_len);
}

void write_pointer(char** sp, std::size_t value) {
    std::size_t* ptr = reinterpret_cast<std::size_t*>(*sp);
    *ptr = value;
    *sp += 8;  // Incrementing by 8 bytes
}

void write_aux_val(char** sp, uint64_t aux_id, uint64_t aux_val) {
    write_pointer(sp, aux_id);
    write_pointer(sp, aux_val);
}


void* setup_stack(Elf64_Ehdr LoadInfo, void* load_address, void* interp_base, char **argv, char **envp) {
    int stack_flags = 0;
    stack_flags |= MAP_PRIVATE;
    stack_flags |= MAP_ANONYMOUS;
    stack_flags |= MAP_STACK;

    int stack_prot = 0;
    stack_prot |= PROT_WRITE;
    stack_prot |= PROT_READ;

    int stack_size = 1024 * 256;
    void* stack_end = mmap(NULL, stack_size, stack_prot, stack_flags, -1, 0);
    if (stack_end == (void*)-1) {
        std::cerr<<"Failed to allocate memory for the stack errno: "<<errno<<"\n";
    }
    char* stack_pointer = (char*)((uint64_t)stack_end + stack_size);
    stack_pointer = (char*)((uint64_t)(stack_pointer + 15) & (~15));
    
    std::vector<char*> env_pointers;
    for (char** env = envp; *env != 0; env++) {
        char* thisEnv = *env;
        stack_pointer -= strlen(thisEnv);
        write_data(stack_pointer, thisEnv, strlen(thisEnv));
        env_pointers.push_back(stack_pointer);
    }
    std::vector<char*> arg_pointers;
    int i =0;
    for (char** arg = argv; *arg != 0; arg++, i++) {
        // Skip the name of the executable
        if (i == 1) continue;
        char* thisArg = *arg;
        stack_pointer -= (strlen(thisArg) + 1);
        arg_pointers.push_back(stack_pointer);
        write_data(stack_pointer, thisArg, strlen(thisArg));
    }

    // 16 byte align stack_pointer
    stack_pointer = (char*)((uint64_t)stack_pointer & ~0xf);

    // Platform string on the stack
    stack_pointer -= strlen("x86_64\0") + 1;
    write_data(stack_pointer, "x86_64\0",strlen("x86_64\0"));

    // 16 bytes of random data as PRNG seed
    std::random_device rd;  // Seed for the random number engine
    std::mt19937 gen(rd());  // Mersenne twister engine seeded with rd()
    std::uniform_int_distribution<> distrib(0, 255);
    char* random_data = new char[16];
    for(int i = 0; i < 16; ++i) {
        random_data[i] = static_cast<char>(distrib(gen));
    }
    stack_pointer -= strlen(random_data);
    write_data(stack_pointer, random_data, strlen(random_data));
    char* prng_pointer = stack_pointer;

    // space for auxv
    stack_pointer -= 0x120;

    // make space for argv and envp pointers
    uint64_t pointers = (arg_pointers.size() + 1) + (env_pointers.size() + 1) + 1;
    stack_pointer -= pointers * 8;
    
    // 16 byte align stack_pointer again
    stack_pointer = (char*)((uint64_t)stack_pointer & ~0xf);
    // the pointer we will return
    void* rsp = reinterpret_cast<void*>(stack_pointer);

    // write argc
    write_pointer(&stack_pointer, arg_pointers.size());
    
    for (char* a : arg_pointers) {
        std::cout<<std::hex<<(uint64_t)a<<"\n";
        write_pointer(&stack_pointer, (uint64_t)a);
    }

    // NULL for end of argv
    write_pointer(&stack_pointer, 0x00);

    for (char* e : env_pointers) {
        write_pointer(&stack_pointer, (uint64_t)e);
    }

    // NULL for end of envp
    write_pointer(&stack_pointer, 0x00);

    write_aux_val(&stack_pointer, AT_SYSINFO_EHDR, getauxval(AT_SYSINFO_EHDR));

    write_aux_val(&stack_pointer, AT_HWCAP, getauxval(AT_HWCAP));
    write_aux_val(&stack_pointer, AT_PAGESZ, getauxval(AT_PAGESZ));
    write_aux_val(&stack_pointer, AT_CLKTCK, getauxval(AT_CLKTCK));
    write_aux_val(&stack_pointer, AT_HWCAP2, getauxval(AT_HWCAP2));

    write_aux_val(&stack_pointer, AT_PHDR, (uint64_t)load_address + (uint64_t)LoadInfo.e_phoff);
    write_aux_val(&stack_pointer, AT_PHENT, 0x38);
    write_aux_val(&stack_pointer, AT_PHNUM, (uint64_t)LoadInfo.e_phnum);

    // base adddress of the elf interpeter
    write_aux_val(&stack_pointer, AT_BASE, (uint64_t)interp_base);

    write_aux_val(&stack_pointer, AT_FLAGS, 0x0);

    switch (LoadInfo.e_type) {
        // Dynamic
        case 3:
            write_aux_val(&stack_pointer, AT_ENTRY, ((uint64_t)load_address + (uint64_t)LoadInfo.e_entry));
            break;
        // Exec
        case 2:
            write_aux_val(&stack_pointer, AT_ENTRY, (uint64_t)LoadInfo.e_entry);
            break;
        default:
            std::cerr<<"Wrong e_type\n";
    }

    write_aux_val(&stack_pointer, AT_UID, getauxval(AT_UID));
    write_aux_val(&stack_pointer, AT_EUID, getauxval(AT_EUID));
    write_aux_val(&stack_pointer, AT_GID, getauxval(AT_GID));
    write_aux_val(&stack_pointer, AT_EGID, getauxval(AT_EGID));
    write_aux_val(&stack_pointer, AT_SECURE, getauxval(AT_SECURE));

    // 16 bytes of random memory for libc to use
    write_aux_val(&stack_pointer, AT_RANDOM, (uint64_t)prng_pointer);

    write_aux_val(&stack_pointer, AT_EXECFN, (uint64_t)&arg_pointers[0]);

    write_aux_val(&stack_pointer, AT_NULL, 0x0);

    return rsp;
}