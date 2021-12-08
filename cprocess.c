#include "compiler.h"
#include "helpers/vector.h"

#include <memory.h>

const char* default_include_dirs[] = {"./dc_includes", "../dc_includes", "/usr/include/dragon-compiler", "/usr/include"};

void compile_process_destroy(struct compile_process *process)
{
    fclose(process->cfile.fp);
    if (process->ofile)
    {
        fclose(process->ofile);
    }
}

const char *compiler_include_dir_begin(struct compile_process *process)
{
    vector_set_peek_pointer(process->include_dirs, 0);
    const char *dir = vector_peek_ptr(process->include_dirs);
    return dir;
}

const char *compiler_include_dir_next(struct compile_process *process)
{
    const char *dir = vector_peek_ptr(process->include_dirs);
    return dir;
}

void compiler_setup_default_include_directories(struct vector *include_vec)
{
    size_t total = sizeof(default_include_dirs) / sizeof(const char*);
    for (int i = 0; i < total; i++)
    {
        vector_push(include_vec, &default_include_dirs[i]);
    }
}

struct compile_process *compile_process_create(const char *filename, const char *out_filename, int flags, struct compile_process *parent_process)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        return NULL;
    }
    FILE *out_file = NULL;
    if (out_filename)
    {
        out_file = fopen(out_filename, "w");
        if (!out_file)
        {
            return NULL;
        }
    }

    struct compile_process *process = malloc(sizeof(struct compile_process));
    memset(process, 0, sizeof(struct compile_process));
    process->flags = flags;
    // Line's don't start from zero. First line of this file is 1
    process->pos.line = 1;

    process->cfile.fp = file;
    process->ofile = out_file;
    process->token_vec = vector_create(sizeof(struct token));
    process->token_vec_original = vector_create(sizeof(struct token));
    process->node_vec = vector_create(sizeof(struct node *));
    process->node_tree_vec = vector_create(sizeof(struct node *));
    process->resolver = resolver_default_new_process(process);
    process->generator = codegenerator_new(process);

    // Initialize the symbol resolver.
    symresolver_initialize(process);

    // Create a new symbol table for the compiler to use throughout its entire
    // duration.
    symresolver_new_table(process);



    // We have a parent processor? THen they should share the same preprocessor
    // instance, so they share all macro definitions
    if (parent_process)
    {
        process->preprocessor = parent_process->preprocessor;
        process->include_dirs = parent_process->include_dirs;
    }
    else
    {
        // Dislike else optomize this.
        process->preprocessor = preprocessor_create(process);
        // Vector of include directories
        process->include_dirs = vector_create(sizeof(const char *));
        // Setup default include directories
        compiler_setup_default_include_directories(process->include_dirs);
    }

    // Load the absolute file path into the file.
    char *path = malloc(PATH_MAX);
    realpath(filename, path);
    process->cfile.abs_path = path;
    node_set_vector(process->node_vec, process->node_tree_vec);
    return process;
}

FILE *compile_process_file(struct compile_process *process)
{
    return process->cfile.fp;
}

char compile_process_next_char(struct lex_process *lex_process)
{
    struct compile_process *process = lex_process->compiler;
    process->pos.col += 1;
    char c = getc(process->cfile.fp);
    if (c == '\n')
    {
        process->pos.line += 1;
        process->pos.col = 0;
    }
    return c;
}

char compile_process_peek_char(struct lex_process *lex_process)
{
    struct compile_process *process = lex_process->compiler;
    char c = getc(process->cfile.fp);
    ungetc(c, process->cfile.fp);
    return c;
}

void compile_process_push_char(struct lex_process *lex_process, char c)
{
    struct compile_process *process = lex_process->compiler;
    ungetc(c, process->cfile.fp);
}