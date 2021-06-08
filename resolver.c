#include "compiler.h"
#include "helpers/vector.h"
#include <assert.h>
struct compile_process* resolver_compiler(struct resolver_process* process)
{
    return process->compiler;
}

struct resolver_scope* resolver_scope_current(struct resolver_process* process)
{
    return process->scope.current;
}

struct resolver_scope* resolver_scope_root(struct resolver_process* process)
{
    return process->scope.root;
}

static struct resolver_scope* resolver_new_scope_create()
{
    struct resolver_scope* scope = calloc(sizeof(struct resolver_scope), 1);
    scope->entities = vector_create(sizeof(struct resolver_entity*));
    return scope;
}

struct resolver_scope* resolver_new_scope(struct resolver_process* resolver, void* private)
{
    struct resolver_scope* scope = resolver_new_scope_create(resolver);
    if (!scope)
    {
        return NULL;
    }

    resolver->scope.current->next = scope;
    scope->prev = resolver->scope.current;
    resolver->scope.current = scope;
    scope->private = private;
    return scope;
}

void resolver_finish_scope(struct resolver_process* resolver)
{
    struct resolver_scope* scope = resolver->scope.current;
    resolver->scope.current = scope->prev;
    resolver->callbacks.delete_scope(scope);
    free(scope);
}

struct resolver_process* resolver_new_process(struct compile_process* compiler, struct resolver_callbacks* callbacks)
{
    struct resolver_process* process = calloc(sizeof(struct resolver_process), 1);
    process->compiler = compiler;
    memcpy(&process->callbacks, callbacks, sizeof(process->callbacks));
    process->scope.root = resolver_new_scope_create();
    process->scope.current = process->scope.root;
    return process;
}


struct resolver_entity* resolver_create_new_entity(struct datatype dtype, void* private)
{
    struct resolver_entity* entity = calloc(sizeof(struct resolver_entity), 1);
    if (!entity)
        return NULL;

    entity->dtype = dtype;
    entity->private = private;
    
    return entity;
}

struct resolver_entity* resolver_create_new_entity_for_var_node(struct resolver_process* process, struct node* var_node, void* private)
{
    struct resolver_entity* entity = resolver_create_new_entity(var_node->var.type, private);
    if (!entity)
        return NULL;
    

    entity->scope = resolver_scope_current(process);
    assert(entity->scope);

    entity->node = var_node;
    return entity;
}

struct resolver_entity* resolver_new_entity_for_var_node(struct resolver_process* process, struct node* var_node, void* private)
{
    struct resolver_entity* entity = resolver_create_new_entity_for_var_node(process, var_node, private);
    if (!entity)
        return NULL;
    vector_push(process->scope.current->entities, &entity);
    return entity;
}

struct resolver_entity* resolver_get_variable_in_scope(const char* var_name, struct resolver_scope* scope)
{
    vector_set_peek_pointer_end(scope->entities);
    vector_set_flag(scope->entities, VECTOR_FLAG_PEEK_DECREMENT);
    struct resolver_entity* current = vector_peek_ptr(scope->entities);
    while(current)
    {
        if (S_EQ(current->node->var.name, var_name))
            break;

        current = vector_peek_ptr(scope->entities);
    }

    return current;
}

struct resolver_entity* resolver_get_variable(struct resolver_process* resolver, const char* var_name)
{
    struct resolver_entity* entity = NULL;
    struct resolver_scope* scope = resolver->scope.current;
    while(scope)
    {
        entity = resolver_get_variable_in_scope(var_name, scope);
        if (entity)
            break;

        scope = scope->prev;
    }

    return entity;
}

static struct resolver_entity *_resolver_get_variable_for_node(struct resolver_process* resolver, struct node *node)
{

    struct resolver_entity *entity = NULL;
    switch (node->type)
    {
    case NODE_TYPE_EXPRESSION:
        if (is_access_operator(node->exp.op))
        {
            struct resolver_entity* struct_left_entity = _resolver_get_variable_for_node(resolver, node->exp.left);
            if (!struct_left_entity)
            {
                // Cannot find scope entity? Perhaps its a global variable
                return NULL;
            }
            int offset = 0;
            struct node *access_node = struct_for_access(resolver_compiler(resolver), node->exp.right, struct_left_entity->node->var.type.type_str, &offset, 0);
            entity = resolver_create_new_entity_for_var_node(resolver, access_node, resolver->callbacks.new_struct_entity(access_node, struct_left_entity, offset));
          
            break;
        }

        entity = _resolver_get_variable_for_node(resolver, node->exp.left);
        break;

    case NODE_TYPE_EXPRESSION_PARENTHESIS:
        entity = _resolver_get_variable_for_node(resolver, node->parenthesis.exp);
        break;

    case NODE_TYPE_UNARY:
        entity = _resolver_get_variable_for_node(resolver, node->unary.operand);
        break;

    case NODE_TYPE_IDENTIFIER:
        entity = resolver_get_variable(resolver, node->sval);
        break;
    }

    return entity;
}


struct resolver_entity *resolver_get_variable_for_node(struct resolver_process* resolver, struct node *node)
{
    return _resolver_get_variable_for_node(resolver, node);
}   

/**
 * Attempts to peek through the tree at the given node and looks for a datatype
 * that can be associated with the node entity.
 * 
 * For example if you had an array and you did array[50].
 * 
 * If you passed the array node here the datatype of "array" would be returned.
 * 
 * If you call this function on a function call then the return type of the function call will be returned
 * the deepest possible type will be returned.
 */
struct datatype* resolver_get_datatype(struct node* node)
{
    switch(node->type)
    {
        case NODE_TYPE_IDENTIFIER:

        break;
    }
}