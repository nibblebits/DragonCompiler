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
    if (!entity)
        return NULL;

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
    return entity->var_data.array_runtime.multiplier > 1;
}

struct resolver_entity *resolver_create_new_entity(int type, void *private)
{
    struct resolver_entity *entity = calloc(sizeof(struct resolver_entity), 1);
    if (!entity)
        return NULL;

    entity->type = type;
    entity->private = private;

    return entity;
}

struct resolver_entity *resolver_create_new_entity_for_var_node_custom_scope(struct resolver_process *process, struct node *var_node, void *private, struct resolver_scope *scope)
{
    struct resolver_entity *entity = resolver_create_new_entity(RESOLVER_ENTITY_TYPE_VARIABLE, private);
    if (!entity)
        return NULL;

    entity->scope = scope;
    assert(entity->scope);
    entity->var_data.dtype = var_node->var.type;
    entity->node = var_node;
    entity->name = var_node->var.name;
    return entity;
}

struct resolver_entity *resolver_create_new_entity_for_var_node(struct resolver_process *process, struct node *var_node, void *private)
{
    return resolver_create_new_entity_for_var_node_custom_scope(process, var_node, private, resolver_scope_current(process));
}

struct resolver_entity *resolver_new_entity_for_var_node_no_push(struct resolver_process *process, struct node *var_node, void *private)
{
    struct resolver_entity *entity = resolver_create_new_entity_for_var_node(process, var_node, private);
    if (!entity)
        return NULL;

    if (resolver_process_scope_current(process)->flags & RESOLVER_SCOPE_FLAG_IS_STACK)
    {
        entity->flags |= RESOLVER_ENTITY_FLAG_IS_STACK;
    }
    return entity;
}
struct resolver_entity *resolver_new_entity_for_var_node(struct resolver_process *process, struct node *var_node, void *private)
{

    struct resolver_entity *entity = resolver_new_entity_for_var_node_no_push(process, var_node, private);
    vector_push(process->scope.current->entities, &entity);
    return entity;
}

struct resolver_entity *resolver_create_new_entity_for_function_call(struct resolver_process *process, struct node *func_node, void *private)
{
    struct resolver_entity *entity = resolver_create_new_entity(RESOLVER_ENTITY_TYPE_FUNCTION_CALL, private);
    if (!entity)
    {
        return NULL;
    }

    entity->name = func_node->func.name;
    entity->func_call_data.arguments = vector_create(sizeof(struct node *));
    return entity;
}

struct resolver_entity *resolver_register_function(struct resolver_process *process, struct node *func_node, void *private)
{
    struct resolver_entity *entity = resolver_create_new_entity(RESOLVER_ENTITY_TYPE_FUNCTION, private);
    if (!entity)
        return NULL;

    entity->name = func_node->func.name;
    entity->node = func_node;
    // Functions must be on the root most scope
    vector_push(process->scope.root->entities, &entity);
    return entity;
}

struct resolver_entity *resolver_get_entity_in_scope_with_entity_type(struct resolver_result *result, struct resolver_process *resolver, struct resolver_scope *scope, const char *entity_name, int entity_type)
{
    // If we have a last structure entity, then they have to be asking for a variable
    // if they are not then theirs a bug here.
    assert(!result->last_struct_union_entity || entity_type == -1 || entity_type == RESOLVER_ENTITY_TYPE_VARIABLE);

    // If we have a last struct entity set then we must be accessing a structure
    // i.e a.b.c therefore we can assume a variable type since structures can only hold variables
    // and other structures that are named with variables.
    if (result->last_struct_union_entity && node_is_struct_or_union_variable(result->last_struct_union_entity->node))
    {
        struct resolver_scope *scope = result->last_struct_union_entity->scope;
        struct node *out_node = NULL;
        struct datatype *node_var_datatype = &variable_node(result->last_struct_union_entity->node)->var.type;

        // Unions offset will always be zero ;)
        int offset = struct_offset(resolver_compiler(resolver), node_var_type_str(variable_node(result->last_struct_union_entity->node)), entity_name, &out_node, 0, 0);
        if (node_var_datatype->type == DATA_TYPE_UNION)
        {
            // Union offset will be zero.
            offset = 0;
        }
        return resolver_new_entity_for_var_node_no_push(resolver, out_node, resolver->callbacks.new_struct_entity(result, variable_node(out_node), offset, scope));
    }

    // Ok this is not a structure variable, lets search the scopes
    // until we can identify this entity
    vector_set_peek_pointer_end(scope->entities);
    vector_set_flag(scope->entities, VECTOR_FLAG_PEEK_DECREMENT);
    struct resolver_entity *current = vector_peek_ptr(scope->entities);
    while (current)
    {
        // We only care about the given entity type, i.e variable, function structure what ever it is.
        if (entity_type != -1 && current->type != entity_type)
        {
            current = vector_peek_ptr(scope->entities);
            continue;
        }

        if (S_EQ(current->name, entity_name))
        {
            break;
        }

        current = vector_peek_ptr(scope->entities);
    }

    return current;
}

struct resolver_entity *resolver_get_entity_for_type(struct resolver_result *result, struct resolver_process *resolver, const char *entity_name, int entity_type)
{
    struct resolver_scope *scope = resolver->scope.current;
    struct resolver_entity *entity = NULL;
    while (scope)
    {
        entity = resolver_get_entity_in_scope_with_entity_type(result, resolver, scope, entity_name, entity_type);
        if (entity)
            break;

        scope = scope->prev;
    }

    if (entity)
    {
        // Clear the last resolve data as we resolved this node again.
        memset(&entity->last_resolve, 0, sizeof(entity->last_resolve));
    }
    return entity;
}

struct resolver_entity *resolver_get_entity(struct resolver_result *result, struct resolver_process *resolver, const char *entity_name)
{
    return resolver_get_entity_for_type(result, resolver, entity_name, -1);
}

struct resolver_entity *resolver_get_entity_in_scope(struct resolver_result *result, struct resolver_process *resolver, struct resolver_scope *scope, const char *entity_name)
{
    return resolver_get_entity_in_scope_with_entity_type(result, resolver, scope, entity_name, -1);
}

struct resolver_entity *resolver_get_variable(struct resolver_result *result, struct resolver_process *resolver, const char *var_name)
{
    struct resolver_entity *entity = resolver_get_entity_for_type(result, resolver, var_name, RESOLVER_ENTITY_TYPE_VARIABLE);
    return entity;
}

struct resolver_entity *resolver_get_function_in_scope(struct resolver_result *result, struct resolver_process *resolver, const char *func_name, struct resolver_scope *scope)
{
    return resolver_get_entity_for_type(result, resolver, func_name, RESOLVER_ENTITY_TYPE_FUNCTION);
}

struct resolver_entity *resolver_get_function(struct resolver_result *result, struct resolver_process *resolver, const char *func_name)
{
    struct resolver_entity *entity = NULL;
    // Functions can only be in the root scope, its not possible in C to have sub functions.
    // No OOP exists.
    struct resolver_scope *scope = resolver->scope.root;
    entity = resolver_get_function_in_scope(result, resolver, func_name, scope);
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
        result_entity = resolver_new_entity_for_var_node_no_push(resolver, left_entity->node, resolver->callbacks.merge_struct_entity(result, extra_entity, left_entity, var_scope));
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

    result_entity = resolver_new_entity_for_var_node_no_push(resolver, right_entity->node, resolver->callbacks.merge_struct_entity(result, left_entity, right_entity, var_scope));

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

    struct resolver_scope *scope = left_entity->scope;

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
            entity->var_data.array_runtime.index_node = right_operand;
            entity->var_data.array_runtime.multiplier = array_multiplier(&entity->var_data.dtype, last_array_index, 1);
        }
        else
        {
            // This needs to be computed at runtime.
            entity = left_entity;
            entity->flags |= RESOLVER_ENTITY_FLAG_ARRAY_FOR_RUNTIME;
            // Set the index node expression so caller knows how to resolve this.
            entity->var_data.array_runtime.index_node = right_operand;
            entity->var_data.array_runtime.multiplier = array_multiplier(&entity->var_data.dtype, last_array_index, 1);
        }
    }
    else if (left_entity->flags & RESOLVER_ENTITY_FLAG_ARRAY_FOR_RUNTIME)
    {
        resolver->callbacks.join_array_entity_index(result, left_entity, right_operand->llnum, last_array_index);
        entity = left_entity;
    }
    else if (right_operand->type == NODE_TYPE_NUMBER)
    {
        entity = resolver_create_new_entity_for_var_node_custom_scope(resolver, variable_node(result->identifier->node), resolver->callbacks.new_array_entity(result, left_entity, right_operand->llnum, last_array_index, scope), scope);
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

static struct resolver_entity *resolver_follow_exp(struct resolver_process *resolver, struct node *node, struct resolver_result *result);

static void resolver_build_function_call_arguments(struct resolver_process *resolver, struct node *argument_node, struct resolver_entity *root_func_call_entity, size_t *total_size_out)
{
    if (is_argument_node(argument_node))
    {
        // Ok we have an expression i.e 50, 20
        // Build the left node for this function argument
        resolver_build_function_call_arguments(resolver, argument_node->exp.left, root_func_call_entity, total_size_out);
        // Build the right node for this function argument
        resolver_build_function_call_arguments(resolver, argument_node->exp.right, root_func_call_entity, total_size_out);
    }
    else if (argument_node->type == NODE_TYPE_EXPRESSION_PARENTHESIS)
    {
        resolver_build_function_call_arguments(resolver, argument_node->parenthesis.exp, root_func_call_entity, total_size_out);
    }
    else
    {
        // We must now push this node to the output vector
        vector_push(root_func_call_entity->func_call_data.arguments, &argument_node);
        // It will use 4 bytes on the stack
        *total_size_out += DATA_SIZE_DWORD;
    }
}

static struct resolver_entity *resolver_follow_function_call(struct resolver_process *resolver, struct resolver_result *result, struct node *node)
{
    // Ok this is a function call, left operand = function name, right operand = arguments
    const char *func_name = node->exp.left->sval;
    struct resolver_entity *entity = resolver_get_function(result, resolver, func_name);
    assert(entity);

    // As this is a function all we must create a new function call entity, for this given function call
    struct resolver_entity *func_call_entity = resolver_create_new_entity_for_function_call(resolver, entity->node, NULL);
    assert(func_call_entity);

    // Let's build the function call arguments
    resolver_build_function_call_arguments(resolver, node->exp.right, func_call_entity, &func_call_entity->func_call_data.stack_size);

    //Push the function call entity to the stack
    resolver_result_entity_push(result, func_call_entity);

    return func_call_entity;
}
static struct resolver_entity *resolver_follow_parentheses(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    // This may be a function call is the left operand an identifier?
    if (node->exp.left->type == NODE_TYPE_IDENTIFIER)
    {
        return resolver_follow_function_call(resolver, result, node);
    }
    // This is a normal expression, process it
    return resolver_follow_exp(resolver, node->parenthesis.exp, result);
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
    else if (is_parentheses_node(node))
    {
        entity = resolver_follow_parentheses(resolver, node, result);
    }
    return entity;
}

struct resolver_entity *resolver_follow_for_name(struct resolver_process *resolver, const char *name, struct resolver_result *result)
{
    struct resolver_entity *entity = resolver_entity_clone(resolver_get_entity(result, resolver, name));
    if (!entity)
    {
        return NULL;
    }

    resolver_result_entity_push(result, entity);

    // If we dont have an identifier entity yet then set it
    // This identifier will act as the first found identifier, to be used as a baseline.
    if (!result->identifier)
    {
        result->identifier = entity;
    }

    if (entity->type == RESOLVER_ENTITY_TYPE_VARIABLE && datatype_is_struct_or_union(&entity->var_data.dtype))
    {
        result->last_struct_union_entity = entity;
    }
    return entity;
}
struct resolver_entity *resolver_follow_identifier(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    struct resolver_entity *entity = resolver_follow_for_name(resolver, node->sval, result);
    if (entity)
    {
        entity->last_resolve.referencing_node = node;
    }
    return entity;
}

struct resolver_entity *resolver_follow_variable(struct resolver_process *resolver, struct node *var_node, struct resolver_result *result)
{
    struct resolver_entity *entity = resolver_follow_for_name(resolver, var_node->var.name, result);
    return entity;
}

static struct resolver_entity *resolver_follow_part_return_entity(struct resolver_process *resolver, struct node *node, struct resolver_result *result);

static struct resolver_entity *resolver_follow_exp_parenthesis(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    return resolver_follow_part_return_entity(resolver, node->parenthesis.exp, result);
}

static struct resolver_entity *resolver_follow_unary_exp(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    struct resolver_entity *entity = resolver_follow_part_return_entity(resolver, node->unary.operand, result);
    if (entity)
    {
        entity->last_resolve.unary = &node->unary;
    }
    return entity;
}

static struct resolver_entity *resolver_follow_part_return_entity(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    struct resolver_entity *entity = NULL;
    switch (node->type)
    {
    case NODE_TYPE_IDENTIFIER:
        entity = resolver_follow_identifier(resolver, node, result);
        break;

    case NODE_TYPE_VARIABLE:
        entity = resolver_follow_variable(resolver, node, result);
        break;
    case NODE_TYPE_EXPRESSION:
        entity = resolver_follow_exp(resolver, node, result);
        break;

    case NODE_TYPE_EXPRESSION_PARENTHESIS:
        entity = resolver_follow_exp_parenthesis(resolver, node, result);
        break;

    case NODE_TYPE_UNARY:
        entity = resolver_follow_unary_exp(resolver, node, result);
        break;
    }

    if (entity)
    {
        entity->result = result;
        entity->resolver = resolver;
    }

    return entity;
}

static void resolver_follow_part(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    resolver_follow_part_return_entity(resolver, node, result);
}

struct resolver_result *resolver_follow(struct resolver_process *resolver, struct node *node)
{
    assert(resolver);
    assert(node);
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