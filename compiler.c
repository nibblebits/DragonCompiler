#include "compiler.h"
#include "helpers/vector.h"
int compile_file(const char* filename)
{
    struct compile_process* process = compile_process_create(filename); 
    if (!process)
        return COMPILER_FAILED_WITH_ERRORS;

    if (lex(process) != LEXICAL_ANALYSIS_ALL_OK)
        return COMPILER_FAILED_WITH_ERRORS;

    for (int i = 0; i < vector_count(process->token_vec); i++)
    {
        struct token* ptr;
        ptr = vector_at(process->token_vec, i);
        printf("%s\n", ptr->sval);
    }
    return COMPILER_FILE_COMPILED_OK;
}