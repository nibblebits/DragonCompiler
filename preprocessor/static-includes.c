#include "compiler.h"
void preprocessor_stddef_include(struct preprocessor* preprocessor, struct preprocessor_included_file* file)
{
    preprocessor_definition_create("size_t", lex_process_tokens(tokens_build_for_string(preprocessor->compiler, "int")), NULL, preprocessor);
}

PREPROCESSOR_STATIC_INCLUDE_HANDLER_POST_CREATION preprocessor_static_include_handler_for(const char* filename)
{
    if (S_EQ(filename, "stddef.h"))
    {
        return preprocessor_stddef_include;
    }


    // This filename we are not responsible for, no static include exists for it.
    return NULL;
}