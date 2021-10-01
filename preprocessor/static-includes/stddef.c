#include "compiler.h"
enum
{
    OFFSET_OF_IS_UNION,
    OFFSET_OF_IS_STRUCT
};

int preprocessor_stddef_includeof_get_offsetof_type(struct token *token)
{
    int type = -1;
    if (token_is_keyword(token, "struct"))
    {
        type = OFFSET_OF_IS_STRUCT;
    }
    else if (token_is_keyword(token, "union"))
    {
        type = OFFSET_OF_IS_UNION;
    }

    return type;
}
int preprocessor_stddef_include_offsetof_pull_struct_or_union(struct compile_process *compiler, struct preprocessor_function_argument *argument, const char **name_out)
{
    int type = -1;
    vector_set_peek_pointer(argument->tokens, 0);
    if (vector_count(argument->tokens) != 2)
    {
        compiler_error(compiler, "Expecting either \"struct name\" or \"union name\"");
    }

    struct token *arg_type = vector_peek(argument->tokens);
    type = preprocessor_stddef_includeof_get_offsetof_type(arg_type);

    struct token *arg_name = vector_peek(argument->tokens);
    if (arg_name->type != TOKEN_TYPE_IDENTIFIER)
    {
        compiler_error(compiler, "Expecting the offsetof structure/union to have a name, but something else was provided");
    }

    *name_out = arg_name->sval;
    return type;
}

int preprocessor_stddef_include_offsetof_pull_member_name(struct compile_process *compiler, struct preprocessor_function_argument *argument, const char **member_name_out)
{
    vector_set_peek_pointer(argument->tokens, 0);
    struct token *token = vector_peek(argument->tokens);
    if (token->type != TOKEN_TYPE_IDENTIFIER)
    {
        compiler_error(compiler, "Expecting an identifier for the offsetof member to search for");
    }

    *member_name_out = token->sval;
    return 0;
}
int preprocessor_stddef_include_offsetof(struct preprocessor_definition *definition, struct preprocessor_function_arguments *arguments)
{
    int arg_type = -1;
    const char *arg_name = NULL;
    const char *member_name = NULL;
    struct preprocessor *preprocessor = definition->preprocessor;
    struct compile_process *compiler = preprocessor->compiler;
    if (preprocessor_function_arguments_count(arguments) != 2)
    {
        compiler_error(compiler, "Native macro function offsetof takes two function arguments");
    }

    // Okay, we must resolve the structure, then peek for the given variable target
    struct preprocessor_function_argument *struct_or_union_arg = preprocessor_function_argument_at(arguments, 0);
    struct preprocessor_function_argument *member_arg = preprocessor_function_argument_at(arguments, 1);
    arg_type = preprocessor_stddef_include_offsetof_pull_struct_or_union(compiler, struct_or_union_arg, &arg_name);
    assert(arg_type != -1);
    preprocessor_stddef_include_offsetof_pull_member_name(compiler, member_arg, &member_name);

    // We don't support unions yet
    assert(arg_type == OFFSET_OF_IS_STRUCT);
    struct symbol *sym = symresolver_get_symbol(compiler, arg_name);
    if (!sym)
    {
        compiler_error(compiler, "The given structure or union with the name %s does not exist", arg_name);
    }
    assert(sym->type == SYMBOL_TYPE_NODE);
    struct node *s_node = sym->data;
    assert(s_node->type == NODE_TYPE_STRUCT);

    return 0;
}
int preprocessor_stddef_include_offsetof_evaluate(struct preprocessor_definition *definition, struct preprocessor_function_arguments *arguments)
{
    struct preprocessor *preprocessor = definition->preprocessor;
    struct compile_process *compiler = preprocessor->compiler;
    compiler_error(compiler, "Sorry you may not use offsetof in this way");
}

struct vector *preprocessor_stddef_include_offsetof_value(struct preprocessor_definition *definition, struct preprocessor_function_arguments *arguments)
{
    int result = preprocessor_stddef_include_offsetof(definition, arguments);
    return preprocessor_build_value_vector_for_integer(result);
}

void preprocessor_stddef_include(struct preprocessor *preprocessor, struct preprocessor_included_file *file)
{
    preprocessor_definition_create("NULL", lex_process_tokens(tokens_build_for_string(preprocessor->compiler, "0")), NULL, preprocessor);
    preprocessor_definition_create("size_t", lex_process_tokens(tokens_build_for_string(preprocessor->compiler, "int")), NULL, preprocessor);
    preprocessor_definition_create("wchar_t", lex_process_tokens(tokens_build_for_string(preprocessor->compiler, "short")), NULL, preprocessor);
    preprocessor_definition_create("ptrdiff_t", lex_process_tokens(tokens_build_for_string(preprocessor->compiler, "int")), NULL, preprocessor);

    // We must create some macro functions for stddef.h
    //offsetof(type, member-designator)
    preprocessor_definition_create_native("offsetof", preprocessor_stddef_include_offsetof_evaluate, preprocessor_stddef_include_offsetof_value, preprocessor);
}
