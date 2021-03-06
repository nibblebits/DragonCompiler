#include "compiler.h"
#include "helpers/vector.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    if(compile_file("./test.c") != COMPILER_FILE_COMPILED_OK)
    {
        printf("Problem compiling input file\n");
    }

    struct vector* vec = vector_create(sizeof(int));

    int val = 0;
    for (int i = 0; i < vector_count(vec); i++)
    {
        val = 50;
        printf("%i\n", *(int*)(vector_at(vec, i)));
    }

    vector_free(vec);
    return 0;
}