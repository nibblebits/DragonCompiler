#include "compiler.h"
#include "helpers/vector.h"
void compiler_error(struct compile_process* compiler, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    fprintf(stderr, " on line %i, col %i\n", compiler->pos.line, compiler->pos.col);

    exit(-1);
}

int compile_file(const char *filename)
{
    struct compile_process *process = compile_process_create(filename);
    if (!process)
        return COMPILER_FAILED_WITH_ERRORS;

    if (lex(process) != LEXICAL_ANALYSIS_ALL_OK)
        return COMPILER_FAILED_WITH_ERRORS;

    for (int i = 0; i < vector_count(process->token_vec); i++)
    {
        struct token *ptr;
        ptr = vector_at(process->token_vec, i);
        if (ptr->type == TOKEN_TYPE_NUMBER)
        {
            printf("%i\n", ptr->inum);
            continue;
        }
        
        switch(ptr->type)
        {
            case TOKEN_TYPE_KEYWORD:
            case TOKEN_TYPE_IDENTIFIER:
            case TOKEN_TYPE_STRING:
            case TOKEN_TYPE_COMMENT:
            case TOKEN_TYPE_OPERATOR:
            printf("%s\n", ptr->sval);
            break;

            case TOKEN_TYPE_NUMBER:
            printf("%i\n", ptr->inum);
            break;

            case TOKEN_TYPE_SYMBOL:
            printf("%c\n", ptr->cval);
            break;
        }
    }
    return COMPILER_FILE_COMPILED_OK;
}