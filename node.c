#include "compiler.h"
#include <assert.h>
#include "helpers/vector.h"


bool node_is_struct_or_union_variable(struct node* node)
{
    if (node->type != NODE_TYPE_VARIABLE)
        return false;

    return datatype_is_struct_or_union(&node->var.type);
}

bool node_is_struct_or_union(struct node* node)
{
    return node->type == NODE_TYPE_STRUCT || node->type == NODE_TYPE_UNION;
}

/**
 * Returns true if this node can be used in an expression
 */
bool node_is_expressionable(struct node* node)
{
    return node->type == NODE_TYPE_EXPRESSION || node->type == NODE_TYPE_EXPRESSION_PARENTHESIS || node->type == NODE_TYPE_UNARY || node->type == NODE_TYPE_IDENTIFIER || node->type == NODE_TYPE_NUMBER;

}
bool node_is_expression_or_parentheses(struct node* node)
{
    return node->type == NODE_TYPE_EXPRESSION_PARENTHESIS || node->type == NODE_TYPE_EXPRESSION;
}

bool node_is_value_type(struct node* node)
{
    return node_is_expression_or_parentheses(node) || node->type == NODE_TYPE_IDENTIFIER || node->type == NODE_TYPE_NUMBER || node->type == NODE_TYPE_UNARY || node->type == NODE_TYPE_TENARY || node->type == NODE_TYPE_STRING;
}

const char* node_var_type_str(struct node* var_node)
{
    return var_node->var.type.type_str;
}

const char* node_var_name(struct node* var_node)
{
    return var_node->var.name;
}

bool is_pointer_node(struct node* node)
{
    if (node->type == NODE_TYPE_VARIABLE && node->var.type.flags & DATATYPE_FLAG_IS_POINTER)
    {
        return true;
    }

    return false;
}

struct vector* node_vector_clone(struct vector* vec)
{
    struct vector* new_vector = vector_create(sizeof(struct node*));
    vector_set_peek_pointer_end(vec);
    vector_set_flag(vec, VECTOR_FLAG_PEEK_DECREMENT);

    struct node* vec_node = vector_peek_ptr(vec);
    while(vec_node)
    {
        vector_push(new_vector, node_clone(vec_node));
        vec_node = vector_peek_ptr(vec);
    }

    return new_vector;
}

struct node* node_clone_memory(struct node* node)
{
    struct node* new_node = calloc(sizeof(struct node), 1);
    memcpy(new_node, node, sizeof(struct node));
    return new_node;
}

struct node* node_clone_identifier_or_number(struct node* node)
{
    struct node* new_node = node_clone_memory(node);
    return new_node;
}

struct node* node_clone_expression(struct node* node)
{
    struct node* new_exp_node = node_clone_memory(node);
    new_exp_node->exp.left = node_clone_memory(node->exp.left);
    new_exp_node->exp.right = node_clone_memory(node->exp.right);
    return new_exp_node;
}

struct node* node_clone_expression_parenthesis(struct node* node)
{
    struct node* new_exp_parenthesis_node = node_clone_memory(node);
    new_exp_parenthesis_node->parenthesis.exp = node_clone(node->parenthesis.exp);
    return new_exp_parenthesis_node;
}

struct node* node_clone_bracket(struct node* node)
{
    struct node* new_bracket_node = node_clone_memory(node);
    new_bracket_node->bracket.inner = node_clone(node->bracket.inner);
    return new_bracket_node;
}

struct node* node_clone_function(struct node* node)
{
    FAIL_ERR("We do not allow cloning of functions yet");
}

struct node* node_clone_return(struct node* node)
{
    struct node* return_node = node_clone_memory(node);
    return_node->stmt.ret.exp = node_clone_memory(node->stmt.ret.exp);
    return return_node;
}

struct node* node_clone_variable(struct node* node)
{
    struct node* variable_node = node_clone_memory(node);
    // struct_node and union_node share the union memory, only need to clone once.
    variable_node->var.type.struct_node = node_clone(node);
    variable_node->var.val = node_clone(node->var.val);
    return variable_node;
}

struct node* node_clone_struct(struct node* node)
{
    struct node* struct_node = node_clone_memory(node);
    struct_node->_struct.body_n = node_clone(node->_struct.body_n);
    return struct_node;
}

struct node* node_clone_body(struct node* node)
{
    struct node* body_node = node_clone_memory(node);
    body_node->body.largest_var_node = node_clone(node->body.largest_var_node);
    body_node->body.statements = node_vector_clone(node->body.statements);
    return body_node;
}
struct node* node_clone(struct node* node)
{
    if (!node)
        return NULL;

    struct node* cloned_node = NULL;
    switch(node->type)
    {
        case NODE_TYPE_IDENTIFIER:
        case NODE_TYPE_NUMBER:
            cloned_node = node_clone_identifier_or_number(node);
        break;

        case NODE_TYPE_EXPRESSION:
            cloned_node = node_clone_expression(node);
        break;

        case NODE_TYPE_EXPRESSION_PARENTHESIS:
            cloned_node = node_clone_expression_parenthesis(node);
        break;

        case NODE_TYPE_BRACKET:
            cloned_node = node_clone_bracket(node);
        break;

        case NODE_TYPE_FUNCTION:
            cloned_node = node_clone_function(node);
        break;

        case NODE_TYPE_STATEMENT_RETURN:
            cloned_node = node_clone_return(node);
        break;

        case NODE_TYPE_VARIABLE:
            cloned_node = node_clone_variable(node);
        break;

        case NODE_TYPE_STRUCT:
            cloned_node = node_clone_struct(node);
        break;
        default:
            FAIL_ERR("Node not supported for cloning");
    }
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

size_t node_sum_scope_size(struct node* node)
{
    if (!node->binded.owner)
    {
        return 0;
    }

    size_t result = node_sum_scope_size(node->binded.owner) + node->binded.owner->body.size;
    return result;
}

size_t function_node_stack_size(struct node* node)
{
    assert(node->type == NODE_TYPE_FUNCTION);
    return node->func.stack_size;
}

bool function_node_is_prototype(struct node* node)
{
    return node->func.body_n == NULL;
}