
#include <stdlib.h>
#include "compiler.h"

static void symresolver_push_symbol(struct compile_process* process, struct symbol* sym)
{
    vector_push(process->symbol_tbl, &sym);
}

struct symbol* symresolver_get_symbol(struct compile_process* process, const char* name)
{    
    vector_set_peek_pointer(process->symbol_tbl, 0);

    struct symbol* symbol = vector_peek_ptr(process->symbol_tbl);
    while(symbol)
    {

        if (S_EQ(symbol->name, name))
        {
            break;
        }

        symbol = vector_peek_ptr(process->symbol_tbl);
    }

    return symbol;
}

struct symbol* symresolver_register_symbol(struct compile_process* process, const char* sym_name, int type, void* data)
{
    struct symbol* sym = calloc(sizeof(struct symbol), 1);
    sym->name = sym_name;
    sym->type = type;
    sym->data = data;
    symresolver_push_symbol(process, sym);
    return sym;
}

struct node* symresolver_node(struct symbol* sym)
{
    if (sym->type != SYMBOL_TYPE_NODE)
    {
        return NULL;
    }

    return sym->data;
}

void symresolver_build_for_variable_node(struct compile_process* process, struct node* node)
{
    symresolver_register_symbol(process, node->var.name, SYMBOL_TYPE_NODE, node);
}

void symresolver_build_for_function_node(struct compile_process* process, struct node* node)
{
    symresolver_register_symbol(process, node->func.name, SYMBOL_TYPE_NODE, node);
}

void symresolver_build_for_structure_node(struct compile_process* process, struct node* node)
{
    symresolver_register_symbol(process, node->_struct.name, SYMBOL_TYPE_NODE, node);
}

void symresolver_build_for_node(struct compile_process* process, struct node* node)
{
    switch(node->type)
    {
        case NODE_TYPE_VARIABLE:
            symresolver_build_for_variable_node(process, node);
        break;

        case NODE_TYPE_FUNCTION:
            symresolver_build_for_function_node(process, node);
        break;

        case NODE_TYPE_STRUCT:
            symresolver_build_for_structure_node(process, node);
        break;
    
        // Ignore all other nodes they are not able to become symbols..
    }
}