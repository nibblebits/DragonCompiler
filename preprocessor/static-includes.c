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

//void va_copy (__builtin_va_list dest, __builtin_va_list src);
void native_va_copy(struct generator *generator, struct node *orinating_function, struct native_function *func, struct vector *arguments)
{
    struct compile_process *compiler = generator->compiler;
    if (vector_count(arguments) != 2)
    {
        compiler_error(compiler, "va_copy expects two arguments %i provided", vector_count(arguments));
    }

    generator->asm_push("; VA_COPY start");
    struct node *dest_arg = vector_peek_ptr(arguments);
    struct node *src_arg = vector_peek_ptr(arguments);
    generator->gen_exp(generator, src_arg, 0);
    register_unset_flag(REGISTER_EAX_IS_USED);

    struct resolver_result *result = resolver_follow(compiler->resolver, dest_arg);
    assert(resolver_result_ok(result));
    struct resolver_entity *dest_arg_entity = resolver_result_entity_root(result);
    struct generator_entity_address address_out;
    generator->entity_address(generator, dest_arg_entity, &address_out);
    // we got the final argument node, lets now load the address into list_arg.
    generator->asm_push("mov dword [%s], eax", address_out.address);
    generator->asm_push("; VA_COPY end");


}

/**
 * va_start(__builtin_va_list list, void* last_stack_var)
 */
void native_va_start(struct generator *generator, struct node *orinating_function, struct native_function *func, struct vector *arguments)
{
    struct compile_process *compiler = generator->compiler;
    if (vector_count(arguments) != 2)
    {
        compiler_error(compiler, "va_start expects two arguments %i provided", vector_count(arguments));
    }

    struct node *list_arg = vector_peek_ptr(arguments);
    struct node *stack_arg = vector_peek_ptr(arguments);
    if (stack_arg->type != NODE_TYPE_IDENTIFIER)
    {
        compiler_error(compiler, "Expecting a valid stack argument for va_start");
    }
    generator->asm_push("; va_start on variable %s", stack_arg->sval);
    vector_set_peek_pointer(arguments, 0);

    generator->gen_exp(generator, stack_arg, EXPRESSION_GET_ADDRESS);
    register_unset_flag(REGISTER_EBX_IS_USED);

    struct resolver_result *result = resolver_follow(compiler->resolver, list_arg);
    assert(resolver_result_ok(result));
    struct resolver_entity *list_arg_entity = resolver_result_entity_root(result);
    struct generator_entity_address address_out;
    generator->entity_address(generator, list_arg_entity, &address_out);
    // we got the final argument node, lets now load the address into list_arg.
    generator->asm_push("mov dword [%s], ebx", address_out.address);
    generator->asm_push("; va_start end for variable %s", stack_arg->sval);
}

/**
 * 
 * void* __builtin_va_arg(__builtin_va_list list, int type)
 */
void native___builtin_va_arg(struct generator *generator, struct node *orinating_function, struct native_function *func, struct vector *arguments)
{

    struct compile_process *compiler = generator->compiler;
    if (vector_count(arguments) != 2)
    {
        compiler_error(compiler, "va_start expects two arguments %i provided", vector_count(arguments));
    }
    generator->asm_push("; va_arg start");
    vector_set_peek_pointer(arguments, 0);
    // We must generate the left argument which will resolve the va_list
    struct node *list_arg = vector_peek_ptr(arguments);
    generator->gen_exp(generator, list_arg, EXPRESSION_GET_ADDRESS);
    register_unset_flag(REGISTER_EBX_IS_USED);
    struct node *size_argument = vector_peek_ptr(arguments);
    if (size_argument->type != NODE_TYPE_NUMBER)
    {
        compiler_error(compiler, "va_arg expects second argument to be numeric size of variable argument. Use macros for automation");
    }
    generator->asm_push("add dword [ebx], %i", size_argument->llnum);
    generator->gen_exp(generator, list_arg, 0);
    register_unset_flag(REGISTER_EAX_IS_USED);
    generator->asm_push("mov dword eax, [eax]");
    generator->asm_push("; va_arg end");
}

void native_va_end(struct generator *generator, struct node *orinating_function, struct native_function *func, struct vector *arguments)
{
    // Nothing to do really..
}

void preprocessor_stdarg_include(struct preprocessor *preprocessor, struct preprocessor_included_file *file)
{
    preprocessor_definition_create("__builtin_va_list", lex_process_tokens(tokens_build_for_string(preprocessor->compiler, "int")), NULL, preprocessor);
    struct symbol *sym = native_create_function(preprocessor->compiler, "va_start", &(struct native_function_callbacks){.call = native_va_start});
    if (!sym)
    {
        compiler_error(preprocessor->compiler, "The function va_start cannot be declared as a symbol with the same name already is present");
    }

    sym = native_create_function(preprocessor->compiler, "__builtin_va_arg", &(struct native_function_callbacks){.call = native___builtin_va_arg});
    if (!sym)
    {
        compiler_error(preprocessor->compiler, "The function native___builtin_va_arg cannot be declared as a symbol with the same name already is present");
    }

    sym = native_create_function(preprocessor->compiler, "va_end", &(struct native_function_callbacks){.call = native_va_end});
    if (!sym)
    {
        compiler_error(preprocessor->compiler, "The function va_end cannot be declared as a symbol with the same name already is present");
    }

    sym = native_create_function(preprocessor->compiler, "va_copy", &(struct native_function_callbacks){.call = native_va_copy});
    if (!sym)
    {
        compiler_error(preprocessor->compiler, "The function va_copy cannot be declared as a symbol with the same name already is present");
    }
}

PREPROCESSOR_STATIC_INCLUDE_HANDLER_POST_CREATION preprocessor_static_include_handler_for(const char *filename)
{
    if (S_EQ(filename, "stddef.h"))
    {
        return preprocessor_stddef_include;
    }
    else if (S_EQ(filename, "stdarg.h"))
    {
        return preprocessor_stdarg_include;
    }

    // This filename we are not responsible for, no static include exists for it.
    return NULL;
}