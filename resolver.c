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
    return !(entity->flags & RESOLVER_ENTITY_FLAG_COMPILE_TIME_ENTITY);
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
    struct resolver_result *result = calloc(sizeof(struct resolver_result), 1);
    result->array_data.array_entities = vector_create(sizeof(struct resolver_entity *));
    return result;
}

void resolver_result_free(struct resolver_result *result)
{
    vector_free(result->array_data.array_entities);
    free(result);
}

static struct resolver_scope *resolver_process_scope_current(struct resolver_process *process)
{
    return process->scope.current;
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

static struct vector *resolver_array_data_vec(struct resolver_result *result)
{
    return result->array_data.array_entities;
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

struct resolver_scope *resolver_new_scope(struct resolver_process *resolver, void *private, int flags)
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
    scope->flags = flags;
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

bool resolver_entity_has_array_multiplier(struct resolver_entity *entity)
{
    return entity->array_runtime.multiplier > 1;
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

    if (resolver_process_scope_current(process)->flags & RESOLVER_SCOPE_FLAG_IS_STACK)
    {
        entity->flags |= RESOLVER_ENTITY_FLAG_IS_STACK;
    }
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

struct resolver_entity *resolver_get_variable(struct resolver_result *result, struct resolver_process *resolver, const char *var_name)
{
    struct resolver_entity *entity = NULL;
    struct resolver_scope *scope = NULL;

    // We may need to get a variable from a structure if we are currently in a structure
    // during this resolusion
    if (result->last_struct_entity && result->last_struct_entity->node->var.type.type == DATA_TYPE_STRUCT)
    {
        scope = result->last_struct_entity->scope;
        struct node *out_node = NULL;
        int offset = struct_offset(resolver_compiler(resolver), node_var_type_str(result->last_struct_entity->node), var_name, &out_node, 0, 0);
        return resolver_new_entity_for_var_node(resolver, out_node, resolver->callbacks.new_struct_entity(result, out_node, offset, scope));
    }

    scope = resolver->scope.current;
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

static struct resolver_entity *resolver_follow_struct_exp(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    struct resolver_entity *result_entity = NULL;

    resolver_follow_part(resolver, node->exp.left, result);
    resolver_follow_part(resolver, node->exp.right, result);

    // Pop off the left node and right node so we can merge the offsets.
    struct resolver_entity *right_entity = resolver_result_pop(result);
    struct resolver_entity *left_entity = resolver_result_pop(result);

    struct resolver_scope *var_scope = result->identifier->scope;

    if (is_array_node(node->exp.left) && is_access_node_with_op(node->exp.right, "->"))
    {
        struct resolver_entity *extra_entity = resolver_result_pop(result);
        result_entity = resolver_new_entity_for_var_node(resolver, left_entity->node, resolver->callbacks.merge_struct_entity(result, extra_entity, left_entity, var_scope));
        resolver_result_entity_push(result, result_entity);
        resolver_result_entity_push(result, right_entity);

        return result_entity;
    }

    if (S_EQ(node->exp.op, "->"))
    {
        resolver_result_entity_push(result, left_entity);
        resolver_result_entity_push(result, right_entity);
        result_entity = right_entity;
        return result_entity;
    }

    result_entity = resolver_new_entity_for_var_node(resolver, right_entity->node, resolver->callbacks.merge_struct_entity(result, left_entity, right_entity, var_scope));

    // Push the right entity back to the stack as it has been merged with the left_entity
    resolver_result_entity_push(result, result_entity);

    return result_entity;
}

static void resolver_array_push(struct resolver_result *result, struct resolver_entity *entity)
{
    resolver_result_entity_push(result, entity);
    vector_push(resolver_array_data_vec(result), &entity);
}

static struct resolver_entity *resolver_follow_array(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    bool first_array_bracket = result->flags & RESOLVER_RESULT_FLAG_PROCESSING_ARRAY_ENTITIES;

    // Left entity is the variable prior to the array access i.e a[5]
    resolver_follow_part(resolver, node->exp.left, result);
    struct resolver_entity *left_entity = resolver_result_pop(result);

    struct resolver_entity *entity = NULL;
    int last_array_index = vector_count(resolver_array_data_vec(result));
    struct node *right_operand = node->exp.right->bracket.inner;

    //[[[a[]z][]z][]1]]

    if (right_operand->type != NODE_TYPE_NUMBER)
    {
        if (left_entity->flags & RESOLVER_ENTITY_FLAG_ARRAY_FOR_RUNTIME)
        {
            // We have left entity as an array for runtime as well, therefore we must
            // maintain left_entity as a seperate instance
            resolver_array_push(result, left_entity);

            entity = resolver_entity_clone(left_entity);  
            
            entity->flags |= RESOLVER_ENTITY_FLAG_ARRAY_FOR_RUNTIME;
            // Set the index node expression so caller knows how to resolve this.
            entity->array_runtime.index_node = right_operand;
            entity->array_runtime.multiplier = array_multiplier(&entity->dtype, last_array_index, 1);
            
        }
        else
        {
            // This needs to be computed at runtime.
            entity = left_entity;
            entity->flags |= RESOLVER_ENTITY_FLAG_ARRAY_FOR_RUNTIME;
            // Set the index node expression so caller knows how to resolve this.
            entity->array_runtime.index_node = right_operand;
            entity->array_runtime.multiplier = array_multiplier(&entity->dtype, last_array_index, 1);
        }
    }
    else if (left_entity->flags & RESOLVER_ENTITY_FLAG_ARRAY_FOR_RUNTIME)
    {
        resolver->callbacks.join_array_entity_index(result, left_entity, right_operand->llnum, last_array_index);
        entity = left_entity;
    }
    else if (right_operand->type == NODE_TYPE_NUMBER)
    {
        entity = resolver_create_new_entity_for_var_node(resolver, result->identifier->node, resolver->callbacks.new_array_entity(result, left_entity, right_operand->llnum, last_array_index));
    }

    resolver_array_push(result, entity);
    //[[sdog.e][]1]]

    if (first_array_bracket)
    {
        // This is the very first array bracket.. As we are now finished
        // parsing the entire array lets clear the vector in case
        // theirs another array in this expression
        vector_clear(resolver_array_data_vec(result));
        result->flags &= ~RESOLVER_RESULT_FLAG_PROCESSING_ARRAY_ENTITIES;
    }
    return entity;
}
static struct resolver_entity *resolver_follow_exp(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    struct resolver_entity *entity = NULL;
    if (is_access_node(node))
    {
        entity = resolver_follow_struct_exp(resolver, node, result);
    }
    else if (is_array_node(node))
    {
        entity = resolver_follow_array(resolver, node, result);
    }

    return entity;
}

struct resolver_entity *resolver_follow_identifier(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    struct resolver_entity *entity = resolver_entity_clone(resolver_get_variable(result, resolver, node->sval));
    resolver_result_entity_push(result, entity);

    // If we dont have an identifier entity yet then set it
    // This identifier will act as the first found identifier, to be used as a baseline.
    if (!result->identifier)
    {
        result->identifier = entity;
    }

    if (entity->dtype.type == DATA_TYPE_STRUCT)
    {
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