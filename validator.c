#include "compiler.h"

static struct compile_process* validator_current_compile_process;
static struct node* current_function;


void validation_new_scope(int flags)
{
    resolver_default_new_scope(validator_current_compile_process->resolver, flags);
}

void validation_end_scope()
{
    resolver_default_finish_scope(validator_current_compile_process->resolver);
}

struct node* validation_next_tree_node()
{
    return vector_peek_ptr(validator_current_compile_process->node_tree_vec);
}

void validate_symbol_unique(const char* name, const char* type_of_symbol, struct node* node)
{
    struct symbol* sym = symresolver_get_symbol(validator_current_compile_process, name);
    if(sym)
    {
        compiler_node_error(node, "Cannot define %s you have already defined a symbol with the name \"%s\"", type_of_symbol, name);
    }
}


void validate_structure_node(struct node* node)
{
    if (!(node->flags & NODE_FLAG_IS_FORWARD_DECLARATION))
    {
        validate_symbol_unique(node->_struct.name, "struct", node);
    }
    symresolver_register_symbol(validator_current_compile_process, node->_struct.name, SYMBOL_TYPE_NODE, node);
}

void validate_union_node(struct node* node)
{
    
    if (!(node->flags & NODE_FLAG_IS_FORWARD_DECLARATION))
    {
       validate_symbol_unique(node->_union.name, "union", node);
    }
    symresolver_register_symbol(validator_current_compile_process, node->_union.name, SYMBOL_TYPE_NODE, node);
}

void validate_variable(struct node* var_node)
{
    struct resolver_entity* entity = resolver_get_variable_from_local_scope(validator_current_compile_process->resolver, var_node->var.name);
    if (entity)
    {
        compiler_node_error(var_node, "You already defined the variable %s in the given scope", var_node->var.name);
    }

    // Add the variable to the scope
    resolver_default_new_scope_entity(validator_current_compile_process->resolver, var_node, 0, 0);
}

void validate_function_argument(struct node* func_argument_var_node)
{
    validate_variable(func_argument_var_node);
}

void validate_function_arguments(struct function_arguments* func_arguments)
{
    struct vector* func_arg_vec = func_arguments->vector;
    vector_set_peek_pointer(func_arg_vec, 0);
    struct node *current = vector_peek_ptr(func_arg_vec);
    while(current)
    {
        validate_function_argument(current);
        current = vector_peek_ptr(func_arg_vec);
    }
}

void validate_identifier(struct node* node)
{
    struct resoler_result* result = resolver_follow(validator_current_compile_process->resolver, node);
    if (!resolver_result_ok(result))
    {
        compiler_error(validator_current_compile_process, "The variable does not exist");
    }
}
void validate_expressionable(struct node* node)
{

    // Alright we have an assignment lets just make sure that the variable exists right.
    switch(node->type)
    {
        case NODE_TYPE_IDENTIFIER:
            validate_identifier(node);
        break;
    }
    
}

void validate_return_node(struct node* node)
{
    if (node->stmt.ret.exp)
    {
        if (datatype_is_void_no_ptr(&current_function->func.rtype))
        {
            // Why are we returning a value in a void return type
            compiler_node_error(node, "You are returning a value in function %s which has a return type of void", current_function->func.name);
        }
        validate_expressionable(node->stmt.ret.exp);
    }
}

void validate_if_stmt(struct node* node)
{   
    validation_new_scope(0);
    validate_body(&node->stmt._if.body_node->body);
    validation_end_scope();
}


void validate_statement(struct node* node)
{
    switch(node->type)
    {
        case NODE_TYPE_VARIABLE:
        validate_variable(node);
        break;

        case NODE_TYPE_STATEMENT_RETURN:
        validate_return_node(node);
        break;  

        case NODE_TYPE_STATEMENT_IF:
        validate_if_stmt(node);
        break;
    }
}

void validate_body(struct body* body)
{
    vector_set_peek_pointer(body->statements, 0);
    struct node* statement = vector_peek_ptr(body->statements);
    while(statement)
    {
        validate_statement(statement);
        statement = vector_peek_ptr(body->statements);
    }   
}

void validate_function_body(struct node* node)
{
    validate_body(&node->body);
}
void validate_function_node(struct node* node)
{
    current_function = node;
    
    if (!(node->flags & NODE_FLAG_IS_FORWARD_DECLARATION))
    {
        validate_symbol_unique(node->func.name, "function", node);
    }

    symresolver_register_symbol(validator_current_compile_process, node->func.name, SYMBOL_TYPE_NODE, node);

    // We have a scope shares by arguments and body
    validation_new_scope(0);
    validate_function_arguments(&node->func.args);
    if (node->func.body_n)
    {
       validate_function_body(node->func.body_n);
    }
    validation_end_scope();
    current_function = NULL;
}


void validate_node(struct node* node)
{
    switch(node->type)
    {
        case NODE_TYPE_FUNCTION:
        validate_function_node(node);
        break;

        case NODE_TYPE_STRUCT:
        validate_structure_node(node);
        break;

        case NODE_TYPE_UNION:
        validate_union_node(node);
        break;
        case NODE_TYPE_STATEMENT_RETURN:
        validate_return_node(node);
        break;
    }
}

int validate_tree()
{
    // We have a global scope
    validation_new_scope(0);
    struct node* node = validation_next_tree_node();
    while(node)
    {
        validate_node(node);
        node = validation_next_tree_node();
    }
    validation_end_scope();
    return VALIDATION_ALL_OK;
}

void validate_initialize(struct compile_process* process)
{
    validator_current_compile_process = process;
    vector_set_peek_pointer(process->node_tree_vec, 0);
    
    symresolver_new_table(process);
}

void validate_destruct(struct compile_process* process)
{
    symresolver_end_table(process);
    vector_set_peek_pointer(process->node_tree_vec, 0);
}

int validate(struct compile_process* process)
{
    int res = 0;
    validate_initialize(process);
    res = validate_tree(process);
    validate_destruct(process);
    return res;
}