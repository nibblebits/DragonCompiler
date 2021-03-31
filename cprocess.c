#include "compiler.h"
#include "helpers/vector.h"
#include <memory.h>

struct compile_process* compile_process_create(const char* filename)
{   
    FILE* file = fopen(filename, "r");
    if (!file)
    {
        return NULL;
    }

    struct compile_process* process = malloc(sizeof(struct compile_process));
    memset(process, 0, sizeof(struct compile_process));
    process->cfile = file;
    process->token_vec = vector_create(sizeof(struct token));
    process->node_vec = vector_create(sizeof(struct node*));
    process->node_tree_vec = vector_create(sizeof(struct node*));
    process->generator.states.expr = vector_create(sizeof(struct expression_state*));
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
        process->pos.line +=1;
        process->pos.col = 0;
    }
    return c;
}

char compile_process_peek_char(struct compile_process* process)
{
    char c = getc(process->cfile);
    ungetc(c, process->cfile);
    return c;
}

void compile_process_push_char(struct compile_process* process, char c)
{
    ungetc(c, process->cfile);
}