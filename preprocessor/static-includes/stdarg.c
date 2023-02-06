#include "compiler.h"
//void va_copy (__builtin_va_list dest, __builtin_va_list src);
void native_va_copy(struct generator *generator, struct native_function *func, struct vector *arguments)
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
void native_va_start(struct generator *generator, struct native_function *func, struct vector *arguments)
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

    struct datatype void_datatype;
    datatype_set_void(&void_datatype);
    generator->ret(&void_datatype, "0");
}

/**
 * 
 * void* __builtin_va_arg(__builtin_va_list list, int type)
 */
void native___builtin_va_arg(struct generator *generator, struct native_function *func, struct vector *arguments)
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
    generator->asm_push("mov dword eax, [ebx] ");
    struct datatype void_dtype;
    datatype_set_void(&void_dtype);
    void_dtype.pointer_depth++;
    void_dtype.flags |= DATATYPE_FLAG_IS_POINTER;
    generator->ret(&void_dtype, "dword [eax]");

    generator->asm_push("; va_arg end");
}

void native_va_end(struct generator *generator, struct native_function *func, struct vector *arguments)
{
    // Nothing to do really..

        struct datatype void_datatype;
    datatype_set_void(&void_datatype);
    generator->ret(&void_datatype, "0");
    
}

void preprocessor_stdarg_internal_include(struct preprocessor *preprocessor, struct preprocessor_included_file *file)
{
    
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
