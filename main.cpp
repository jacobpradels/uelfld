#include <elf.h>
#include "elf_loader.h"
#include "stack_setup.h"
int main(int argc, char** argv, char** envp) {
    void* entry_point;
    void* interp_base;
    LoadInfo elf_info = parse_elf(std::string(argv[1]));
    if (elf_info.elf_interp) {
      std::cout<<elf_info.elf_interp.value()<<"\n";
      LoadInfo interp_info = parse_elf(elf_info.elf_interp.value());
      interp_base = load_segments(interp_info);
      entry_point = interp_base + interp_info.entry_point;
    } else {
      interp_base = (void*)0;
      entry_point = (void*)elf_info.entry_point;
    }
    void* load_address = load_segments(elf_info);

    void* stack_top = setup_stack(elf_info, load_address, interp_base, argc, argv, envp);
    std::cout<<std::hex<<entry_point<<"\n";
    asm volatile(
        "mov %0, %%rsp\n\t"
        "push %1\n\t"

        "xor %%rax, %%rax\n\t"
        "xor %%rbx, %%rbx\n\t"
        "xor %%rcx, %%rcx\n\t"
        "xor %%rdx, %%rdx\n\t"
        "xor %%rdi, %%rdi\n\t"
        "xor %%rsi, %%rsi\n\t"

        "xor %%r9, %%r9\n\t"
        "xor %%r10, %%r10\n\t"
        "xor %%r11, %%r11\n\t"
        "xor %%r12, %%r12\n\t"
        "xor %%r13, %%r13\n\t"
        "xor %%r14, %%r14\n\t"
        "xor %%r15, %%r15\n\t"

        "ret\n\t"
        :
        : "a"(stack_top), "b"(entry_point)
    );
    return 0;
}