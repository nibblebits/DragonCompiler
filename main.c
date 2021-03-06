#include "compiler.h"
#include "helpers/vector.h"
#include <stdio.h>

int main(int argc, char** argv)
{

    if(compile_file("./test.c") != COMPILER_FILE_COMPILED_OK)
    {
        printf("Problem compiling input file\n");
    }
    return 0;
}