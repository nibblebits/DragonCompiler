#include "compiler.h"
#include "helpers/buffer.h"
#include <stdarg.h>
#include <stdbool.h>
static struct compile_process *current_process;

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
    struct node **node_ptr = vector_peek(current_process->node_tree_vec);
    if (!node_ptr)
        return NULL;

    return *node_ptr;
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
    asm_push("mov eax, 0");
    codegen_generate_expressionable(node);
}
const char *op;

/**
 * For literal numbers
 */
void codegen_generate_number_node(struct node *node)
{
    if (S_EQ(op, "+"))
        asm_push("add eax, %i", node->inum);
    else if (S_EQ(op, "-"))
        asm_push("sub eax, %i", node->inum);

    // For multiplications and divisions set the ECX register. We will multiply it later.
    else if (S_EQ(op, "*"))
        asm_push("mov ecx, %i", node->inum);
    else if (S_EQ(op, "/"))
        asm_push("mov ecx, %i", node->inum);
}
/**
 * Hopefully we can find a better way of doing this.
 * Currently its the best ive got, to check for mul_or_div for certain operations
 */
static bool is_node_mul_or_div(struct node *node)
{
    return S_EQ(node->exp.op, "*") || S_EQ(node->exp.op, "/");
}

void codegen_generate_exp_node(struct node *node)
{
    op = node->exp.op;

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
    }
}

void codegen_exp_write_to_buffer(struct buffer *buffer, struct node *node)
{
    switch (node->type)
    {
    case NODE_TYPE_EXPRESSION:
        codegen_exp_write_to_buffer(buffer, node->exp.left);
        buffer_printf(buffer, "%s", node->exp.op);
        codegen_exp_write_to_buffer(buffer, node->exp.right);
        break;

    case NODE_TYPE_IDENTIFIER:
        buffer_printf(buffer, "%s", node->sval);
    case NODE_TYPE_NUMBER:
        buffer_printf(buffer, "%i", node->inum);
        break;
    }
}
/**
 * Converts the given expression node into a string that can be passed around
 */
struct buffer *codegen_exp_to_buffer(struct node *node)
{
    // We need to create a buffer to write the node into
    struct buffer *buf = buffer_create();
    codegen_exp_write_to_buffer(buf, node);
    return buf;
}

void codegen_generate_global_variable_with_value(struct node *node)
{
    struct buffer* buf = codegen_exp_to_buffer(node->var.val);
    asm_push("%s: %s %s", node->var.name, asm_keyword_for_size(node->var.type.size), buffer_ptr(buf));
    buffer_free(buf);
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
    codegen_generate_global_variable_without_value(node);
}

void codegen_generate_root_node(struct node *node)
{
    switch (node->type)
    {
    case NODE_TYPE_VARIABLE:
        // Was processed earlier..
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
    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_generate_data_section();
    vector_set_peek_pointer(process->node_tree_vec, 0);
    codegen_generate_root();

    return 0;
}