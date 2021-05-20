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
    *var_node_out = NULL;
    while (var_node_cur)
    {
        *var_node_out = var_node_cur;
        if (S_EQ(var_node_cur->var.name, var_name))
        {
            position = var_node_cur->var.offset;
            break;
        }

        var_node_cur = vector_peek_ptr(struct_vars_vec);
    }

    return position;
}

/**
 * Returns the node for the structure access expression.
 * 
 * For example if you had the structure "test" and "abc"
 * struct abc
 * {
 *    int z;
 * }
 *
 * struct test
 * {
 *   struct abc a;
 * }
 * 
 * and your expression node was "a.z" and your type_str was "test" you would have 
 * the node for variable "z" returned.
 * 
 * Likewise if only "a" was provided then the "a" variable node in the test structure would be returned.
 * 
 * \param offset_out This is set to the offset based on the access pattern. I.e "a.b.c.d.e.f" would take all those additional structure variables into account for calculating the offset
 */
struct node* struct_for_access(struct compile_process* process, struct node* node, const char* type_str, int* offset_out)
{
    assert((node->type == NODE_TYPE_EXPRESSION && is_access_operator(node->exp.op)) || node->type == NODE_TYPE_IDENTIFIER);

    struct node* var_node = NULL;
    if (node->type == NODE_TYPE_IDENTIFIER)
    {
        struct_offset(process, type_str, node->sval, &var_node);
        *offset_out += var_node->var.offset;
        return var_node;
    }

    // Let's handle the access expression i.e "a.z"
    var_node = struct_for_access(process, node->exp.left, type_str, offset_out);
    assert(var_node);

    return struct_for_access(process, node->exp.right, var_node->var.type.type_str, offset_out);

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
 * Decrements the depth so long as the value is not equal to DEPTH_INFINITE
 */
int decrement_depth(int depth)
{
    if(depth == DEPTH_INFINITE)
    {
        return DEPTH_INFINITE;
    }

    return depth-1;
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
 * Passing a depth of DEPTH_INFINITE is essentially the same as just checking if the given node if the type provider
 * no deeper searching will be done at all.
 */
struct node *first_node_of_type_from_left(struct node *node, int type, int depth)
{
    if (node->type == type)
    {
        return node;
    }

    if (depth <= -1)
    {
        return NULL;
    }

    struct node* left_node = NULL;
    switch (node->type)
    {
    case NODE_TYPE_EXPRESSION:
        left_node = first_node_of_type_from_left(node->exp.left, type, decrement_depth(depth));
        break;

    case NODE_TYPE_EXPRESSION_PARENTHESIS:
        left_node = first_node_of_type_from_left(node->parenthesis.exp, type, decrement_depth(depth));
        break;

    case NODE_TYPE_UNARY:
        left_node = first_node_of_type_from_left(node->unary.operand, type, decrement_depth(depth));
        break;
    }

    return left_node;
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
bool node_in_expression(struct node *node)
{
    return node->flags & NODE_FLAG_INSIDE_EXPRESSION;
}

bool node_is_root_expression(struct node *node)
{
    // Root expressions are not inside expressions.
    return !(node->flags & NODE_FLAG_INSIDE_EXPRESSION);
}

bool op_is_indirection(const char *op)
{
    return S_EQ(op, "*");
}


int align_value(int val, int to)
{
    if (val % to)
    {
        val += to - (val % to);
    }
    return val;
}

/**
 * Aligns the given value and if its a negative value then it pretends its positive
 * aligns it and then returns the negative result
 */
int align_value_treat_positive(int val, int to)
{
    assert(to >= 0);

    if (val < 0)
    {
        to = -to;
    }
    
    return align_value(val, to);
}