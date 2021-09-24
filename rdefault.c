#include "compiler.h"

struct resolver_default_entity_data *resolver_default_entity_private(struct resolver_entity *entity)
{
    return entity->private;
}

struct resolver_default_scope_data *resolver_default_scope_private(struct resolver_scope *scope)
{
    return scope->private;
}

char *resolver_default_stack_asm_address(int stack_offset, char *out)
{
    if (stack_offset < 0)
    {
        sprintf(out, "ebp%i", stack_offset);
        return out;
    }

    sprintf(out, "ebp+%i", stack_offset);
    return out;
}

struct resolver_default_entity_data *resolver_default_new_entity_data()
{
    struct resolver_default_entity_data *entity_data = calloc(sizeof(struct resolver_default_entity_data), 1);
    return entity_data;
}

void resolver_default_global_asm_address(const char* name, int offset, char *address_out)
{
    if (offset == 0)
    {
        sprintf(address_out, "%s", name);
        return;
    }
    assert(name);
    assert(address_out);
    sprintf(address_out, "%s+%i", name, offset);
}

void resolver_default_entity_data_set_address(struct resolver_default_entity_data *entity_data, struct node *var_node, int offset, int flags)
{
    if(!variable_node(var_node)->var.name)
    {
        // Only variables whome have a name should we care about setting an address for
        return;
    }

    entity_data->offset = offset;
    if (flags & RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK)
    {
        resolver_default_stack_asm_address(offset, entity_data->address);
        sprintf(entity_data->base_address, "ebp");
    }
    else
    {
        resolver_default_global_asm_address(variable_node(var_node)->var.name, offset, entity_data->address);
        sprintf(entity_data->base_address, "%s", variable_node(var_node)->var.name);
    }
}

struct resolver_default_entity_data *resolver_default_new_entity_data_for_var_node(struct node *var_node, int offset, int flags)
{
    struct resolver_default_entity_data *entity_data = resolver_default_new_entity_data();
    assert(variable_node(var_node));
    entity_data->offset = offset;
    entity_data->flags = flags;
    entity_data->type = RESOLVER_DEFAULT_ENTITY_DATA_TYPE_VARIABLE;
    resolver_default_entity_data_set_address(entity_data, variable_node(var_node), offset, flags);
    return entity_data;
}

struct resolver_default_entity_data *resolver_default_new_entity_data_for_function(struct node *func_node, int flags)
{
    struct resolver_default_entity_data *entity_data = resolver_default_new_entity_data();
    entity_data->flags = flags;
    entity_data->type = RESOLVER_DEFAULT_ENTITY_DATA_TYPE_FUNCTION;
    resolver_default_global_asm_address(func_node->sval, 0, entity_data->address);
    return entity_data;
}

struct resolver_entity *resolver_default_new_scope_entity(struct resolver_process* resolver, struct node *var_node, int offset, int flags)
{
    assert(var_node->type == NODE_TYPE_VARIABLE);
    struct resolver_default_entity_data *entity_data = resolver_default_new_entity_data_for_var_node(variable_node(var_node), offset, flags);
    return resolver_new_entity_for_var_node(resolver, variable_node(var_node), entity_data);
}

struct resolver_entity* resolver_default_register_function(struct resolver_process* resolver, struct node* func_node, int flags)
{
    struct resolver_default_entity_data *private = resolver_default_new_entity_data_for_function(func_node, flags);
    return resolver_register_function(resolver, func_node, private);
}

void resolver_default_new_scope(struct resolver_process* resolver, int flags)
{
    struct resolver_default_scope_data *scope_data = calloc(sizeof(struct resolver_default_scope_data), 1);
    scope_data->flags |= flags;

    int resolver_scope_flags = 0;
    if (flags & RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK)
    {
        resolver_scope_flags |= RESOLVER_SCOPE_FLAG_IS_STACK;
    }
    resolver_new_scope(resolver, scope_data, resolver_scope_flags);
}

void resolver_default_finish_scope(struct resolver_process* resolver)
{
    resolver_finish_scope(resolver);
}



/**
 * This function is called when a structure is accessed and that causes scope entities
 * to be created. This function is responsible for retunring the private data that must
 * be assigned to this resolver_entity
 * 
 * Only called for the first entity structure that needs to be created.
 * 
 * I.e "a.b.c" will only be called for "b". "c will call another function
 */
void *resolver_default_new_struct_entity(struct resolver_result *result, struct node *var_node, int offset, struct resolver_scope *scope)
{
    int entity_flags = 0x00;
    if (scope->flags & RESOLVER_SCOPE_FLAG_IS_STACK)
    {
        entity_flags |= RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK;
    }

    struct resolver_default_entity_data *result_entity = resolver_default_new_entity_data_for_var_node(variable_node(result->identifier->node), offset, entity_flags);
    return result_entity;
}

void *resolver_default_merge_struct_entity(struct resolver_result *result, struct resolver_entity *left_entity, struct resolver_entity *right_entity, struct resolver_scope *scope)
{
    int entity_flags = 0x00;
    if (scope->flags & RESOLVER_SCOPE_FLAG_IS_STACK)
    {
        entity_flags |= RESOLVER_DEFAULT_ENTITY_FLAG_IS_LOCAL_STACK;
    }

    int left_offset = resolver_default_entity_private(left_entity)->offset;
    int right_offset = resolver_default_entity_private(right_entity)->offset;
    return resolver_default_new_entity_data_for_var_node(variable_node(result->first_entity_const->node), left_offset + right_offset, entity_flags);
}

void *resolver_default_new_array_entity(struct resolver_result *result, struct resolver_entity *array_entity, int index_val, int index)
{
    int index_offset = array_offset(&array_entity->var_data.dtype, index, index_val);
    int final_offset = resolver_default_entity_private(array_entity)->offset + index_offset;
    return resolver_default_new_entity_data_for_var_node(variable_node(result->identifier->node), final_offset, 0);
}

void resolver_default_join_array_entity_index(struct resolver_result *result, struct resolver_entity *join_entity, int index_val, int index)
{
    struct resolver_default_entity_data *private = resolver_default_entity_private(join_entity);
    int index_offset = array_offset(&join_entity->var_data.dtype, index, index_val);
    int final_offset = resolver_default_entity_private(join_entity)->offset + index_offset;
    resolver_default_entity_data_set_address(resolver_default_entity_private(join_entity), variable_node(join_entity->node), final_offset, 0);
}

void resolver_default_delete_entity(struct resolver_entity *entity)
{
    free(entity->private);
}

void resolver_default_delete_scope(struct resolver_scope *scope)
{
    free(scope->private);
}


struct resolver_process* resolver_default_new_process(struct compile_process* compiler)
{
    return resolver_new_process(compiler, &(struct resolver_callbacks){.new_struct_entity = resolver_default_new_struct_entity, .merge_struct_entity = resolver_default_merge_struct_entity, .new_array_entity = resolver_default_new_array_entity, .join_array_entity_index = resolver_default_join_array_entity_index, .delete_entity = resolver_default_delete_entity, .delete_scope = resolver_default_delete_scope});
}