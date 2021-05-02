/**
 * Helper.c is a terrible name come up with a better one..
 */

#include "compiler.h"
#include <assert.h>

/**
 * Gets the offset from the given structure stored in the "compile_proc".
 * Looks for the given variable specified named by "var"
 * Returns the absolute position starting from 0 upwards.
 * 
 * I.e
 * 
 * struct abc
 * {
 *    int a;
 *    int b;
 * };
 * 
 * If we did abc.a then 0 would be returned. If we did abc.b then 4 would be returned. because
 * int is 4 bytes long. 
 * 
 * \param compile_proc The compiler process to peek for a structure for
 * \param struct_name The name of the given structure we must peek into
 * \param var_name The variable in the structure that we want the offset for.
 * \param var_node_out Set to the variable node in the structure that we are resolving an offset for.
 */
int struct_offset(struct compile_process *compile_proc, const char *struct_name, const char *var_name, struct node **var_node_out)
{
    struct symbol *struct_sym = symresolver_get_symbol(compile_proc, struct_name);
    assert(struct_sym->type == SYMBOL_TYPE_NODE);

    struct node *node = struct_sym->data;
    assert(node->type == NODE_TYPE_STRUCT);

    struct vector *struct_vars_vec = node->_struct.body_n->body.statements;
    vector_set_peek_pointer(struct_vars_vec, 0);

    struct node *var_node_cur = vector_peek_ptr(struct_vars_vec);
    int position = 0;
    while (var_node_cur)
    {
        *var_node_out = var_node_cur;
        if (S_EQ(var_node_cur->var.name, var_name))
        {
            break;
        }

        position += var_node_cur->var.type.size;
        var_node_cur = vector_peek_ptr(struct_vars_vec);
    }

    return position;
}

bool is_access_operator(const char *op)
{
    return S_EQ(op, "->") || S_EQ(op, ".");
}

bool is_access_operator_node(struct node *node)
{
    return node->type == NODE_TYPE_EXPRESSION && is_access_operator(node->exp.op);
}

/**
 * If the node provided has the same type we are looking for then its self is returned
 * otherwise if its an expression we will iterate through the left operands of this expression
 * and all sub-expressions until the given type is found. Otherwise NULL is returned.
 * 
 * The depth specifies how deep to search, if the depth is 1 then only the left operand
 * of an expression will be checked and if its not found NULL will be returned. No deeper searching
 * will be done.
 * 
 * Passing a depth of zero is essentially the same as just checking if the given node if the type provider
 * no deeper searching will be done at all.
 */
struct node *first_node_of_type_from_left(struct node *node, int type, int depth)
{
    if (node->type == NODE_TYPE_EXPRESSION)
    {
        if (depth <= 0)
        {
            return NULL;
        }
        return first_node_of_type_from_left(node->exp.left, type, depth - 1);
    }

    if (node->type == type)
    {
        return node;
    }

    return NULL;
}

/**
 * Finds the first node of the given type.
 * 
 * For example lets imagine the expression "a.b.e.f"
 * if you called this function with NODE_TYPE_IDENTIFIER as the type and you passed in 
 * the right operand of the expression i.e a.E then you would find that the node of "b"
 * would be returned
 */
struct node *first_node_of_type(struct node *node, int type)
{
    if (node->type == type)
        return node;

    // Impossible for us to find this node type now unless we have an expression.
    if (!node->type == NODE_TYPE_EXPRESSION)
    {
        return NULL;
    }

    if (node->exp.left->type == type)
        return node->exp.left;

    struct node *tmp_node = first_node_of_type(node->exp.left, type);
    if (tmp_node)
    {
        return tmp_node;
    }

    return first_node_of_type(node->exp.right, type);
}

/**
 * Returns true if the given node is apart of an expression
 */
bool node_in_expression(struct node* node)
{
    return node->flags & NODE_FLAG_INSIDE_EXPRESSION;
}

bool node_is_root_expression(struct node* node)
{
    // Root expressions are not inside expressions.
    return !(node->flags & NODE_FLAG_INSIDE_EXPRESSION);
}