#include "compiler.h"
#include "helpers/vector.h"
#include <assert.h>

static struct resolver_entity *resolver_follow_part_return_entity(struct resolver_process *resolver, struct node *node, struct resolver_result *result);
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
        result->count++;
        return;
    }

    // Since we have more than one entity the first entity must be obtained via referen
    result->last_entity->next = entity;
    entity->prev = result->last_entity;
    result->last_entity = entity;
    result->count++;
}

struct resolver_entity *resolver_result_peek(struct resolver_result *result)
{
    return result->last_entity;
}

struct resolver_entity *resolver_result_peek_ignore_rule_entity(struct resolver_result *result)
{
    struct resolver_entity *entity = resolver_result_peek(result);
    while (entity && entity->type == RESOLVER_ENTITY_TYPE_RULE)
    {
        entity = entity->prev;
    }

    return entity;
}

struct resolver_entity *resolver_result_pop(struct resolver_result *result)
{
    struct resolver_entity *entity = result->last_entity;
    if (result->entity == NULL)
    {
        return NULL;
    }

    if (result->entity == result->last_entity)
    {
        result->entity = result->last_entity->prev;
        result->last_entity = result->last_entity->prev;
        result->count--;
        goto out;
    }

    result->last_entity = result->last_entity->prev;
    result->count--;
out:
    if (result->count == 0)
    {
        // Popped the last element? Okay then reRESOLVER_ENTITY_FLAG_DO_INDIRECTIONe some things
        result->first_entity_const = NULL;
        result->last_entity = NULL;
        result->entity = NULL;
    }

    entity->prev = NULL;
    entity->next = NULL;
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
    // return entity->var_data.array_runtime.multiplier > 1;
}

struct resolver_entity *resolver_create_new_entity(struct resolver_result *result, int type, void *private)
{
    struct resolver_entity *entity = calloc(sizeof(struct resolver_entity), 1);
    if (!entity)
        return NULL;

    entity->type = type;
    entity->private = private;

    return entity;
}

struct resolver_entity *resolver_create_new_entity_for_unsupported_node(struct resolver_result *result, struct node *node)
{
    struct resolver_entity *entity = resolver_create_new_entity(result, RESOLVER_ENTITY_TYPE_UNSUPPORTED, NULL);
    if (!entity)
        return NULL;

    entity->node = node;
    // We are unsupported, we cannot merge.
    entity->flags = RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY | RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY;
    return entity;
}

struct resolver_entity *resolver_create_new_entity_for_array_bracket(struct resolver_result *result, struct resolver_process *process, struct node *node, struct node *array_index_node, int index, struct datatype *dtype, void *private, struct resolver_scope *scope)
{
    struct resolver_entity *entity = resolver_create_new_entity(result, RESOLVER_ENTITY_TYPE_ARRAY_BRACKET, private);
    if (!entity)
        return NULL;

    entity->scope = scope;
    assert(entity->scope);
    entity->name = NULL;
    entity->dtype = *dtype;
    entity->node = node;
    entity->array.index = index;
    entity->array.dtype = *dtype;
    entity->array.multiplier = array_multiplier(dtype, index, 1);
    entity->array.array_index_node = array_index_node;

    int array_index_val = 1;
    if (array_index_node->type == NODE_TYPE_NUMBER)
    {
        array_index_val = array_index_node->llnum;
    }
    entity->offset = array_offset(dtype, index, array_index_val);

    return entity;
}

struct resolver_entity *resolver_create_new_entity_for_merged_array_bracket(struct resolver_result *result, struct resolver_process *process, struct node *node, struct node *array_index_node, int index, struct datatype *dtype, void *private, struct resolver_scope *scope)
{
    struct resolver_entity *entity = resolver_create_new_entity(result, RESOLVER_ENTITY_TYPE_ARRAY_BRACKET, private);
    if (!entity)
        return NULL;

    entity->scope = scope;
    assert(entity->scope);
    entity->name = NULL;
    entity->dtype = *dtype;
    entity->node = node;
    entity->array.index = index;
    entity->array.dtype = *dtype;
    entity->array.multiplier = array_multiplier(dtype, index, 1);
    entity->array.array_index_node = array_index_node;
    return entity;
}

struct resolver_entity *resolver_create_new_unknown_entity(struct resolver_process *process, struct resolver_result *result, struct datatype *dtype, struct node *node, struct resolver_scope *scope, int offset)
{
    struct resolver_entity *entity = resolver_create_new_entity(NULL, RESOLVER_ENTITY_TYPE_GENERAL, NULL);
    if (!entity)
        return NULL;

    entity->flags |= RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY | RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY;
    entity->scope = scope;
    entity->dtype = *dtype;
    entity->node = node;
    entity->offset = offset;
    return entity;
}

struct resolver_entity *resolver_create_new_unary_indirection_entity(struct resolver_process *process, struct resolver_result *result, struct node *node, int indirection_depth)
{
    struct resolver_entity *entity = resolver_create_new_entity(NULL, RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION, NULL);
    if (!entity)
        return NULL;

    entity->flags = RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY | RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY;
    entity->node = node;
    entity->indirection.depth = indirection_depth;
    return entity;
}

struct resolver_entity *resolver_create_new_unary_get_address_entity(struct resolver_process *process, struct resolver_result *result, struct datatype *dtype, struct node *node, struct resolver_scope *scope, int offset)
{
    struct resolver_entity *entity = resolver_create_new_entity(NULL, RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS, NULL);
    if (!entity)
        return NULL;

    // Address entity should not merge with any entity
    // once encountered we know we must get the address of the entity
    // THe stack should look like for &a.b.c
    // a - b - c - &
    // Joined to
    // c - &
    entity->flags = RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY | RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY;
    entity->node = node;
    entity->scope = scope;

    entity->dtype = *dtype;
    entity->dtype.flags |= DATATYPE_FLAG_IS_POINTER;
    entity->dtype.pointer_depth++;
    return entity;
}

struct resolver_entity *resolver_create_new_cast_entity(struct resolver_process *process, struct resolver_scope *scope, struct datatype *cast_dtype)
{
    struct resolver_entity *entity = resolver_create_new_entity(NULL, RESOLVER_ENTITY_TYPE_CAST, NULL);
    if (!entity)
        return NULL;

    entity->flags |= RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY | RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY;
    entity->scope = scope;
    entity->dtype = *cast_dtype;
    return entity;
}

struct resolver_entity *resolver_create_new_entity_for_var_node_custom_scope(struct resolver_process *process, struct node *var_node, void *private, struct resolver_scope *scope, int offset)
{
    assert(var_node->type == NODE_TYPE_VARIABLE);
    struct resolver_entity *entity = resolver_create_new_entity(NULL, RESOLVER_ENTITY_TYPE_VARIABLE, private);
    if (!entity)
        return NULL;

    entity->scope = scope;
    assert(entity->scope);
    entity->dtype = var_node->var.type;
    entity->var_data.dtype = var_node->var.type;
    entity->node = var_node;
    entity->name = var_node->var.name;
    entity->offset = offset;
    return entity;
}

struct resolver_entity *resolver_create_new_entity_for_var_node(struct resolver_process *process, struct node *var_node, void *private, int offset)
{
    return resolver_create_new_entity_for_var_node_custom_scope(process, var_node, private, resolver_scope_current(process), offset);
}

struct resolver_entity *resolver_new_entity_for_var_node_no_push(struct resolver_process *process, struct node *var_node, void *private, int offset, struct resolver_scope *scope)
{
    struct resolver_entity *entity = resolver_create_new_entity_for_var_node_custom_scope(process, var_node, private, scope, offset);
    if (!entity)
        return NULL;

    if (scope->flags & RESOLVER_SCOPE_FLAG_IS_STACK)
    {
        entity->flags |= RESOLVER_ENTITY_FLAG_IS_STACK;
    }

    return entity;
}

struct resolver_entity *resolver_new_entity_for_var_node(struct resolver_process *process, struct node *var_node, void *private, int offset)
{
    struct resolver_entity *entity = resolver_new_entity_for_var_node_no_push(process, var_node, private, offset, resolver_process_scope_current(process));
    vector_push(process->scope.current->entities, &entity);
    return entity;
}

void resolver_new_entity_for_rule(struct resolver_process *process, struct resolver_result *result, struct resolver_entity_rule *rule)
{
    struct resolver_entity *entity_rule = resolver_create_new_entity(result, RESOLVER_ENTITY_TYPE_RULE, NULL);
    entity_rule->rule = *rule;
    resolver_result_entity_push(result, entity_rule);
}

struct resolver_entity *resolver_make_entity(struct resolver_process *process, struct resolver_result *result, struct datatype *custom_dtype, struct node *node, struct resolver_entity *guided_entity, struct resolver_scope *scope)
{
    struct resolver_entity *entity = NULL;
    int offset = guided_entity->offset;
    int flags = guided_entity->flags;
    switch (node->type)
    {
    case NODE_TYPE_VARIABLE:
        entity = resolver_new_entity_for_var_node_no_push(process, node, NULL, offset, scope);
        break;

    default:
        entity = resolver_create_new_unknown_entity(process, result, custom_dtype, node, scope, offset);
    }

    if (entity)
    {
        entity->flags |= flags;
        if (custom_dtype)
        {
            entity->dtype = *custom_dtype;
        }
        entity->private = process->callbacks.make_private(entity, node, offset, scope);
    }
    return entity;
}

struct resolver_entity *resolver_create_new_entity_for_function_call(struct resolver_result *result, struct resolver_process *process, struct resolver_entity *left_operand_entity, void *private)
{
    struct resolver_entity *entity = resolver_create_new_entity(result, RESOLVER_ENTITY_TYPE_FUNCTION_CALL, private);
    if (!entity)
    {
        return NULL;
    }
    entity->dtype = left_operand_entity->dtype;
    entity->name = left_operand_entity->name;
    entity->func_call_data.arguments = vector_create(sizeof(struct node *));
    return entity;
}

struct resolver_entity *resolver_register_function(struct resolver_process *process, struct node *func_node, void *private)
{
    struct resolver_entity *entity = resolver_create_new_entity(NULL, RESOLVER_ENTITY_TYPE_FUNCTION, private);
    if (!entity)
        return NULL;

    entity->name = func_node->func.name;
    entity->node = func_node;
    entity->dtype = func_node->func.rtype;
    entity->scope = resolver_process_scope_current(process);
    // Functions must be on the root most scope
    vector_push(process->scope.root->entities, &entity);
    return entity;
}

struct resolver_entity *resolver_create_new_entity_for_native_function(struct resolver_process *process, const char *name, struct symbol *native_func_symbol)
{
    struct resolver_entity *entity = resolver_create_new_entity(NULL, RESOLVER_ENTITY_TYPE_NATIVE_FUNCTION, NULL);
    if (!entity)
        return NULL;
    entity->name = name;
    // Void return type
    datatype_set_void(&entity->dtype);
    make_function_node(&entity->dtype, name, NULL, NULL); 
    entity->node = node_pop();
    entity->name = name;
    entity->native_func.symbol = native_func_symbol;
    entity->scope = resolver_process_scope_current(process);
    // Functions must be on the root most scope
    vector_push(process->scope.root->entities, &entity);
    return entity;
}

struct resolver_entity *resolver_get_entity_in_scope_with_entity_type(struct resolver_result *result, struct resolver_process *resolver, struct resolver_scope *scope, const char *entity_name, int entity_type)
{
    // If we have a last structure entity, then they have to be asking for a variable
    // if they are not then theirs a bug here.
    assert(!result || !result->last_struct_union_entity || entity_type == -1 || entity_type == RESOLVER_ENTITY_TYPE_VARIABLE);

    // If we have a last struct entity set then we must be accessing a structure
    // i.e a.b.c therefore we can assume a variable type since structures can only hold variables
    // and other structures that are named with variables.
    if (result && result->last_struct_union_entity)
    {
        struct resolver_scope *scope = result->last_struct_union_entity->scope;
        struct node *out_node = NULL;
        struct datatype *node_var_datatype = &result->last_struct_union_entity->dtype;

        // Unions offset will always be zero ;)
        int offset = struct_offset(resolver_compiler(resolver), node_var_datatype->type_str, entity_name, &out_node, 0, 0);
        if (node_var_datatype->type == DATA_TYPE_UNION)
        {
            // Union offset will be zero.
            offset = 0;
        }
        return resolver_make_entity(resolver, result, NULL, out_node, &(struct resolver_entity){.type = RESOLVER_ENTITY_TYPE_VARIABLE, .offset = offset}, scope);
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

struct resolver_entity *resolver_get_variable_from_local_scope(struct resolver_process *resolver, const char *var_name)
{
    struct resolver_result *result = resolver_new_result(resolver);
    return resolver_get_entity_in_scope(result, resolver, resolver_scope_current(resolver), var_name);
}

struct resolver_entity *resolver_get_function_in_scope(struct resolver_result *result, struct resolver_process *resolver, const char *func_name, struct resolver_scope *scope)
{
    struct resolver_entity* entity =  resolver_get_entity_for_type(result, resolver, func_name, RESOLVER_ENTITY_TYPE_FUNCTION);
    if (!entity)
    {
        entity =  resolver_get_entity_for_type(result, resolver, func_name, RESOLVER_ENTITY_TYPE_NATIVE_FUNCTION);
    
    }
    return entity;
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

bool resolver_do_indirection(struct resolver_entity *entity)
{
    struct resolver_result *result = entity->result;
    return entity->type != RESOLVER_ENTITY_TYPE_FUNCTION_CALL && !(result->flags & RESOLVER_RESULT_FLAG_DOES_GET_ADDRESS) && entity->type != RESOLVER_ENTITY_TYPE_CAST;
}

static struct resolver_entity *resolver_follow_struct_exp(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    struct resolver_entity *result_entity = NULL;

    resolver_follow_part(resolver, node->exp.left, result);
    struct resolver_entity *left_entity = resolver_result_peek(result);
    struct resolver_entity_rule rule = {};
    if (is_access_node_with_op(node, "->"))
    {
        rule.left.flags = RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY;

        // Function calls return addresses of structures, no need for indirection
        // Obviously theirs no indirection if we are getting the address as we dont want to peek into the next pointer in this regard.
        if (resolver_do_indirection(left_entity))
        {
            rule.right.flags = RESOLVER_ENTITY_FLAG_DO_INDIRECTION;
        }
    }
    resolver_new_entity_for_rule(resolver, result, &rule);
    resolver_follow_part(resolver, node->exp.right, result);
    return NULL;
}

static void resolver_array_push(struct resolver_result *result, struct resolver_entity *entity)
{
    resolver_result_entity_push(result, entity);
    vector_push(resolver_array_data_vec(result), &entity);
}

static void resolver_array_bracket_set_flags(struct resolver_entity *bracket_entity, struct datatype *dtype, struct node *bracket_node, int index)
{
    if (!(dtype->flags & DATATYPE_FLAG_IS_ARRAY) || array_brackets_count(dtype) <= index)
    {
        bracket_entity->flags = RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY | RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY | RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY;
    }
    else if (bracket_node->bracket.inner->type != NODE_TYPE_NUMBER)
    {
        bracket_entity->flags = RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY | RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY;
    }
    else
    {
        // If its a number then we have already computed the entire offset, let the compiler know with a flag..
        bracket_entity->flags = RESOLVER_ENTITY_FLAG_JUST_USE_OFFSET;
    }
}

struct datatype *resolver_get_datatype(struct resolver_process *resolver, struct node *node)
{
    struct resolver_result *result = resolver_follow(resolver, node);
    if (!resolver_result_ok(result))
        return NULL;

    return &result->last_entity->dtype;
}

static struct resolver_entity *resolver_follow_array_bracket(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    // We must create some private data for this array bracket
    assert(node->type == NODE_TYPE_BRACKET);
    int index = 0;
    struct datatype dtype;
    struct resolver_scope *scope = NULL;
    struct resolver_entity *last_entity = resolver_result_peek_ignore_rule_entity(result);
    scope = last_entity->scope;
    dtype = last_entity->dtype;

    if (last_entity->type == RESOLVER_ENTITY_TYPE_ARRAY_BRACKET)
    {
        index = last_entity->array.index + 1;
    }

    // It is not always an array maybe its a pointer?
    if (dtype.flags & DATATYPE_FLAG_IS_ARRAY)
    {
        // We must readjust the datatype size because we have accessed a deeper part of the array
        dtype.array.size = array_brackets_calculate_size_from_index(&dtype, dtype.array.brackets, index + 1);
    }
    // We must reduce the dtype
    void *private = resolver->callbacks.new_array_entity(result, node);
    struct resolver_entity *array_bracket_entity = resolver_create_new_entity_for_array_bracket(result, resolver, node, node->bracket.inner, index, &dtype, private, scope);
    struct resolver_entity_rule rule = {};
    resolver_array_bracket_set_flags(array_bracket_entity, &dtype, node, index);
    // Last entity uses an array bracket
    last_entity->flags |= RESOLVER_ENTITY_FLAG_USES_ARRAY_BRACKETS;

    if (array_bracket_entity->flags & RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY)
    {
        // SInce we are accessing the pointer part of the array entity i.e
        // char* abc; abc[5] then we must adjust the datatype accordingly
        datatype_decrement_pointer(&array_bracket_entity->dtype);
    }
    // The array bracket must be pushed to the stack
    resolver_result_entity_push(result, array_bracket_entity);
    return array_bracket_entity;
}

static struct resolver_entity *resolver_follow_array(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    // Left entity is the variable/or other type of node prior to the array access i.e a[5]
    resolver_follow_part(resolver, node->exp.left, result);
    struct resolver_entity *left_entity = resolver_result_peek(result);
    resolver_follow_part(resolver, node->exp.right, result);
    return left_entity;
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
    else if (node_valid(argument_node))
    {
        // We must now push this node to the output vector
        vector_push(root_func_call_entity->func_call_data.arguments, &argument_node);
        size_t stack_change = DATA_SIZE_DWORD;
        struct datatype *dtype = resolver_get_datatype(resolver, argument_node);
        if (dtype)
        {
            // It will use 4 bytes on the stack unless its a structure
            stack_change = datatype_element_size(dtype);
            if (stack_change < DATA_SIZE_DWORD)
            {
                stack_change = DATA_SIZE_DWORD;
            }

            // We must round if appropiate to highest word
            stack_change = align_value(stack_change, DATA_SIZE_DWORD);
        }
        *total_size_out += stack_change;
    }
}

static struct resolver_entity *resolver_follow_function_call(struct resolver_process *resolver, struct resolver_result *result, struct node *node)
{
    // Ok this is a function call, left operand = function name or function pointer, right operand = arguments
    resolver_follow_part(resolver, node->exp.left, result);

    struct resolver_entity *left_entity = resolver_result_peek(result);
    // As this is a function all we must create a new function call entity, for this given function call
    struct resolver_entity *func_call_entity = resolver_create_new_entity_for_function_call(result, resolver, left_entity, NULL);
    assert(func_call_entity);
    func_call_entity->flags |= RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY | RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY;

    // Let's build the function call arguments
    resolver_build_function_call_arguments(resolver, node->exp.right, func_call_entity, &func_call_entity->func_call_data.stack_size);

    // Push the function call entity to the stack
    resolver_result_entity_push(result, func_call_entity);

    return func_call_entity;
}
static struct resolver_entity *resolver_follow_parentheses(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    // Need better way of checking if this is function call or not..
    if (node->exp.left->type == NODE_TYPE_IDENTIFIER)
    {
        return resolver_follow_function_call(resolver, result, node);
    }
    // This is a normal expression, process it
    return resolver_follow_exp(resolver, node->parenthesis.exp, result);
}

static struct resolver_entity *resolver_follow_unsupported_node(struct resolver_process *resovler, struct node *node, struct resolver_result *result);

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

    if (entity->type == RESOLVER_ENTITY_TYPE_VARIABLE && datatype_is_struct_or_union(&entity->var_data.dtype) ||
        (entity->type == RESOLVER_ENTITY_TYPE_FUNCTION && datatype_is_struct_or_union(&entity->dtype)))
    {
        result->last_struct_union_entity = entity;
    }
    return entity;
}
struct resolver_entity *resolver_follow_identifier(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    struct resolver_entity *entity = resolver_follow_for_name(resolver, node->sval, result);
    if (!entity)
    {
        // No entity? Is it a native function maybe
        struct symbol *symbol = symresolver_get_symbol_for_native_function(resolver->compiler, node->sval);
        if (symbol)
        {
            // Yep it is great
            entity = resolver_create_new_entity_for_native_function(resolver, symbol->name, symbol);
            resolver_result_entity_push(result, entity);
        }
    }
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

struct resolver_entity *resolver_follow_indirection(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    // This is an indirection unary i.e **a.b; or *a.b;
    // Follow the operand
    resolver_follow_part(resolver, node->unary.operand, result);

    struct resolver_entity *last_entity = resolver_result_peek(result);
    if (!last_entity)
    {
        last_entity = resolver_follow_unsupported_node(resolver, node->unary.operand, result);
    }
    struct resolver_entity *unary_indirection_entity = resolver_create_new_unary_indirection_entity(resolver, result, node, node->unary.indirection.depth);
    resolver_result_entity_push(result, unary_indirection_entity);
    return unary_indirection_entity;
}
struct resolver_entity *resolver_follow_unary_address(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    result->flags |= RESOLVER_RESULT_FLAG_DOES_GET_ADDRESS;

    // Okay this is an address unary i.e &a.b.c;
    resolver_follow_part(resolver, node->unary.operand, result);

    struct resolver_entity *last_entity = resolver_result_peek(result);

    // We have resolved the operand....
    // We must push to the stack a rule regarding the unary address
    // that will +1 on the pointer depth of the datatype
    // I.e
    // char a; char* x = &a; will produce a char* . char datatype will become char*

    struct resolver_entity *unary_address_entity = resolver_create_new_unary_get_address_entity(resolver, result, &last_entity->dtype, node, last_entity->scope, last_entity->offset);
    resolver_result_entity_push(result, unary_address_entity);
    return unary_address_entity;
}

struct resolver_entity *resolver_follow_unary(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{

    struct resolver_entity *result_entity = NULL;
    // What type of unary is this?
    if (op_is_indirection(node->unary.op))
    {
        result_entity = resolver_follow_indirection(resolver, node, result);
    }
    else if (op_is_address(node->unary.op))
    {
        result_entity = resolver_follow_unary_address(resolver, node, result);
    }

    return result_entity;
}

struct resolver_entity *resolver_follow_cast(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    struct resolver_entity *operand_entity = NULL;

    // By default operands of a cast will be unsupported and need further processing.
    resolver_follow_unsupported_node(resolver, node->cast.operand, result);
    operand_entity = resolver_result_peek(result);

    operand_entity->flags |= RESOLVER_ENTITY_FLAG_WAS_CASTED;

    struct resolver_entity *cast_entity = resolver_create_new_cast_entity(resolver, operand_entity->scope, &node->cast.dtype);
    if (datatype_is_struct_or_union(&node->cast.dtype))
    {
        if (!cast_entity->scope)
        {
            cast_entity->scope = resolver->scope.current;
        }
        result->last_struct_union_entity = cast_entity;
    }
    resolver_result_entity_push(result, cast_entity);
    return cast_entity;
}

struct resolver_entity *resolver_follow_unsupported_unary_node(struct resolver_process *resolver, struct node *node, struct resolver_result *result)
{
    return resolver_follow_part_return_entity(resolver, node->unary.operand, result);
}
static struct resolver_entity *resolver_follow_unsupported_node(struct resolver_process *resovler, struct node *node, struct resolver_result *result)
{
    // We still need to know the type of this unsupported node so we should continue to follow it
    bool followed = false;
    switch (node->type)
    {
    case NODE_TYPE_UNARY:
        resolver_follow_unsupported_unary_node(resovler, node, result);
        followed = true;
        break;

    default:
        followed = false;
    }

    struct resolver_entity *unsupported_entity = resolver_create_new_entity_for_unsupported_node(result, node);
    assert(unsupported_entity);

    // Push the unsupported entity to the result stack
    resolver_result_entity_push(result, unsupported_entity);
    return unsupported_entity;
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

    case NODE_TYPE_BRACKET:
        entity = resolver_follow_array_bracket(resolver, node, result);
        break;

    case NODE_TYPE_EXPRESSION_PARENTHESIS:
        entity = resolver_follow_exp_parenthesis(resolver, node, result);
        break;

    case NODE_TYPE_CAST:
        entity = resolver_follow_cast(resolver, node, result);
        break;

    case NODE_TYPE_UNARY:
        entity = resolver_follow_unary(resolver, node, result);
        break;
    default:
    {
        // Couldn't do anything? Then create a special entity that requires more computation
        // later on
        entity = resolver_follow_unsupported_node(resolver, node, result);
    }
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

struct resolver_entity *resolver_merge_compile_time_result(struct resolver_process *resolver, struct resolver_result *result, struct resolver_entity *left_entity, struct resolver_entity *right_entity)
{
    if (left_entity && right_entity)
    {
        if (left_entity->flags & RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_NEXT_ENTITY ||
            right_entity->flags & RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY)
        {
            goto no_merge_possible;
        }

        struct resolver_entity *result_entity =
            resolver->callbacks.merge_entities(resolver, result, left_entity, right_entity);
        if (!result_entity)
        {
            goto no_merge_possible;
        }
        return result_entity;
    }

no_merge_possible:
    return NULL;
}

void resolver_push_vector_of_entities(struct resolver_result *result, struct vector *vec)
{
    vector_set_peek_pointer_end(vec);
    vector_set_flag(vec, VECTOR_FLAG_PEEK_DECREMENT);
    struct resolver_entity *entity = vector_peek_ptr(vec);
    while (entity)
    {
        resolver_result_entity_push(result, entity);
        entity = vector_peek_ptr(vec);
    }
}

void _resolver_merge_compile_times(struct resolver_process *resolver, struct resolver_result *result)
{
    struct vector *saved_entities = vector_create(sizeof(struct resolver_entity *));

    while (1)
    {
        struct resolver_entity *right_entity = resolver_result_pop(result);
        struct resolver_entity *left_entity = resolver_result_pop(result);
        if (!right_entity)
        {
            // Nothing on the stack...
            break;
        }

        if (!left_entity)
        {
            // Only one entity? Then theirs nothing to be done push it back and lets go
            resolver_result_entity_push(result, right_entity);
            break;
        }

        struct resolver_entity *merged_entity = resolver_merge_compile_time_result(resolver, result, left_entity, right_entity);
        if (merged_entity)
        {
            // We have a merged entity push to the resolver result.
            resolver_result_entity_push(result, merged_entity);
            continue;
        }

        // Right entity must never merge with the left again.
        right_entity->flags |= RESOLVER_ENTITY_FLAG_NO_MERGE_WITH_LEFT_ENTITY;

        // We failed to merge, we must push the right entity to the saved entities stack
        vector_push(saved_entities, &right_entity);

        // The left entity goes back to the result as we may be able to merge it with next entity
        resolver_result_entity_push(result, left_entity);
    }

    // Now we must push the vector back to the result
    resolver_push_vector_of_entities(result, saved_entities);
    vector_free(saved_entities);
}

void resolver_merge_compile_times(struct resolver_process *resolver, struct resolver_result *result)
{
    size_t total_entities = 0;
    do
    {
        total_entities = result->count;
        _resolver_merge_compile_times(resolver, result);
        break;
    } while (total_entities != 1 && total_entities != result->count);
}

void resolver_rule_apply_rules(struct resolver_entity *rule_entity, struct resolver_entity *left_entity, struct resolver_entity *right_entity)
{
    assert(rule_entity->type == RESOLVER_ENTITY_TYPE_RULE);
    if (left_entity)
    {
        left_entity->flags |= rule_entity->rule.left.flags;
    }

    if (right_entity)
    {
        right_entity->flags |= rule_entity->rule.right.flags;
    }
}
void resolver_execute_rules(struct resolver_process *resolver, struct resolver_result *result)
{
    struct vector *saved_entities = vector_create(sizeof(struct resolver_entity *));
    struct resolver_entity *entity = resolver_result_pop(result);
    struct resolver_entity *last_processed_entity = NULL;
    while (entity)
    {
        if (entity->type == RESOLVER_ENTITY_TYPE_RULE)
        {
            struct resolver_entity *left_entity = resolver_result_pop(result);
            resolver_rule_apply_rules(entity, left_entity, last_processed_entity);
            entity = left_entity;
        }

        vector_push(saved_entities, &entity);
        last_processed_entity = entity;
        entity = resolver_result_pop(result);
    }
    resolver_push_vector_of_entities(result, saved_entities);
}

void resolver_finalize_unary(struct resolver_process *resolver, struct resolver_result *result, struct resolver_entity *entity)
{
    // We must finalize the unary, this is acomplished by taking the entity  previous to this entity
    // then merging the datatypes
    struct resolver_entity *previous_entity = entity->prev;
    if (!previous_entity)
    {
        // What are we going to do..
        return;
    }

    entity->scope = previous_entity->scope;
    entity->dtype = previous_entity->dtype;
    entity->offset = previous_entity->offset;

    if (entity->type == RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION)
    {
        int indirection_depth = entity->indirection.depth;
        entity->dtype.pointer_depth -= indirection_depth;
        if (entity->dtype.pointer_depth <= 0)
        {
            //     // We aren't a pointer anymore.. i.e char* a; *a; = (char)
            entity->dtype.flags &= ~DATATYPE_FLAG_IS_POINTER;
        }
    }
    else if (entity->type == RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS)
    {
        // Since we are a get address entity we need to also turn the datatype into a pointer..
        // Ideally this should be acheived on the stack, for now it will be achieved here...
        entity->dtype.flags |= DATATYPE_FLAG_IS_POINTER;
        entity->dtype.pointer_depth++;
    }
}

void resolver_finalize_last_entity(struct resolver_process *resolver, struct resolver_result *result)
{
    struct resolver_entity *last_entity = resolver_result_peek(result);
    switch (last_entity->type)
    {
    case RESOLVER_ENTITY_TYPE_UNARY_INDIRECTION:
    case RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS:
        resolver_finalize_unary(resolver, result, last_entity);
        break;
    }
}

void resolver_finalize_result_flags(struct resolver_process *resolver, struct resolver_result *result)
{
    int flags = RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE;
    // We must iterate through all of the results
    struct resolver_entity *entity = result->entity;
    struct resolver_entity *first_entity = entity;
    struct resolver_entity *last_entity = result->last_entity;
    bool does_get_address = false;
    if (entity == last_entity)
    {
        // One entity?
        // Is it a structure?
        if (last_entity->type == RESOLVER_ENTITY_TYPE_VARIABLE &&
            datatype_is_struct_or_union_non_pointer(&last_entity->dtype))
        {
            // Last variable is a structure non pointer..
            // therefore it must be pushed to the stack which may require loading the first entity
            // into the EBX register.
            flags |= RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX;
            flags &= ~RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE;
        }

        result->flags = flags;
        return;
    }

    while (entity)
    {
        if (entity->flags & RESOLVER_ENTITY_FLAG_DO_INDIRECTION)
        {
            // Since we have indirection we must first load the address
            // of the first entity.
            flags |= RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX | RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE;
            flags &= ~RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE;
        }

        if (entity->type == RESOLVER_ENTITY_TYPE_UNARY_GET_ADDRESS)
        {
            flags |= RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX | RESOLVER_RESULT_FLAG_DOES_GET_ADDRESS;
            flags &= ~RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE | RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE;
            does_get_address = true;
        }

        if (entity->type == RESOLVER_ENTITY_TYPE_FUNCTION_CALL)
        {
            // We have a function call for the first entity? THen we must load the address before
            // processing the entity.
            flags |= RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX;
            flags &= ~RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE;
        }

        if (entity->type == RESOLVER_ENTITY_TYPE_ARRAY_BRACKET)
        {
            if (entity->dtype.flags & DATATYPE_FLAG_IS_POINTER)
            {
                flags |= RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE;
                flags &= ~RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX;
            }
            else
            {
                flags |= RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX;
                flags &= ~RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE;
            }
            if (entity->flags & RESOLVER_ENTITY_FLAG_IS_POINTER_ARRAY_ENTITY)
            {
                flags |= RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE;
            }
        }

        if (entity->type == RESOLVER_ENTITY_TYPE_GENERAL)
        {
            flags |= RESOLVER_RESULT_FLAG_FIRST_ENTITY_LOAD_TO_EBX | RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE;
            flags &= ~RESOLVER_RESULT_FLAG_FIRST_ENTITY_PUSH_VALUE;
        }

        entity = entity->next;
    }

    if (last_entity->dtype.flags & DATATYPE_FLAG_IS_ARRAY && (!does_get_address && last_entity->type == RESOLVER_ENTITY_TYPE_VARIABLE && !(last_entity->flags & RESOLVER_ENTITY_FLAG_USES_ARRAY_BRACKETS)))
    {
        // Here we need to deal with circumstances such as
        // char abc[50]; char* p = abc; Without handling this senario abc[0] will go into the p variable
        // rather than the address of abc.

        flags &= ~RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE;
    }
    else if (last_entity->type == RESOLVER_ENTITY_TYPE_VARIABLE)
    {
        flags |= RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE;
    }

    if (does_get_address)
    {
        // Getting address does not require indirection
        flags &= ~RESOLVER_RESULT_FLAG_FINAL_INDIRECTION_REQUIRED_FOR_VALUE;
    }
    result->flags |= flags;
}
void resolver_finalize_result(struct resolver_process *resolver, struct resolver_result *result)
{
    struct resolver_entity *first_entity = resolver_result_entity_root(result);
    if (!first_entity)
    {
        // Nothing .. okay
        return;
    }
    resolver->callbacks.set_result_base(result, first_entity);
    resolver_finalize_result_flags(resolver, result);
    resolver_finalize_last_entity(resolver, result);
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
    resolver_execute_rules(resolver, result);
    resolver_merge_compile_times(resolver, result);
    resolver_finalize_result(resolver, result);
    return result;
}
