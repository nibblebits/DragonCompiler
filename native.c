#include "compiler.h"

struct symbol* native_create_function(struct compile_process* compiler, const char* name, struct native_function_callbacks* callbacks)
{
    struct native_function* func = calloc(sizeof(struct native_function), 1);
    memcpy(&func->callbacks, callbacks, sizeof(func->callbacks));
    func->name = name;
    return symresolver_register_symbol(compiler, name, SYMBOL_TYPE_NATIVE_FUNCTION, func);
}

struct native_function* native_function_get(struct compile_process* compiler, const char* name)
{
    struct symbol* sym = symresolver_get_symbol_for_native_function(compiler, name);
    if (!sym)
    {
        return NULL;
    }

    return sym->data;
}