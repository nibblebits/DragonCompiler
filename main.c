#include "compiler.h"
#include "helpers/hashmap.h"
#include <stdio.h>
#include <math.h>

int main(int argc, char **argv)
{
    const char *input_file = "./test.c";
    const char *output_file = "./a.out";
    const char *option = "exec";
    if (argc > 1)
    {
        input_file = argv[1];
    }
    if (argc > 2)
    {
        output_file = argv[2];
    }
    if (argc > 3)
    {
        option = argv[3];
    }

    int compile_flags = COMPILE_PROCESS_EXECUTE_NASM;
    if (S_EQ(option, "object"))
    {
        compile_flags |= COMPILE_PROCESS_EXPORT_AS_OBJECT;
    }

    if (compile_file(input_file, output_file, compile_flags) != COMPILER_FILE_COMPILED_OK)
    {
        printf("Problem compiling file\n");
    }

    // We should invoke the NASM assembler if we are instructed to do so.
    if (compile_flags & COMPILE_PROCESS_EXECUTE_NASM)
    {
        char nasm_output_file[40];
        char nasm_cmd[512];
        sprintf(nasm_output_file, "%s.o", output_file);

        if (compile_flags & COMPILE_PROCESS_EXPORT_AS_OBJECT)
        {
            sprintf(nasm_cmd, "nasm -f elf32 %s -o %s", output_file, nasm_output_file);
        }
        else
        {
            sprintf(nasm_cmd, "nasm -f elf32 %s -o %s && gcc -m32 %s -o %s", output_file, nasm_output_file, nasm_output_file, output_file);
        }

        printf("%s", nasm_cmd);
        int res = system(nasm_cmd);
        if (res < 0)
        {
            printf("Issue assembling the assembly file with NASM and linking with GCC");
            return res;
        }


    }
    return 0;
}