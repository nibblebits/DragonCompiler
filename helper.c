/**
 * Helper.c is a terrible name come up with a better one..
 */

#include "compiler.h"
#include "helpers/vector.h"
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

long arithmetic(struct compile_process *compiler, long left_operand, long right_operand, const char *op, bool *success)
{
    *success = true;
    int result = 0;
    if (S_EQ(op, "*"))
    {
        result = left_operand * right_operand;
    }
    else if (S_EQ(op, "/"))
    {
        result = left_operand / right_operand;
    }
    else if (S_EQ(op, "+"))
    {
        result = left_operand + right_operand;
    }
    else if (S_EQ(op, "-"))
    {
        result = left_operand - right_operand;
    }
    else if (S_EQ(op, "=="))
    {
        result = left_operand == right_operand;
    }
    else if (S_EQ(op, "!="))
    {
        result = left_operand != right_operand;
    }
    else if (S_EQ(op, ">"))
    {
        result = left_operand > right_operand;
    }
    else if (S_EQ(op, "<"))
    {
        result = left_operand < right_operand;
    }
    else if (S_EQ(op, ">="))
    {
        result = left_operand >= right_operand;
    }
    else if (S_EQ(op, "<="))
    {
        result = left_operand <= right_operand;
    }
    else if (S_EQ(op, "<<"))
    {
        result = left_operand << right_operand;
    }
    else if (S_EQ(op, ">>"))
    {
        result = left_operand >> right_operand;
    }
    else if (S_EQ(op, "&&"))
    {
        result = left_operand && right_operand;
    }
    else if (S_EQ(op, "||"))
    {
        result = left_operand || right_operand;
    }
    else
    {
        *success = false;
    }

    // Unary operators will be hanlded later...
    // Forgot to handle them.

    return result;
}

bool datatype_is_struct_or_union(struct datatype *dtype)
{
    return dtype->type == DATA_TYPE_STRUCT || dtype->type == DATA_TYPE_UNION;
}

bool datatype_is_struct_or_union_for_name(const char *name)
{
    return S_EQ(name, "struct") || S_EQ(name, "union");
}

bool is_pointer_datatype(struct datatype *dtype)
{
    return dtype->pointer_depth > 0;
}

bool datatype_is_non_pointer_struct(struct datatype *dtype)
{
    return dtype->type == DATA_TYPE_STRUCT && !is_pointer_datatype(dtype);
}
bool is_hex_char(char c)
{
    c = tolower(c);
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

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
    return variable_struct_or_union_body_node(var_node)->body.largest_var_node;
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

    // We will allow union access here as they share similar aspects as structures.
    assert(node_is_struct_or_union(node));

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

    struct node *var_node_cur = variable_node(vector_peek_ptr(struct_vars_vec));
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
                position = align_value_treat_positive(position, variable_struct_or_union_largest_variable_node(var_node_cur)->var.type.size);
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
        var_node_cur = variable_node(vector_peek_ptr(struct_vars_vec));
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


void datatype_set_void(struct datatype* dtype)
{
    dtype->type_str = "void";
    dtype->type = DATA_TYPE_VOID;
    dtype->size = 0;
}

bool datatype_is_void_no_ptr(struct datatype *dtype)
{
    return S_EQ(dtype->type_str, "void") && !(dtype->flags & DATATYPE_FLAG_IS_POINTER);
}

struct datatype *datatype_for_final_node(struct node *node)
{
    switch (node->type)
    {
    case NODE_TYPE_IDENTIFIER:

        break;
    }
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

// Consider a better name for this.

/**
 * Returns true if this operator is special.
 * Special operators require instructions that could break the flow of registers
 * examples include multiplication and division.
 */
bool is_special_operator(const char *op)
{
    return S_EQ(op, "*") || S_EQ(op, "/") || is_bitwise_operator(op);
}

bool is_special_node(struct node *node)
{
    return node->type == NODE_TYPE_EXPRESSION && is_special_operator(node->exp.op);
}

bool is_bitwise_operator(const char *op)
{
    return S_EQ(op, "^") || S_EQ(op, "<<") || S_EQ(op, ">>") || S_EQ(op, "&") || S_EQ(op, "|");
}

bool is_logical_operator(const char *op)
{
    return S_EQ(op, "&&") || S_EQ(op, "||");
}

bool is_logical_node(struct node *node)
{
    return node->type == NODE_TYPE_EXPRESSION && is_logical_operator(node->exp.op);
}

bool is_argument_operator(const char *op)
{
    return S_EQ(op, ",");
}

bool is_argument_node(struct node *node)
{
    return node->type == NODE_TYPE_EXPRESSION && is_argument_operator(node->exp.op);
}

bool is_array_node(struct node *node)
{
    return node->type == NODE_TYPE_EXPRESSION && is_array_operator(node->exp.op);
}

bool is_parentheses_operator(const char *op)
{
    return S_EQ(op, "()");
}

bool is_parentheses_node(struct node *node)
{
    return node->type == NODE_TYPE_EXPRESSION && is_parentheses_operator(node->exp.op);
}

bool is_operator_token(struct token *token)
{
    return token && token->type == TOKEN_TYPE_OPERATOR;
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

bool op_is_address(const char *op)
{
    return S_EQ(op, "&");
}

int padding(int val, int to)
{
    // We cannot deal with zero., therefore zero padding.
    if (to <= 0)
        return 0;

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

bool datatype_is_primitive_for_string(const char *type)
{
    return S_EQ(type, "void") || S_EQ(type, "char") || S_EQ(type, "short") || S_EQ(type, "int") || S_EQ(type, "long") || S_EQ(type, "float") || S_EQ(type, "double");
}

bool datatype_is_primitive_non_pointer(struct datatype *dtype)
{
    return datatype_is_primitive(dtype) && !(dtype->flags & DATATYPE_FLAG_IS_POINTER);
}

bool datatype_is_struct_or_union_non_pointer(struct datatype *dtype)
{
    return dtype->type != DATA_TYPE_UNKNOWN && !datatype_is_primitive(dtype) && !(dtype->flags & DATATYPE_FLAG_IS_POINTER);
}

bool datatype_is_primitive(struct datatype *dtype)
{
    return datatype_is_primitive_for_string(dtype->type_str);
}

void datatype_decrement_pointer(struct datatype *dtype)
{
    dtype->pointer_depth--;
    if (dtype->pointer_depth <= 0)
    {
        dtype->flags &= ~DATATYPE_FLAG_IS_POINTER;
    }
}

bool variable_node_is_primative(struct node *node)
{
    // It is possible that you can declare structures as well as defining
    // a variable for them, therefore this has to be allowed.
    assert(node->type == NODE_TYPE_VARIABLE);
    return datatype_is_primitive(&node->var.type);
}

struct datatype *datatype_pointer_reduce(struct datatype *datatype, int by)
{
    struct datatype *new_datatype = calloc(1, sizeof(struct datatype));
    memcpy(new_datatype, datatype, sizeof(struct datatype));
    new_datatype->pointer_depth -= by;
    if (new_datatype->pointer_depth <= 0)
    {
        new_datatype->flags &= ~DATATYPE_FLAG_IS_POINTER;
        new_datatype->pointer_depth = 0;
    }
    return new_datatype;
}

size_t datatype_size_no_ptr(struct datatype *datatype)
{
    if (datatype->flags & DATATYPE_FLAG_IS_ARRAY)
        return datatype->array.size;
    return datatype->size;
}

size_t datatype_size_for_array_access(struct datatype *datatype)
{
    if (datatype_is_struct_or_union(datatype) && datatype->flags & DATATYPE_FLAG_IS_POINTER && datatype->pointer_depth == 1)
    {
        // We have something like this struct dog* abc; abc[1]
        // We should return the datatype size even though its a pointer.
        return datatype->size;
    }

    return datatype_size(datatype);
}

off_t datatype_offset_for_identifier(struct compile_process* compiler, struct datatype* struct_union_datatype, struct node* identifier_node, struct datatype* datatype_out, off_t current_offset)
{
    assert(datatype_is_struct_or_union(struct_union_datatype));
    assert(identifier_node->type == NODE_TYPE_IDENTIFIER);
    off_t offset = current_offset;
    struct symbol* sym = symresolver_get_symbol(compiler, struct_union_datatype->type_str);
    assert(sym);
    assert(sym->type == SYMBOL_TYPE_NODE);
    struct node* symbol_node = sym->data;
    assert(node_is_struct_or_union(symbol_node));

    // Okay we have the structure/union _struct can be used since the substructures are the same
    struct node* body_node = symbol_node->_struct.body_n;
    vector_set_peek_pointer(body_node->body.statements, 0);
    struct node* statement_node = vector_peek_ptr(body_node->body.statements);
    while(statement_node)
    {
        if (statement_node->type == NODE_TYPE_VARIABLE)
        {
            if (S_EQ(statement_node->var.name, identifier_node->sval))
            {
                if (datatype_out)
                {
                    *datatype_out = statement_node->var.type;
                }
                break;
            }
            offset += variable_size(statement_node);
        }

        statement_node = vector_peek_ptr(body_node->body.statements);
    }

    return offset;
}

off_t _datatype_offset(struct compile_process* compiler, off_t current_offset, struct datatype* struct_union_datatype, struct node* member_node, struct datatype* datatype_out);

off_t datatype_offset_for_expression(struct compile_process* compiler, struct datatype* struct_union_datatype, struct node* expression_node, off_t current_offset, struct datatype* datatype_out)
{
    off_t offset = current_offset;
    assert(datatype_is_struct_or_union(struct_union_datatype));
    assert(expression_node->type == NODE_TYPE_EXPRESSION);
    struct node* left_node = expression_node->exp.left;
    struct node* right_node = expression_node->exp.right;
    struct datatype left_datatype = {0};

    offset = _datatype_offset(compiler, offset, struct_union_datatype, left_node, &left_datatype);
    assert(datatype_is_struct_or_union(&left_datatype));

    // Change the structure union datatype to the left type
    offset = _datatype_offset(compiler, offset, &left_datatype, right_node, datatype_out);
    return offset;
}
off_t _datatype_offset(struct compile_process* compiler, off_t current_offset, struct datatype* struct_union_datatype, struct node* member_node,  struct datatype* datatype_out)
{
    assert(datatype_is_struct_or_union(struct_union_datatype));
    off_t offset = current_offset;
    switch(member_node->type)
    {
        case NODE_TYPE_EXPRESSION:
            offset = datatype_offset_for_expression(compiler, struct_union_datatype, member_node, offset, datatype_out);

        break;

        case NODE_TYPE_EXPRESSION_PARENTHESIS:
            compiler_error(compiler, "Parenthesis is not allowed in a structure/union member offsetof expression");
        break;

        case NODE_TYPE_IDENTIFIER:
           offset = datatype_offset_for_identifier(compiler, struct_union_datatype, member_node, datatype_out, offset);
        break;
    }

    return offset;
}
off_t datatype_offset(struct compile_process* compiler, struct datatype* datatype, struct node* member_node)
{
    off_t offset = 0;

    // We can only make offsets from within structures, if a non structure
    // is provided then return zero
    if (!datatype_is_struct_or_union(datatype))
    {
       return 0;
    }

    struct datatype datatype_of_member = {0};
    offset =_datatype_offset(compiler, 0, datatype, member_node, &datatype_of_member);
    
    off_t aligned_offset = offset;
    if (datatype_size(&datatype_of_member) > 0)
    {
        aligned_offset = align_value(offset, datatype_size(&datatype_of_member));
    }
    return aligned_offset;
}

size_t datatype_size(struct datatype *datatype)
{
    if (datatype->flags & DATATYPE_FLAG_IS_POINTER && datatype->pointer_depth > 0)
        return DATA_SIZE_DWORD;

    if (datatype->flags & DATATYPE_FLAG_IS_ARRAY)
        return datatype->array.size;
    return datatype->size;
}

size_t datatype_element_size(struct datatype *datatype)
{
    if (datatype->flags & DATATYPE_FLAG_IS_POINTER)
        return DATA_SIZE_DWORD;

    return datatype->size;
}

/**
 * @brief Returns a numerical datatype for the default datatype of "int" for numerical numbers.
 * 
 * @return struct datatype The "int" datatype with a size of 4 bytes.
 */
struct datatype datatype_for_numeric()
{
    struct datatype dtype = {};
    dtype.flags |= DATATYPE_FLAG_IS_LITERAL;
    dtype.type = DATA_TYPE_INTEGER;
    dtype.type_str = "int";
    dtype.size = DATA_SIZE_DWORD;
    return dtype;
}

struct datatype *datatype_thats_a_pointer(struct datatype *d1, struct datatype *d2)
{
    if (d1->flags & DATATYPE_FLAG_IS_POINTER)
    {
        return d1;
    }
    else if (d2->flags & DATATYPE_FLAG_IS_POINTER)
    {
        return d2;
    }

    return NULL;
}

/**
 * @brief Returns a datatype for strings.
 * 
 * @return struct datatype The "char*" datatype with pointer flag on
 */
struct datatype datatype_for_string()
{
    struct datatype dtype = {};
    dtype.type = DATA_TYPE_INTEGER;
    dtype.type_str = "char";
    dtype.flags |= DATATYPE_FLAG_IS_POINTER | DATATYPE_FLAG_IS_LITERAL;
    dtype.pointer_depth = 1;
    dtype.size = DATA_SIZE_DWORD;
    return dtype;
}

size_t variable_size(struct node *var_node)
{
    return datatype_size(&var_node->var.type);
}

size_t variable_size_for_list(struct node *var_list_node)
{
    size_t size = 0;
    assert(var_list_node->type == NODE_TYPE_VARIABLE_LIST);
    vector_set_peek_pointer(var_list_node->var_list.list, 0);
    struct node *var_node = vector_peek_ptr(var_list_node->var_list.list);
    while (var_node)
    {
        size += variable_size(var_node);
        var_node = vector_peek_ptr(var_list_node->var_list.list);
    }

    return size;
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

struct node *union_node_for_name(struct compile_process *current_process, const char *struct_name)
{
    struct node *node = node_from_symbol(current_process, struct_name);
    if (!node)
        return NULL;

    if (node->type != NODE_TYPE_UNION)
        return NULL;

    return node;
}

struct node *struct_or_union_declaration_variable(struct node *node)
{
    assert(node->type == NODE_TYPE_STRUCT || node->type == NODE_TYPE_UNION);

    if (node->type == NODE_TYPE_STRUCT)
    {
        return node->_struct.var;
    }

    return node->_union.var;
}

struct node *variable_struct_or_union_body_node(struct node *var_node)
{
    if (var_node->type != NODE_TYPE_VARIABLE && !node_is_struct_or_union(var_node))
    {
        return NULL;
    }

    if (!datatype_is_struct_or_union(&var_node->var.type))
    {
        return NULL;
    }

    if (var_node->var.type.type == DATA_TYPE_STRUCT)
    {
        if (!var_node->var.type.struct_node)
        {
            printf("issue");
        }
        return var_node->var.type.struct_node->_struct.body_n;
    }
    return var_node->var.type.union_node->_union.body_n;
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

bool variable_padded(struct node *var_node)
{
    struct node *node = variable_struct_or_union_body_node(var_node);
    if (!node)
    {
        return false;
    }

    return node->body.padded;
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

struct node *variable_struct_or_union_largest_variable_node(struct node *var_node)
{
    return body_largest_variable_node(variable_struct_or_union_body_node(var_node));
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

int array_multiplier(struct datatype *dtype, int index, int index_value)
{
    if (!(dtype->flags & DATATYPE_FLAG_IS_ARRAY))
    {
        // No array here? then just return the index value.
        return index_value;
    }

    vector_set_peek_pointer(dtype->array.brackets->n_brackets, index + 1);

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

    return size_sum;
}

int array_offset(struct datatype *dtype, int index, int index_value)
{
    if (!(dtype->flags & DATATYPE_FLAG_IS_ARRAY) ||
        (index == vector_count(dtype->array.brackets->n_brackets) - 1))
    {
        return index_value * datatype_element_size(dtype);
    }
    return array_multiplier(dtype, index, index_value) * datatype_element_size(dtype);
}

size_t array_brackets_count(struct datatype *dtype)
{
    return vector_count(dtype->array.brackets->n_brackets);
}

bool is_parentheses(const char *s)
{
    return (S_EQ(s, "("));
}

bool unary_operand_compatiable(struct token *token)
{
    return is_access_operator(token->sval) ||
           is_array_operator(token->sval) ||
           is_parentheses(token->sval);
}

bool char_is_delim(char c, const char *delims)
{
    int len = strlen(delims);
    for (int i = 0; i < len; i++)
    {
        if (c == delims[i])
            return true;
    }

    return false;
}

bool is_pointer_array_access(struct datatype *dtype, int index)
{
    return !(dtype->flags & DATATYPE_FLAG_IS_ARRAY) || array_total_indexes(dtype) <= index;
}

struct datatype *datatype_get(struct node *node)
{
    struct datatype *dtype = NULL;
    switch (node->type)
    {
    case NODE_TYPE_VARIABLE:
        dtype = &node->var.type;
        break;

    case NODE_TYPE_FUNCTION:
        dtype = &node->func.rtype;
        break;
    }
    return dtype;
}