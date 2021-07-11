#include "compiler.h"
#include "helpers/vector.h"
#include <stdio.h>
#include <math.h>

int main(int argc, char** argv)
{

   // struct string* s = string_make("Hello world");

    if(compile_file("./test.c") != COMPILER_FILE_COMPILED_OK)
    {
        printf("Problem compiling input file\n");
    }
    return 0;
}