#include "compiler.h"
#include "helpers/buffer.h"
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

enum
{
    // This flag is set if this is a global variable.
    CODEGEN_SCOPE_FLAG_VARIABLE_IS_GLOBAL = 0b00000001
};

static struct compile_process *current_process;

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
    return *(struct expression_state **)(vector_back(current_process->generator.states.expr));
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
    codegen_current_exp_state()->flags &= ~EXPRESSION_FLAG_RIGHT_NODE;
    if (is_right_operand)
    {
        codegen_current_exp_state()->flags |= EXPRESSION_FLAG_RIGHT_NODE;
    }
}

void codegen_generate_node(struct node *node);
void codegen_generate_expressionable(struct node *node);
void codegen_generate_new_expressionable(struct node *node)
{
    codegen_generate_expressionable(node);
    register_unset_flag(REGISTER_EAX_IS_USED);
}
const char *op;

/**
 * For literal numbers
 */
void codegen_generate_number_node(struct node *node)
{
    if (!register_is_used("eax"))
    {
        register_set_flag(REGISTER_EAX_IS_USED);
        asm_push("mov eax, %i", node->llnum);
        return;
    }

    asm_push("add eax, %lld", node->llnum);
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
 * will return the closest entity it can that can be done at compile time
 */
struct codegen_scope_entity *codegen_get_variable_for_node(struct node *node)
{
    // Structures and arrays are not supported yet
    if (node->type != NODE_TYPE_IDENTIFIER)
    {
        return NULL;
    }
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
    codegen_new_expression_state();
    // Process the right node first as this is an expression
    codegen_generate_expressionable(node->exp.right);

    // Now lets find the stack offset
    struct codegen_scope_entity *assignment_operand = codegen_get_variable_for_node(node->exp.left);
    assert(assignment_operand);

    if (assignment_operand->stack_offset < 0)
    {
        asm_push("mov [ebp%i], %s", assignment_operand->stack_offset, codegen_sub_register("eax", assignment_operand->node->var.type.size));
    }
    else
    {
        asm_push("mov [ebp%i], %s", assignment_operand->stack_offset, codegen_sub_register("eax", assignment_operand->node->var.type.size));
    }
    codegen_end_expression_state();
}

void codegen_generate_exp_node(struct node *node)
{
    if (is_node_assignment(node))
    {
        codegen_generate_assignment_expression(node);
        return;
    }

    codegen_new_expression_state();
    codegen_generate_expressionable(node->exp.left);
    codegen_generate_expressionable(node->exp.right);

    if (is_node_mul_or_div(node))
    {
        if (S_EQ(node->exp.op, "*"))
            asm_push("mul ecx");
        else if (S_EQ(node->exp.op, "/"))
            asm_push("div ecx");
    }
    codegen_end_expression_state();
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

    if (entity->flags & CODEGEN_SCOPE_FLAG_VARIABLE_IS_GLOBAL)
    {
        // global variables not supported yet;
        assert(1 == 0);
        return;
    }

    if (entity->stack_offset < 0)
    {
        sprintf(out, "ebp%i", entity->stack_offset);
        return;
    }

    sprintf(out, "ebp+%i", entity->stack_offset);
}
void codegen_generate_address_of_variable(struct node *node, const char **reg_out)
{
    // When we have an identifier this typically represents a variable
    // lets treat it as such. We are accessing a variable here.
    char tmp[256];
    struct codegen_scope_entity *var_entity = codegen_get_variable(node->sval);
    assert(var_entity && "Expected a variable, validation is not done here, validator should have picked up this variable does not exist. Compiler bug");
    const char *reg = codegen_choose_ebx_or_edx();

    codegen_scope_entity_to_asm_address(var_entity, tmp);
    if (var_entity->stack_offset < 0)
    {
        asm_push("lea %s, [%s]", reg, var_entity->stack_offset, tmp);
        *reg_out = reg;
        return;
    }

    asm_push("lea %s, [%s]", reg, var_entity->stack_offset, tmp);
    *reg_out = reg;
}

void codegen_generate_identifier(struct node *node)
{
    struct codegen_scope_entity *assignment_operand = codegen_get_variable_for_node(node);
    assert(assignment_operand);

    if (!register_is_used("eax"))
    {
        register_set_flag(REGISTER_EAX_IS_USED);
        char tmp[256];
        codegen_scope_entity_to_asm_address(assignment_operand, tmp);
        asm_push("mov eax, [%s]", tmp);
        return;
    }

    asm_push("add eax, %lld", node->llnum);
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
    codegen_scope_push(codegen_new_scope_entity(node, 0, CODEGEN_SCOPE_FLAG_VARIABLE_IS_GLOBAL));
    asm_push("; %s %s", node->var.type.type_str, node->var.name);
    if (node->var.val)
    {
        codegen_generate_global_variable_with_value(node);
        return;
    }
    codegen_generate_global_variable_without_value(node);
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
        codegen_generate_new_expressionable(node);
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
    codegen_generate_scope(node->func.body_node->body.statements);
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