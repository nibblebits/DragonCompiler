#include "compiler.h"

struct compile_process* compile_process_create(const char* filename)
{   
    FILE* file = fopen(filename, "r");
    if (!file)
    {
        return NULL;
    }

    struct compile_process* process = malloc(sizeof(struct compile_process));
    process->cfile = file;
    return process;
}

FILE* compile_process_file(struct compile_process* process)
{
    return process->cfile;
}


char compile_process_next_char(struct compile_process* process)
{
    process->pos.col += 1;
    char c = getc(process->cfile);
    if (c == '\n')
    {
        process->pos.line +=1 ;
        process->pos.col = 0;
    }
    return c;
}