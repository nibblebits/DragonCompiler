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
int struct_offset(struct compile_process *compile_proc, const char *struct_name, const char *var_name, struct node **var_node_out, int last_pos, int flags)
{
    struct symbol *struct_sym = symresolver_get_symbol(compile_proc, struct_name);
    assert(struct_sym->type == SYMBOL_TYPE_NODE);

    struct node *node = struct_sym->data;
    assert(node->type == NODE_TYPE_STRUCT);

    struct vector *struct_vars_vec = node->_struct.body_n->body.statements;
    vector_set_peek_pointer(struct_vars_vec, 0);

    // Do we need to read the structure backwards. I.e
    // struct a
    // {
    //   int c;
    //   char b
    // }
    // Then we will read "b" then "c"
    if (flags & STRUCT_ACCESS_BACKWARDS)
    {
        // We need to go through this structure in reverse
        vector_set_peek_pointer_end(struct_vars_vec);
        vector_set_flag(struct_vars_vec, VECTOR_FLAG_PEEK_DECREMENT);
    }

    struct node *var_node_cur = vector_peek_ptr(struct_vars_vec);
    struct node *var_node_last = NULL;
    int position = last_pos;
    *var_node_out = NULL;
    while (var_node_cur)
    {
        *var_node_out = var_node_cur;

        if (var_node_last)
        {
            position += var_node_last->var.type.size;
            if (variable_node_is_primative(var_node_cur))
            {
                position = align_value_treat_positive(position, var_node_cur->var.type.size);
            }
            else
            {
                position = align_value_treat_positive(position, variable_struct_largest_variable_node(var_node_cur)->var.type.size);
            }
        }

        if (S_EQ(var_node_cur->var.name, var_name))
        {
            // Note we don't access the "offset" this is because the offset assume its a global
            // structure, it also has no concept of weather or not its nested insde a structure
            // its aware of only its self. Therefore offset is not fit for purpose
            break;
        }

        var_node_last = var_node_cur;
        var_node_cur = vector_peek_ptr(struct_vars_vec);
    }

    vector_unset_flag(struct_vars_vec, VECTOR_FLAG_PEEK_DECREMENT);

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
struct node *struct_for_access(struct compile_process *process, struct node *node, const char *type_str, int *offset_out, int flags)
{
    assert((node->type == NODE_TYPE_EXPRESSION && is_access_operator(node->exp.op)) || node->type == NODE_TYPE_IDENTIFIER);

    struct node *var_node = NULL;
    if (node->type == NODE_TYPE_IDENTIFIER)
    {
        *offset_out = struct_offset(process, type_str, node->sval, &var_node, *offset_out, flags);
        return var_node;
    }

    var_node = struct_for_access(process, node->exp.left, type_str, offset_out, flags);
    assert(var_node);

    struct node *ret_node = NULL;
    // Start of structure must be aligned in memory correctly
    *offset_out = align_value_treat_positive(*offset_out, variable_struct_node(var_node)->_struct.body_n->body.largest_var_node->var.type.size);
    ret_node = struct_for_access(process, node->exp.right, var_node->var.type.type_str, offset_out, flags);

    return ret_node;
}

bool is_access_operator(const char *op)
{
    return S_EQ(op, "->") || S_EQ(op, ".");
}

bool is_array_operator(const char *op)
{
    return S_EQ(op, "[]");
}

bool is_compile_computable(struct node *node)
{
    return node->type == NODE_TYPE_NUMBER;
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
    if (depth == DEPTH_INFINITE)
    {
        return DEPTH_INFINITE;
    }

    return depth - 1;
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

    struct node *left_node = NULL;
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

int padding(int val, int to)
{
    if ((val % to) == 0)
        return 0;

    return to - (val % to) % to;
}

int align_value(int val, int to)
{
    if (val % to)
    {
        val += padding(val, to);
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

void variable_align_offset(struct node *var_node, int *stack_offset_out)
{
    if ((*stack_offset_out + var_node->var.type.size) % DATA_SIZE_DWORD)
    {
        *stack_offset_out = align_value_treat_positive(*stack_offset_out, DATA_SIZE_DWORD);
    }
}

void var_node_set_offset(struct node *node, int offset)
{
    assert(node->type == NODE_TYPE_VARIABLE);
    node->var.offset = offset;
}

int compute_sum_padding(struct vector *vec)
{
    int padding = 0;
    int last_type = -1;
    bool mixed_types = false;
    vector_set_peek_pointer(vec, 0);
    struct node *cur_node = vector_peek_ptr(vec);
    struct node *last_node = NULL;
    while (cur_node)
    {
        if (cur_node->type != NODE_TYPE_VARIABLE)
        {
            cur_node = vector_peek_ptr(vec);
            continue;
        }

        padding += cur_node->var.padding;
        last_type = cur_node->var.type.type;
        last_node = cur_node;
        cur_node = vector_peek_ptr(vec);
    }

    return padding;
}

int compute_sum_padding_for_body(struct node *node)
{
    assert(node->type == NODE_TYPE_BODY);
    return compute_sum_padding(node->body.statements);
}

bool variable_node_is_primative(struct node *node)
{
    assert(node->type == NODE_TYPE_VARIABLE);
    return node->var.type.type != DATA_TYPE_STRUCT && node->var.type.type != DATA_TYPE_UNION;
}

size_t variable_size(struct node *var_node)
{
    if (var_node->var.type.flags & DATATYPE_FLAG_IS_ARRAY)
        return var_node->var.type.array.size;

    return var_node->var.type.size;
}

struct node *node_from_sym(struct symbol *sym)
{
    if (sym->type != SYMBOL_TYPE_NODE)
        return 0;

    struct node *node = sym->data;
    return node;
}

struct node *node_from_symbol(struct compile_process *current_process, const char *name)
{
    struct symbol *sym = symresolver_get_symbol(current_process, name);
    if (!sym)
    {
        return 0;
    }
    return node_from_sym(sym);
}

struct node *struct_node_for_name(struct compile_process *current_process, const char *struct_name)
{
    struct node *node = node_from_symbol(current_process, struct_name);
    if (!node)
        return NULL;

    if (node->type != NODE_TYPE_STRUCT)
        return NULL;

    return node;
}

struct node *variable_struct_node(struct node *var_node)
{
    if (var_node->type != NODE_TYPE_VARIABLE)
    {
        return NULL;
    }

    if (var_node->var.type.type != DATA_TYPE_STRUCT)
        return NULL;

    return var_node->var.type.struct_node;
}

bool variable_struct_padded(struct node *var_node)
{
    struct node *s_node = variable_struct_node(var_node);
    if (!s_node)
    {
        return false;
    }

    return s_node->_struct.body_n->body.padded;
}

struct node *body_largest_variable_node(struct node *body_node)
{
    if (!body_node)
        return NULL;

    if (body_node->type != NODE_TYPE_BODY)
    {
        return NULL;
    }

    return body_node->body.largest_var_node;
}

static long compile_computable_value(struct node* node)
{
    long result = -1;
    switch (node->type)
    {
        case NODE_TYPE_NUMBER:
            result = node->llnum;
        break;

        default:
            // Change to a function.. Terrible..
            assert(0==1 && "Not compile computable, use is_compiler_computable next time!");
    }

    return result;
}

struct node *variable_struct_largest_variable_node(struct node *var_node)
{
    return body_largest_variable_node(variable_struct_node(var_node)->_struct.body_n);
}

int compute_array_offset_with_multiplier(struct node *node, size_t single_element_size, size_t multiplier)
{
    // Ignore multiplier for now... We ill jsut return single_element_size * by value

    if (!is_compile_computable(node->bracket.inner))
    {
        return -1;
    }

    return compile_computable_value(node->bracket.inner);

}

int compute_array_offset(struct node *node, size_t single_element_size)
{
    return compute_array_offset_with_multiplier(node, single_element_size, 0);
}

