#include "compiler.h"

int compile_file(const char* filename)
{
    struct compile_process* process = compile_process_create(filename);
    if (!process)
        return COMPILER_FAILED_WITH_ERRORS;

    if (lex(process) != LEXICAL_ANALYSIS_ALL_OK)
        return COMPILER_FAILED_WITH_ERRORS;

    return COMPILER_FILE_COMPILED_OK;
}