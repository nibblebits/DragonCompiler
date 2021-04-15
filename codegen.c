#include "compiler.h"
#include "helpers/buffer.h"
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

static struct compile_process *current_process;

// Returned when we have no expression state.
static struct expression_state blank_state = {};

#define codegen_err(...) \
    compiler_error(current_process, __VA_ARGS__)

#define codegen_scope_new() \
    scope_new(current_process)

#define codegen_scope_finish() \
    scope_finish(current_process)

#define codegen_scope_push(value) \
    scope_push(current_process, value)

#define codegen_scope_last_entity() \
    scope_last_entity(current_process)

struct codegen_scope_entity *codegen_get_scope_variable_for_node(struct node *node, bool *position_known_at_compile_time);
void codegen_scope_entity_to_asm_address(struct codegen_scope_entity *entity, char *out);

struct codegen_scope_entity
{
    // The flags for this scope entity
    int flags;

    // The stack offset this scope entity can be accessedd at.
    // i.e -4, -8 -12
    // If this scope entity has no stack entity as its a global scope
    // then this value should be ignored.
    int stack_offset;

    // A node to a variable declaration.
    struct node *node;
};

enum
{
    CODEGEN_ENTITY_TYPE_STACK,
    CODEGEN_ENTITY_TYPE_SYMBOL
};

/**
 * Codegen entities are addressable areas of memory known at compile time.
 * For example they can represent scope variables, functions or global variables
 */
struct codegen_entity
{
    int type;
    // The node of the entity
    struct node *node;
    // The address that can be addressed in assembly. I.e [ebp-4] [name]
    char address[60];
    bool is_scope_entity;

    union
    {
        struct codegen_scope_entity *scope_entity;
        struct symbol *global_symbol;
    };
};

struct codegen_scope_entity *codegen_new_scope_entity(struct node *node, int stack_offset, int flags)
{
    struct codegen_scope_entity *entity = calloc(sizeof(struct codegen_scope_entity), 1);
    entity->node = node;
    entity->flags = flags;
    entity->stack_offset = stack_offset;
    return entity;
}

void codegen_free_scope_entity(struct codegen_scope_entity *entity)
{
    free(entity);
}

struct codegen_scope_entity *codegen_get_variable(const char *name)
{
    struct scope *current = current_process->scope.current;
    while (current)
    {
        scope_iteration_start(current);
        for (int i = 0; i < vector_count(current->entities); i++)
        {
            struct codegen_scope_entity *entity = scope_iterate_back(current);
            if (!entity)
            {
                continue;
            }

            if (S_EQ(entity->node->var.name, name))
                return entity;
        }

        scope_iteration_end(current);
        current = current->parent;
    }

    return NULL;
}

struct node *codegen_get_entity_node(struct codegen_entity *entity)
{
    struct node *node = NULL;
    switch (entity->type)
    {
    case CODEGEN_ENTITY_TYPE_STACK:
        node = entity->scope_entity->node;
        break;

    case CODEGEN_ENTITY_TYPE_SYMBOL:
        node = entity->global_symbol->data;
        assert(entity->global_symbol->type == SYMBOL_TYPE_NODE);
        break;

    default:
        // TODO: Create a function to do this kind of thing..
        assert(0 == 1 && "Unknown entity");
    }

    return node;
}

int codegen_get_entity_for_node(struct node *node, struct codegen_entity *entity_out)
{
    memset(entity_out, 0, sizeof(struct codegen_entity));

    // We currently do not deal with expressions.. not today
    if (node->type != NODE_TYPE_IDENTIFIER)
    {
        return -1;
    }

    bool position_known_at_compile_time;
    struct codegen_scope_entity *scope_entity = codegen_get_scope_variable_for_node(node, &position_known_at_compile_time);
    if (scope_entity)
    {
        entity_out->type = CODEGEN_ENTITY_TYPE_STACK;
        entity_out->scope_entity = scope_entity;
        entity_out->is_scope_entity = true;
        codegen_scope_entity_to_asm_address(entity_out->scope_entity, entity_out->address);
        entity_out->node = codegen_get_entity_node(entity_out);

        return 0;
    }

    struct symbol *sym = symresolver_get_symbol(current_process, node->sval);
    // Assertion because its a compile bug if we still cant find this symbol
    // validation should have peacefully temrinated the compiler way back...
    if (!sym)
    {
        return -1;
    }

    entity_out->type = CODEGEN_ENTITY_TYPE_SYMBOL;
    entity_out->global_symbol = sym;
    entity_out->is_scope_entity = false;
    entity_out->node = codegen_get_entity_node(entity_out);
    sprintf(entity_out->address, "%s", node->sval);

    // We only deal with node symbols right now.
    assert(sym->type == SYMBOL_TYPE_NODE);

    return 0;
}

static bool register_is_used(const char *reg)
{
    bool used = true;
    if (S_EQ(reg, "eax"))
    {
        used = current_process->generator.used_registers & REGISTER_EAX_IS_USED;
    }
    else if (S_EQ(reg, "ecx"))
    {
        used = current_process->generator.used_registers & REGISTER_ECX_IS_USED;
    }
    else if (S_EQ(reg, "edx"))
    {
        used = current_process->generator.used_registers & REGISTER_EDX_IS_USED;
    }
    else if (S_EQ(reg, "ebx"))
    {
        used = current_process->generator.used_registers & REGISTER_EBX_IS_USED;
    }

    return used;
}

void register_set_flag(int flag)
{
    current_process->generator.used_registers |= flag;
}

void register_unset_flag(int flag)
{
    current_process->generator.used_registers &= ~flag;
}

void asm_push(const char *ins, ...)
{
    va_list args;
    va_start(args, ins);
    vfprintf(stdout, ins, args);
    fprintf(stdout, "\n");
    va_end(args);
}

static const char *asm_keyword_for_size(size_t size)
{
    const char *keyword = NULL;
    switch (size)
    {
    case 1:
        keyword = "db";
        break;
    case 2:
        keyword = "dw";
        break;
    case 4:
        keyword = "dd";
        break;
    case 8:
        keyword = "dq";
        break;
    }

    return keyword;
}

static struct node *node_next()
{
    return vector_peek_ptr(current_process->node_tree_vec);
}

void codegen_new_expression_state()
{
    struct expression_state *state = malloc(sizeof(struct expression_state));
    memset(state, 0, sizeof(struct expression_state));

    vector_push(current_process->generator.states.expr, &state);
}

struct expression_state *codegen_current_exp_state()
{
    struct expression_state* state = vector_back_ptr_or_null(current_process->generator.states.expr);
    if (!state)
    {
        return &blank_state;
    }

    return state;
}

void codegen_end_expression_state()
{
    struct expression_state *state = codegen_current_exp_state();
    // Delete the memory we don't need it anymore
    free(state);
    vector_pop(current_process->generator.states.expr);
}

bool codegen_expression_on_right_operand()
{
    return codegen_current_exp_state()->flags & EXPRESSION_FLAG_RIGHT_NODE;
}

void codegen_expression_flags_set_right_operand(bool is_right_operand)
{
    assert(codegen_current_exp_state() != &blank_state);
    codegen_current_exp_state()->flags &= ~EXPRESSION_FLAG_RIGHT_NODE;
    if (is_right_operand)
    {
        codegen_current_exp_state()->flags |= EXPRESSION_FLAG_RIGHT_NODE;
    }
}

void codegen_expression_flags_set_in_function_call_arguments(bool in_function_arguments)
{
    assert(codegen_current_exp_state() != &blank_state);
    if (in_function_arguments)
    {
        codegen_current_exp_state()->flags |= EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS;
        return;
    }

    codegen_current_exp_state()->flags &= ~EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS;
    memset(&codegen_current_exp_state()->fca, 0, sizeof(codegen_current_exp_state()->fca));
}

void codegen_expression_flags_set_in_function_call_left_operand_flag(bool in_function_call_left_operand)
{
     assert(codegen_current_exp_state() != &blank_state);
    if (in_function_call_left_operand)
    {
        codegen_current_exp_state()->flags |= EXPRESSION_IN_FUNCTION_CALL_LEFT_OPERAND;
        return;
    }

    codegen_current_exp_state()->flags &= ~EXPRESSION_IN_FUNCTION_CALL_LEFT_OPERAND;
}

void codegen_generate_node(struct node *node);
void codegen_generate_expressionable(struct node *node);
void codegen_generate_new_expressionable(struct node *node)
{
    codegen_generate_expressionable(node);
}
const char *op;

/**
 * For literal numbers
 */
void codegen_generate_number_node(struct node *node)
{
    assert(!register_is_used("eax"));
    register_set_flag(REGISTER_EAX_IS_USED);
    asm_push("mov eax, %i", node->llnum);
}
/**
 * Hopefully we can find a better way of doing this.
 * Currently its the best ive got, to check for mul_or_div for certain operations
 */
static bool is_node_mul_or_div(struct node *node)
{
    return S_EQ(node->exp.op, "*") || S_EQ(node->exp.op, "/");
}

static bool is_node_assignment(struct node *node)
{
    return S_EQ(node->exp.op, "=") ||
           S_EQ(node->exp.op, "+=") ||
           S_EQ(node->exp.op, "-=") ||
           S_EQ(node->exp.op, "*=") ||
           S_EQ(node->exp.op, "/=");
}

/**
 * Iterates through the node until a codegen_scope_entity can be found, otherwise returns NULL.
 * This function does not work for structure pointers as its impossible to complete this action
 * at compile time. If the node ends up at any point pointing to a structure pointer, then this function
 * will return the closest entity it can that can be done at compile time and the position_known_at_compile_time will be set to false signifying some runtime code will be required to complete
 * positioning.
 * 
 * \param position_known_at_compile_time The pointer here is set to true by this function if the exact location of the variable is known. Otherwise its false. 
 */
struct codegen_scope_entity *codegen_get_scope_variable_for_node(struct node *node, bool *position_known_at_compile_time)
{
    *position_known_at_compile_time = false;
    // Structures and arrays are not supported yet
    if (node->type != NODE_TYPE_IDENTIFIER)
    {
        return NULL;
    }

    *position_known_at_compile_time = true;
    struct codegen_scope_entity *entity = codegen_get_variable(node->sval);
    return entity;
}

/**
 * Finds the correct sub register to use for the original register provided.
 * 
 * I.e if the size is one byte and you provide eax as the original register then al will be returned
 * 
 * \attention The original register must be a 32-bit wide general purpose register i.e eax, ecx, edx, or ebx
 */
const char *codegen_sub_register(const char *original_register, size_t size)
{
    const char *reg = NULL;
    if (S_EQ(original_register, "eax"))
    {
        if (size == 1)
        {
            reg = "al";
        }
        else if (size == 2)
        {
            reg = "ax";
        }
        else if (size == 4)
        {
            reg = "eax";
        }
    }
    else if (S_EQ(original_register, "ebx"))
    {
        if (size == 1)
        {
            reg = "bl";
        }
        else if (size == 2)
        {
            reg = "bx";
        }
        else if (size == 4)
        {
            reg = "ebx";
        }
    }
    else if (S_EQ(original_register, "ecx"))
    {
        if (size == 1)
        {
            reg = "cl";
        }
        else if (size == 2)
        {
            reg = "cx";
        }
        else if (size == 4)
        {
            reg = "ecx";
        }
    }
    else if (S_EQ(original_register, "edx"))
    {
        if (size == 1)
        {
            reg = "dl";
        }
        else if (size == 2)
        {
            reg = "dx";
        }
        else if (size == 4)
        {
            reg = "edx";
        }
    }
    return reg;
}

void codegen_generate_assignment_expression(struct node *node)
{
    struct codegen_entity assignment_operand_entity;

    codegen_new_expression_state();

    // Process the right node first as this is an expression
    codegen_generate_expressionable(node->exp.right);
    // Now lets find the stack offset
    assert(codegen_get_entity_for_node(node->exp.left, &assignment_operand_entity) == 0);

    codegen_end_expression_state();

    // Mark the EAX register as no longer used.
    register_unset_flag(REGISTER_EAX_IS_USED);

    // Write the move. Only intergers supported at the moment as you can see
    // this will be improved.
    asm_push("mov [%s], eax", assignment_operand_entity.address);
}


void codegen_generate_expressionable_function_arguments(struct codegen_entity* func_entity, struct node* func_call_args_exp_node, size_t* total_arguments_out)
{
    codegen_new_expression_state();
    codegen_expression_flags_set_in_function_call_arguments(true);
    codegen_generate_expressionable(func_call_args_exp_node);
    *total_arguments_out = codegen_current_exp_state()->fca.total_args;
    codegen_expression_flags_set_in_function_call_arguments(false);
    codegen_end_expression_state();

}

void codegen_generate_pop(const char* reg, size_t times)
{
    for (size_t i = 0; i < times; i++)
    {
        asm_push("pop %s", reg);
    }
}

void codegen_generate_function_call_for_exp_node(struct codegen_entity *func_entity, struct node *node)
{
    // Generate expression for left node. EBX should contain the address we care about
    codegen_new_expression_state();
    codegen_expression_flags_set_in_function_call_left_operand_flag(true);
    codegen_generate_expressionable(node->exp.left);
    codegen_expression_flags_set_in_function_call_left_operand_flag(false);
    codegen_end_expression_state();

    size_t total_args = 0;

    // Code generate the function arguments
    codegen_generate_expressionable_function_arguments(func_entity, node->exp.right, &total_args);

    // Call the function
    asm_push("call [ebx]");

    // We don't need EBX anymore
    register_unset_flag(REGISTER_EBX_IS_USED);

    // Now lets restore the stack to its original state before this function call
    asm_push("add esp, %i", FUNCTION_CALL_ARGUMENTS_GET_STACK_SIZE(total_args));

}

void codegen_generate_exp_node(struct node *node)
{
    if (is_node_assignment(node))
    {
        codegen_generate_assignment_expression(node);
        return;
    }

    struct codegen_entity entity;
    if (codegen_get_entity_for_node(node->exp.left, &entity) == 0)
    {
        switch (entity.node->type)
        {
        case NODE_TYPE_FUNCTION:
            codegen_generate_function_call_for_exp_node(&entity, node);
            break;
        }

        return;
    }

    codegen_generate_expressionable(node->exp.left);
    asm_push("push eax");
    // EAX has been saved, and its free to be used again
    register_unset_flag(REGISTER_EAX_IS_USED);
    codegen_generate_expressionable(node->exp.right);
    asm_push("pop ecx");
    // We are done with EAX we have stored the result.
    register_unset_flag(REGISTER_EAX_IS_USED);

    // We have the left node in ECX and the right node in EAX, we should flip them
    // so they are in the original order
    asm_push("xchg ecx, eax");

    // We now have left expression result in eax, and right expression result in ecx
    // Let's now deal with it
    if (S_EQ(node->exp.op, "*"))
    {
        asm_push("mul ecx");
    }
    else if (S_EQ(node->exp.op, "/"))
    {
        asm_push("div ecx");
    }
    else if (S_EQ(node->exp.op, "+"))
    {
        asm_push("add eax, ecx");
    }
    else if (S_EQ(node->exp.op, "-"))
    {
        asm_push("sub eax, ecx");
    }
}

/**
 * Picks EBX or EDX depending which one is free, if none are free then this is a compiler bug.
 * Once a register is picked it is marked as used. It is the callers responsibility
 * to unuse it when its done.
 */
const char *codegen_choose_ebx_or_edx()
{
    assert(!(register_is_used("edx") && register_is_used("ebx")));
    const char *reg = "";
    if (!register_is_used("ebx"))
    {
        reg = "ebx";
        register_set_flag(REGISTER_EBX_IS_USED);
    }
    else if (!register_is_used("edx"))
    {
        reg = "edx";
        register_set_flag(REGISTER_EDX_IS_USED);
    }

    return reg;
}

void codegen_release_register(const char *reg)
{
    if (S_EQ(reg, "eax"))
    {
        register_unset_flag(REGISTER_EAX_IS_USED);
    }
    else if (S_EQ(reg, "ebx"))
    {
        register_unset_flag(REGISTER_EBX_IS_USED);
    }
    else if (S_EQ(reg, "ecx"))
    {
        register_unset_flag(REGISTER_ECX_IS_USED);
    }
    else if (S_EQ(reg, "edx"))
    {
        register_unset_flag(REGISTER_EDX_IS_USED);
    }
}

void codegen_scope_entity_to_asm_address(struct codegen_scope_entity *entity, char *out)
{
    if (entity->stack_offset < 0)
    {
        sprintf(out, "ebp%i", entity->stack_offset);
        return;
    }

    sprintf(out, "ebp+%i", entity->stack_offset);
}

void codegen_handle_variable_access(struct codegen_entity *entity)
{
    assert(!register_is_used("eax"));
    // We accessing a variable? Then grab its value put it in EAX!
    register_set_flag(REGISTER_EAX_IS_USED);
    asm_push("mov eax, [%s]", entity->address);
}

void codegen_handle_function_access(struct codegen_entity* entity)
{
    register_set_flag(REGISTER_EBX_IS_USED);
    asm_push("lea ebx, [%s]", entity->address);
}

void codegen_generate_identifier(struct node *node)
{
    struct codegen_entity entity;
    assert(codegen_get_entity_for_node(node, &entity) == 0);


    // WHat is the type that we are referencing? A variable, a function? WHat is it...
    switch(entity.node->type)
    {
        case NODE_TYPE_VARIABLE:
          codegen_handle_variable_access(&entity);
        break;

        case NODE_TYPE_FUNCTION:
          codegen_handle_function_access(&entity);
        break;

        default:
            // Get a function for this thing..
            assert(1 == 0 && "Compiler bug");
    }
}

void codegen_generate_expressionable(struct node *node)
{
    switch (node->type)
    {
    case NODE_TYPE_NUMBER:
        codegen_generate_number_node(node);
        break;

    case NODE_TYPE_EXPRESSION:
        codegen_generate_exp_node(node);
        break;

    case NODE_TYPE_IDENTIFIER:
        codegen_generate_identifier(node);
        break;
    }

    if (codegen_current_exp_state()->flags & EXPRESSION_IN_FUNCTION_CALL_ARGUMENTS)
    {
        // We are currently in function arguments i.e (50, 20, 40)
        // because of this we have to push the EAX register once its been handled
        // so that the function call will have access to the stack.
        asm_push("push eax");

        // Let's remember the arguments for later.
        codegen_current_exp_state()->fca.total_args += 1;
    }

}

void codegen_generate_global_variable_with_value(struct node *node)
{
    switch (node->var.type.type)
    {
    case DATA_TYPE_CHAR:
    case DATA_TYPE_SHORT:
    case DATA_TYPE_INTEGER:
    case DATA_TYPE_LONG:
        asm_push("%s: %s %lld", node->var.name, asm_keyword_for_size(node->var.type.size), node->var.val->llnum);
        break;
    case DATA_TYPE_DOUBLE:
        codegen_err("Doubles are not yet supported for global variable values.");

        break;
    case DATA_TYPE_FLOAT:
        codegen_err("Floats are not yet supported for global variable values.");
        break;

    default:
        codegen_err("Not sure how to generate value for global variable.. Problem!");
    }
}

void codegen_generate_global_variable_without_value(struct node *node)
{
    asm_push("%s: %s 0", node->var.name, asm_keyword_for_size(node->var.type.size));
}

void codegen_generate_global_variable(struct node *node)
{
    asm_push("; %s %s", node->var.type.type_str, node->var.name);
    if (node->var.val)
    {
        codegen_generate_global_variable_with_value(node);
        return;
    }
}

size_t codegen_compute_stack_size(struct vector *vec)
{
    // Empty vector then theirs no stack size.
    if (vector_count(vec) == 0)
        return 0;

    size_t stack_size = 0;
    vector_set_peek_pointer(vec, 0);
    struct node *node = vector_peek_ptr(vec);
    while (node)
    {
        switch (node->type)
        {
        case NODE_TYPE_VARIABLE:
            stack_size += node->var.type.size;
            break;

        default:
            // We ignore all other nodes, we don't care for them. they wont
            // help us compute the stack size...
            break;
        };
        node = vector_peek_ptr(vec);
    }

    // Stack size must be 16 byte aligned as per C specification
    if (stack_size % C_STACK_ALIGNMENT)
        stack_size += C_STACK_ALIGNMENT - (stack_size % C_STACK_ALIGNMENT);

    return stack_size;
}

int codegen_stack_offset(struct node *node)
{
    int offset = -node->var.type.size;
    struct codegen_scope_entity *last_entity = codegen_scope_last_entity();
    if (last_entity)
    {
        // We use += because if the stack_offset is negative then this will do a negative
        // if its positive then it will do a positive. += is the best operator for both cases
        offset += last_entity->stack_offset;
    }

    return offset;
}

void codegen_generate_scope_variable(struct node *node)
{
    codegen_scope_push(codegen_new_scope_entity(node, codegen_stack_offset(node), 0));
}

void codegen_generate_statement(struct node *node)
{
    switch (node->type)
    {
    case NODE_TYPE_EXPRESSION:
        codegen_generate_exp_node(node);
        break;

    case NODE_TYPE_VARIABLE:
        codegen_generate_scope_variable(node);
        break;
    }
}
void codegen_stack_sub(size_t stack_size)
{
    if (stack_size != 0)
    {
        asm_push("sub esp, %lld", stack_size);
    }
}

void codegen_stack_add(size_t stack_size)
{
    if (stack_size != 0)
    {
        asm_push("add esp, %lld", stack_size);
    }
}

void codegen_generate_scope_no_new_scope(struct vector *statements)
{
    vector_set_peek_pointer(statements, 0);
    struct node *statement_node = vector_peek_ptr(statements);
    while (statement_node)
    {
        codegen_generate_statement(statement_node);
        statement_node = vector_peek_ptr(statements);
    }
}

void codegen_generate_scope(struct vector *statements)
{
    // New body new scope.
    codegen_scope_new();
    // We got to compute the stack size we need for our statements
    size_t stack_size = codegen_compute_stack_size(statements);
    codegen_stack_sub(stack_size);
    codegen_generate_scope_no_new_scope(statements);
    codegen_stack_add(stack_size);

    codegen_scope_finish();
}

void codegen_generate_function_body(struct node *node)
{
    codegen_generate_scope(node->body.statements);
}
void codegen_generate_function(struct node *node)
{
    asm_push("; %s function", node->func.name);
    asm_push("%s:", node->func.name);

    // We have to create a stack frame ;)
    asm_push("push ebp");
    asm_push("mov ebp, esp");

    // Generate scope for functon arguments
    codegen_scope_new();
    // We got to compute the stack size we need for our statements
    size_t stack_size = codegen_compute_stack_size(node->func.argument_vector);
    codegen_stack_sub(stack_size);
    codegen_generate_scope_no_new_scope(node->func.argument_vector);
    // Generate the function body
    codegen_generate_function_body(node->func.body_node);
    codegen_stack_add(stack_size);
    // End scope for function arguments
    codegen_scope_finish();

    asm_push("pop ebp");
    asm_push("ret");
}

void codegen_generate_root_node(struct node *node)
{
    switch (node->type)
    {
    case NODE_TYPE_VARIABLE:
        // Was processed earlier.. for data section
        break;

    case NODE_TYPE_FUNCTION:
        codegen_generate_function(node);
        break;
    }
}

void codegen_generate_data_section()
{
    asm_push("section .data");
    struct node *node = NULL;
    while ((node = node_next()) != NULL)
    {
        if (node->type == NODE_TYPE_VARIABLE)
        {
            codegen_generate_global_variable(node);
        }
    }
}
/**
 * Starts generating code from the root of the tree, working its way down the leafs
 */
void codegen_generate_root()
{
    asm_push("section .text");
    struct node *node = NULL;
    while ((node = node_next()) != NULL)
    {
        codegen_generate_root_node(node);
    }
}

int codegen(struct compile_process *process)
{
    current_process = process;
    // Create the root scope for this process
    scope_create_root(process);
    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_generate_data_section();
    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_generate_root();

    return 0;
}