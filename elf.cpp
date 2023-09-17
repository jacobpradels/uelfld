#include <stdio.h>

int main(int argc, char** argv)
{
    fprintf(stdout, "Hello world! fprintf=%p, stdout=%p argc=%d\n", fprintf, stdout, argc);
    return 0;
}