/**
 * Helper.c is a terrible name come up with a better one..
 */

#include "compiler.h"
#include "helpers/vector.h"
#include <assert.h>

/**
 * Returns the largest variable node for the given var_node.
 * If a variable representing a structure is passed to this function then the largest variable of that structure will be returned.
 * If its not a variable representing a structure then the passed var_node is returned
 * 
 * You must provide a variable node, in the future this function could be improved so you could pass
 * body nodes as well. However for now only variable nodes are supported
 * \param var_node The variable node that you wish to get the largest variable for
 */
struct node *variable_largest(struct node *var_node)
{
    assert(var_node->type == NODE_TYPE_VARIABLE);
    if (var_node->var.type.type != DATA_TYPE_STRUCT)
    {
        return var_node;
    }
    return variable_struct_node(var_node)->_struct.body_n->body.largest_var_node;
}

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
        // If the STRUCT_NO_OFFSET flag is set then we must not recalculate any positions.
        if (var_node_last)
        {
            position += variable_size(var_node_last);
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
 */
struct node *_struct_for_access(struct resolver_process *process, struct node *node, const char *type_str, int flags, struct struct_access_details *details_out)
{
    assert(details_out);
    FAIL_ERR("This function is no longer supported");
    struct node *var_node = NULL;
    if (node->type == NODE_TYPE_IDENTIFIER)
    {
        // No type? Then we have not located the first variable yet.
        if (type_str == NULL)
        {
            //   var_node = resolver_get_variable(process, node->sval)->node;
            details_out->first_node = var_node;
            return var_node;
        }

        details_out->offset = struct_offset(resolver_compiler(process), type_str, node->sval, &var_node, details_out->offset, flags);
        if (!details_out->first_node)
        {
            details_out->first_node = var_node;
        }
        return var_node;
    }
    else if (is_array_node(node))
    {
        var_node = _struct_for_access(process, node->exp.left, type_str, flags, details_out);
        return var_node;
    }
    var_node = _struct_for_access(process, node->exp.left, type_str, flags, details_out);
    if (details_out->flags & STRUCT_ACCESS_DETAILS_FLAG_NOT_FINISHED)
    {
        // We have to clone the node and swap the branches
        // i.e currently we might have [a.b]->c so we want the next_node to be seen as
        // [b->c] rather than just .b
        struct node *cloned_node = node_clone(node);
        cloned_node->exp.left = node->exp.left->exp.right;
        details_out->next_node = cloned_node;
        return var_node;
    }

    // Since we have pointer access and the pointer access flag is set
    // We must not continue.
    if (S_EQ(node->exp.op, "->") && flags & STRUCT_STOP_AT_POINTER_ACCESS)
    {
        details_out->next_node = node->exp.right;
        details_out->flags |= STRUCT_ACCESS_DETAILS_FLAG_NOT_FINISHED;
        return var_node;
    }

    struct node *ret_node = NULL;
    // Start of structure must be aligned in memory correctly
    details_out->offset = align_value_treat_positive(details_out->offset, variable_largest(var_node)->var.type.size);
    ret_node = _struct_for_access(process, node->exp.right, var_node->var.type.type_str, flags, details_out);

    return ret_node;
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
 */
struct node *struct_for_access(struct resolver_process *process, struct node *node, const char *type_str, int flags, struct struct_access_details *details_out)
{
    memset(details_out, 0, sizeof(struct struct_access_details));
    return _struct_for_access(process, node, type_str, flags, details_out);
}
bool is_access_node(struct node *node)
{
    return node->type == NODE_TYPE_EXPRESSION && is_access_operator(node->exp.op);
}

bool is_access_node_with_op(struct node *node, const char *op)
{
    return is_access_node(node) && S_EQ(node->exp.op, op);
}

bool is_access_operator(const char *op)
{
    return S_EQ(op, "->") || S_EQ(op, ".");
}

bool is_array_operator(const char *op)
{
    return S_EQ(op, "[]");
}

bool is_array_node(struct node *node)
{
    return node->type == NODE_TYPE_EXPRESSION && is_array_operator(node->exp.op);
}

static bool is_exp_compile_computable(struct node *node)
{
    if (S_EQ(node->exp.op, "->"))
    {
        return false;
    }

    if (!is_compile_computable(node->exp.left))
    {
        return false;
    }

    if (!is_compile_computable(node->exp.right))
    {
        return false;
    }

    return true;
}

bool is_compile_computable_unary(struct node *node)
{
    if (op_is_indirection(node->unary.op))
    {
        return false;
    }

    return is_compile_computable(node->unary.operand);
}

bool is_compile_computable(struct node *node)
{
    switch (node->type)
    {
    case NODE_TYPE_EXPRESSION:
        return is_exp_compile_computable(node);
        break;

    case NODE_TYPE_BRACKET:
        return is_compile_computable(node->bracket.inner);
        break;

    case NODE_TYPE_UNARY:
        return is_compile_computable_unary(node);
        break;
    }

    return node->type == NODE_TYPE_NUMBER || node->type == NODE_TYPE_IDENTIFIER;
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

static long compile_computable_value(struct node *node)
{
    long result = -1;
    switch (node->type)
    {
    case NODE_TYPE_NUMBER:
        result = node->llnum;
        break;

    default:
        // Change to a function.. Terrible..
        assert(0 == 1 && "Not compile computable, use is_compiler_computable next time!");
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

int array_offset(struct datatype *dtype, int index, int index_value)
{
    if (index == vector_count(dtype->array.brackets->n_brackets)-1)
        return index_value * dtype->size;

    vector_set_peek_pointer(dtype->array.brackets->n_brackets, index+1);
    
    int size_sum = index_value;
    struct node *bracket_node = vector_peek_ptr(dtype->array.brackets->n_brackets);
    while (bracket_node)
    {
        assert(bracket_node->bracket.inner->type == NODE_TYPE_NUMBER);
        int declared_index = bracket_node->bracket.inner->llnum;
        int size_value = declared_index;
        size_sum *= size_value;
        bracket_node = vector_peek_ptr(dtype->array.brackets->n_brackets);
    }

    return size_sum * dtype->size;
}