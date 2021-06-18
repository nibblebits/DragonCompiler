#include "compiler.h"
#include "helpers/vector.h"
#include <assert.h>

bool resolver_result_failed(struct resolver_result *result)
{
    return result->flags & RESOLVER_RESULT_FLAG_FAILED;
}

bool resolver_result_ok(struct resolver_result *result)
{
    return !resolver_result_failed(result);
}

bool resolver_result_finished(struct resolver_result *result)
{
    return result->flags & RESOLVER_RESULT_FLAG_RUNTIME_NEEDED_TO_FINISH_PATH;
}

bool resolver_entity_runtime_required(struct resolver_entity *entity)
{
    return !(entity->flags & RESOLVER_ENTITY_COMPILE_TIME_ENTITY);
}

struct resolver_entity *resolver_result_entity_root(struct resolver_result *result)
{
    return result->entity;
}

struct resolver_entity *resolver_result_entity_next(struct resolver_entity *entity)
{
    return entity->next;
}

struct resolver_entity *resolver_entity_clone(struct resolver_entity *entity)
{
    struct resolver_entity *new_entity = calloc(sizeof(struct resolver_entity), 1);
    memcpy(new_entity, entity, sizeof(struct resolver_entity));
    return new_entity;
}

struct resolver_entity *resolver_result_entity(struct resolver_result *result)
{
    if (resolver_result_failed(result))
        return NULL;

    return result->entity;
}

struct resolver_result *resolver_new_result(struct resolver_process *process)
{
    return calloc(sizeof(struct resolver_result), 1);
}

void resolver_result_free(struct resolver_result *result)
{
    free(result);
}

static void resolver_runtime_needed(struct resolver_result *result, struct resolver_entity *last_entity)
{
    result->entity = last_entity;
    result->flags &= ~RESOLVER_RESULT_FLAG_RUNTIME_NEEDED_TO_FINISH_PATH;
}

void resolver_result_entity_push(struct resolver_result *result, struct resolver_entity *entity)
{
    if (!result->first_entity_const)
    {
        result->first_entity_const = entity;
    }

    if (!result->last_entity)
    {
        result->entity = entity;
        result->last_entity = entity;
        return;
    }

    result->last_entity->next = entity;
    entity->prev = result->last_entity;
    result->last_entity = entity;
}

struct resolver_entity *resolver_result_peek(struct resolver_result *result)
{
    return result->last_entity;
}

struct resolver_entity *resolver_result_pop(struct resolver_result *result)
{
    struct resolver_entity *entity = result->last_entity;

    if (result->entity == result->last_entity)
    {
        result->entity = result->last_entity->prev;
        result->last_entity = result->last_entity->prev;
        return entity;
    }

    result->last_entity = result->last_entity->prev;
    return entity;
}


struct compile_process *resolver_compiler(struct resolver_process *process)
{
    return process->compiler;
}

struct resolver_scope *resolver_scope_current(struct resolver_process *process)
{
    return process->scope.current;
}

struct resolver_scope *resolver_scope_root(struct resolver_process *process)
{
    return process->scope.root;
}

static struct resolver_scope *resolver_new_scope_create()
{
    struct resolver_scope *scope = calloc(sizeof(struct resolver_scope), 1);
    scope->entities = vector_create(sizeof(struct resolver_entity *));
    return scope;
}

struct resolver_scope *resolver_new_scope(struct resolver_process *resolver, void *private)
{
    struct resolver_scope *scope = resolver_new_scope_create(resolver);
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

void resolver_finish_scope(struct resolver_process *resolver)
{
    struct resolver_scope *scope = resolver->scope.current;
    resolver->scope.current = scope->prev;
    resolver->callbacks.delete_scope(scope);
    free(scope);
}

struct resolver_process *resolver_new_process(struct compile_process *compiler, struct resolver_callbacks *callbacks)
{
    struct resolver_process *process = calloc(sizeof(struct resolver_process), 1);
    process->compiler = compiler;
    memcpy(&process->callbacks, callbacks, sizeof(process->callbacks));
    process->scope.root = resolver_new_scope_create();
    process->scope.current = process->scope.root;
    return process;
}

struct resolver_entity *resolver_create_new_entity(struct datatype dtype, void *private)
{
    struct resolver_entity *entity = calloc(sizeof(struct resolver_entity), 1);
    if (!entity)
        return NULL;

    entity->dtype = dtype;
    entity->private = private;

    return entity;
}

struct resolver_entity *resolver_create_new_entity_for_var_node(struct resolver_process *process, struct node *var_node, void *private)
{
    struct resolver_entity *entity = resolver_create_new_entity(var_node->var.type, private);
    if (!entity)
        return NULL;

    entity->scope = resolver_scope_current(process);
    assert(entity->scope);

    entity->node = var_node;
    return entity;
}

struct resolver_entity *resolver_new_entity_for_var_node(struct resolver_process *process, struct node *var_node, void *private)
{
    struct resolver_entity *entity = resolver_create_new_entity_for_var_node(process, var_node, private);
    if (!entity)
        return NULL;
    vector_push(process->scope.current->entities, &entity);
    return entity;
}

struct resolver_entity *resolver_get_variable_in_scope(const char *var_name, struct resolver_scope *scope)
{
    vector_set_peek_pointer_end(scope->entities);
    vector_set_flag(scope->entities, VECTOR_FLAG_PEEK_DECREMENT);
    struct resolver_entity *current = vector_peek_ptr(scope->entities);
    while (current)
    {
        if (S_EQ(current->node->var.name, var_name))
            break;

        current = vector_peek_ptr(scope->entities);
    }

    return current;
}

struct resolver_entity *resolver_get_variable(struct resolver_result* result, struct resolver_process *resolver, const char *var_name)
{
    struct resolver_entity *entity = NULL;
    struct resolver_scope *scope = resolver->scope.current;

    // We may need to get a variable from a structure if we are currently in a structure
    // during this resolusion
    if(result->last_struct_entity && result->last_struct_entity->node->var.type.type == DATA_TYPE_STRUCT)
    {
        struct node* out_node = NULL;
        int offset = struct_offset(resolver_compiler(resolver), node_var_type_str(result->last_struct_entity->node), var_name, &out_node, 0, 0);
        return resolver_new_entity_for_var_node(resolver, out_node, resolver->callbacks.new_struct_entity(result, out_node, offset, 0));
    }
    while (scope)
    {
        entity = resolver_get_variable_in_scope(var_name, scope);
        if (entity)
            break;

        scope = scope->prev;
    }

    return entity;
}

static void resolver_follow_part(struct resolver_process *resolver, struct node *node, struct resolver_result *result);

/*static struct resolver_entity *resolver_follow_struct_exp(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
     const char* type_str = NULL;
    if (resolver_result_peek(result))
    {
        type_str = resolver_result_peek(result)->dtype.type_str;
    }

    // Compute the struct offset until no longer possible and push the result
    struct struct_access_details details;
    struct node *access_node = struct_for_access(resolver, node, type_str, STRUCT_STOP_AT_POINTER_ACCESS, &details);
    
    struct resolver_entity* entity = resolver_create_new_entity_for_var_node(resolver, access_node, resolver->callbacks.new_struct_entity(access_node, &details, 0));
    resolver_result_entity_push(result, entity);
    
    if(details.flags & STRUCT_ACCESS_DETAILS_FLAG_NOT_FINISHED)
    {
        resolver_follow_part(resolver, details.next_node, result);
    }

    return entity;
}
*/

static struct resolver_entity *resolver_follow_struct_exp(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    struct resolver_entity* result_entity = NULL;
    resolver_follow_part(resolver, node->exp.left, result);
    resolver_follow_part(resolver, node->exp.right, result);

    // Pop off the left node and right node so we can merge the offsets.
    struct resolver_entity* right_entity = resolver_result_pop(result);
    struct resolver_entity* left_entity = resolver_result_pop(result);

    // Set the last structure entity that is known
    result->last_struct_entity = left_entity;

    if (S_EQ(node->exp.op, "->"))
    {
       resolver_result_entity_push(result, left_entity);
       resolver_result_entity_push(result, right_entity);
       result_entity = right_entity;
       return result_entity;
    }
    
    result_entity = resolver_new_entity_for_var_node(resolver, right_entity->node, resolver->callbacks.merge_struct_entity(result, left_entity, right_entity));
    // Push the right entity back to the stack as it has been merged with the left_entity
    resolver_result_entity_push(result, result_entity);

    return result_entity;
}

static struct resolver_entity* resolver_follow_array(struct resolver_process* resolver, struct node* node, struct resolver_result* result)
{  
    // Left entity is the variable prior to the array access i.e a[5]
    resolver_follow_part(resolver, node->exp.left, result);
    struct resolver_entity* left_entity = resolver_result_pop(result);
    // Now for the right entity, if its just a number then we can merge the offsets, otherwise
    // it will need to be computed at runtime

    struct node* right_operand = node->exp.right->bracket.inner;
    if (right_operand->type == NODE_TYPE_NUMBER)
    {
        struct resolver_entity* entity = resolver_create_new_entity_for_var_node(resolver, left_entity->node, resolver->callbacks.new_array_entity(result, left_entity, right_operand->llnum));
        resolver_result_entity_push(result, entity);
        return entity;
    }

    FAIL_ERR("Only static indexes supported right now");

}
static struct resolver_entity *resolver_follow_exp(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    struct resolver_entity *entity = NULL;
    if (is_access_node(node))
    {
        entity = resolver_follow_struct_exp(resolver, node, result);
    }
    else if(is_array_node(node))
    {
        entity = resolver_follow_array(resolver, node, result);
    }

    return entity;
}

struct resolver_entity* resolver_follow_identifier(struct resolver_process* resolver, struct node* node, struct resolver_result* result)
{
    struct resolver_entity* entity = resolver_get_variable(result, resolver, node->sval);
    resolver_result_entity_push(result, entity);

    if (entity->node->var.type.type == DATA_TYPE_STRUCT)
    {
        // Set the last structure entity that is known
        result->last_struct_entity = entity;
    }
    return entity;

}
static void resolver_follow_part(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    struct resolver_entity *entity = NULL;
    switch (node->type)
    {
    case NODE_TYPE_IDENTIFIER:
        entity = resolver_follow_identifier(resolver, node, result);
        break;

    case NODE_TYPE_EXPRESSION:
        entity = resolver_follow_exp(resolver, node, result);
        break;
        
    }

    if (entity)
    {
        entity->result = result;
        entity->resolver = resolver;
    }
}

struct resolver_result *resolver_follow(struct resolver_process *resolver, struct node *node)
{
    struct resolver_result *result = resolver_new_result(resolver);
    resolver_follow_part(resolver, node, result);
    if (!resolver_result_entity_root(result))
    {
        result->flags |= RESOLVER_RESULT_FLAG_FAILED;
    }
    return result;
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
struct datatype *resolver_get_datatype(struct node *node)
{
    switch (node->type)
    {
    case NODE_TYPE_IDENTIFIER:

        break;
    }
}