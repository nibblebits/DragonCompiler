#include "compiler.h"
#include <assert.h>
#include "helpers/vector.h"

// The current body that the parser is in
// Note: The set body may be uninitialized and should be used as reference only
// don't use functionality
struct node *parser_current_body = NULL;

// The current function we are in.
struct node *parser_current_function = NULL;

// The last token parsed by the parser, may be NULL
struct token *parser_last_token = NULL;
struct vector *node_vector = NULL;
struct vector *node_vector_root = NULL;

void node_set_vector(struct vector *vec, struct vector *root_vec)
{
    node_vector = vec;
    node_vector_root = root_vec;
}

void node_push(struct node *node)
{
    vector_push(node_vector, &node);
}

struct node *node_peek_or_null()
{
    return vector_back_ptr_or_null(node_vector);
}

/**
 * Peeks at the last node and if it can be used in an expression, 
 * then this function will return that node otherwise NULL.
 * 
 * Expressionable node types are: NODE_TYPE_NUMERIC, NODE_TYPE_EXPRESSION, NODE_TYPE_PARENTHESES, NODE_TYPE_IDENTIFIER
 */
struct node *node_peek_expressionable_or_null()
{
    struct node *last_node = node_peek_or_null();
    return node_is_expressionable(last_node) ? last_node : NULL;
}

/**
 * Peeks at the last node pushed to the stack
 */
struct node *node_peek()
{
    return *((struct node **)(vector_back(node_vector)));
}

/**
 * Pops the last node we pushed to the vector
 */
struct node *node_pop()
{
    struct node *last_node = vector_back_ptr(node_vector);
    struct node *last_node_root = vector_empty(node_vector_root) ? NULL : vector_back_ptr(node_vector_root);

    vector_pop(node_vector);

    if (last_node == last_node_root)
    {
        // We also have pushed this node to the tree root so we need to pop from here too
        vector_pop(node_vector_root);
    }

    return last_node;
}

void node_pop_remaining_push_to_root_vec()
{
    vector_set_peek_pointer(node_vector, 0);
    struct node *node = node_peek_or_null();
    if (node)
    {
        // Push the root element to the tree
        vector_push(node_vector_root, &node);
    }
}

/**
 * Peeks at the node on the node_tree_vec, root of the tree basically.
 * returns the next node the peek pointer is at then moves to the next node
 */
struct node **node_next()
{
    return vector_peek(node_vector_root);
}

void node_swap(struct node **f_node, struct node **s_node)
{
    struct node *tmp_node = *f_node;
    *f_node = *s_node;
    *s_node = tmp_node;
}

struct node *node_create(struct node *_node)
{
    struct node *node = malloc(sizeof(struct node));
    memcpy(node, _node, sizeof(struct node));
    node->binded.owner = parser_current_body;
    node->binded.function = parser_current_function;
    if (parser_last_token)
    {
        node->pos = parser_last_token->pos;
    }
    node_push(node);
    return node;
}

void make_for_node(struct node *init_node, struct node *cond_node, struct node *loop_node, struct node *body_node)
{
    node_create(&(struct node){NODE_TYPE_STATEMENT_FOR, .stmt._for.init = init_node, .stmt._for.cond = cond_node, .stmt._for.loop = loop_node, .stmt._for.body = body_node});
}

void make_case_node(struct node *exp_node)
{
    node_create(&(struct node){NODE_TYPE_STATEMENT_CASE, .stmt._case.exp = exp_node});
}

void make_default_node()
{
    node_create(&(struct node){NODE_TYPE_STATEMENT_DEFAULT});
}

void make_switch_node(struct node *exp_node, struct node *body_node, struct vector *cases, bool has_default_case)
{
    node_create(&(struct node){NODE_TYPE_STATEMENT_SWITCH, .stmt._switch.exp = exp_node, .stmt._switch.body = body_node, .stmt._switch.cases = cases, .stmt._switch.has_default_case = has_default_case});
}

void make_label_node(struct node *label_name_node)
{
    node_create(&(struct node){NODE_TYPE_LABEL, .label.name = label_name_node});
}
void make_goto_node(struct node *label_node)
{
    node_create(&(struct node){NODE_TYPE_STATEMENT_GOTO, .stmt._goto.label = label_node});
}

void make_tenary_node(struct node *true_result_node, struct node *false_result_node)
{
    node_create(&(struct node){NODE_TYPE_TENARY, .tenary.true_node = true_result_node, .tenary.false_node = false_result_node});
}

void make_exp_node(struct node *node_left, struct node *node_right, const char *op)
{
    // Expressions must have left and right operands..
    assert(node_left);
    assert(node_right);
    node_create(&(struct node){NODE_TYPE_EXPRESSION, .exp.op = op, .exp.left = node_left, .exp.right = node_right});
}

void make_exp_parentheses_node(struct node *exp_node)
{
    node_create(&(struct node){NODE_TYPE_EXPRESSION_PARENTHESIS, .parenthesis.exp = exp_node});
}

void make_break_node()
{
    node_create(&(struct node){NODE_TYPE_STATEMENT_BREAK});
}

void make_continue_node()
{
    node_create(&(struct node){NODE_TYPE_STATEMENT_CONTINUE});
}

void make_if_node(struct node *cond_node, struct node *body_node, struct node *next_node)
{
    node_create(&(struct node){NODE_TYPE_STATEMENT_IF, .stmt._if.cond_node = cond_node, .stmt._if.body_node = body_node, .stmt._if.next = next_node});
}

void make_while_node(struct node *cond_node, struct node *body_node)
{
    node_create(&(struct node){NODE_TYPE_STATEMENT_WHILE, .stmt._while.cond = cond_node, .stmt._while.body = body_node});
}

void make_do_while_node(struct node *body_node, struct node *cond_node)
{
    node_create(&(struct node){NODE_TYPE_STATEMENT_DO_WHILE, .stmt._do_while.cond = cond_node, .stmt._do_while.body = body_node});
}

void make_else_node(struct node *body_node)
{
    node_create(&(struct node){NODE_TYPE_STATEMENT_ELSE, .stmt._else.body_node = body_node});
}

void make_union_node(const char *struct_name, struct node *body_node)
{
    int flags = 0;
    if (!body_node)
    {
        // No body then we have forward declared.
        flags = NODE_FLAG_IS_FORWARD_DECLARATION;
    }

    node_create(&(struct node){NODE_TYPE_UNION, .flags = flags, ._union.name = struct_name, ._union.body_n = body_node});
}

void make_struct_node(const char *struct_name, struct node *body_node)
{
    int flags = 0;
    if (!body_node)
    {
        // No body then we have forward declared.
        flags = NODE_FLAG_IS_FORWARD_DECLARATION;
    }

    node_create(&(struct node){NODE_TYPE_STRUCT, .flags = flags, ._struct.name = struct_name, ._struct.body_n = body_node});
}

void make_unary_node(const char *unary_op, struct node *operand_node, int flags)
{
    node_create(&(struct node){NODE_TYPE_UNARY, .unary.op = unary_op, .unary.operand = operand_node, .unary.flags=flags});
}

void make_cast_node(struct datatype* dtype, struct node* operand_node)
{
    node_create(&(struct node){NODE_TYPE_CAST, .cast.dtype=*dtype, .cast.operand=operand_node});
}

void make_return_node(struct node *exp_node)
{
    node_create(&(struct node){NODE_TYPE_STATEMENT_RETURN, .stmt.ret.exp = exp_node});
}

void make_function_node(struct datatype *ret_type, const char *name, struct vector *arguments, struct node *body)
{
    struct node* function_node = node_create(&(struct node){NODE_TYPE_FUNCTION, .func.rtype = *ret_type, .func.name = name, .func.args.vector = arguments, .func.args.stack_addition=DATA_SIZE_DDWORD, .func.body_n = body});
    function_node->func.frame.elements = vector_create(sizeof(struct stack_frame_element));
}

void make_body_node(struct vector *body_vec, size_t size, bool padded, struct node *largest_var_node)
{
    node_create(&(struct node){NODE_TYPE_BODY, .body.statements = body_vec, .body.size = size, .body.padded = padded, .body.largest_var_node = largest_var_node});
}

bool node_valid(struct node* node)
{
    return node && node->type != NODE_TYPE_BLANK;
}

bool node_is_struct_or_union_variable(struct node *node)
{
    if (node->type != NODE_TYPE_VARIABLE)
        return false;

    return datatype_is_struct_or_union(&node->var.type);
}

bool node_is_struct_or_union(struct node *node)
{
    return node->type == NODE_TYPE_STRUCT || node->type == NODE_TYPE_UNION;
}

bool node_is_possibly_constant(struct node *node)
{
    return node->type == NODE_TYPE_IDENTIFIER || node->type == NODE_TYPE_NUMBER;
}

bool node_is_constant(struct resolver_process *process, struct node *node)
{
    if (!node)
    {
        return false;
    }
    
    if (!node_is_possibly_constant(node))
    {
        return false;
    }

    if (node->type == NODE_TYPE_IDENTIFIER)
    {
        struct resolver_result *result = resolver_follow(process, node);
        struct resolver_entity *entity = NULL;
        if (resolver_result_ok(result))
        {
            entity = resolver_result_entity(result);
            struct variable *var = &variable_node(entity->node)->var;
            if (var->type.flags & DATATYPE_FLAG_IS_CONST)
            {
                // Okay its constant
                return true;
            }
        }

        // Not a constant variable we cant use this.
        return false;
    }

    return true;
}

long node_pull_literal(struct resolver_process *process, struct node *node)
{
    if (!node_is_constant(process, node))
    {
        return -1;
    }

    if (node->type == NODE_TYPE_IDENTIFIER)
    {
        struct resolver_result *result = resolver_follow(process, node);
        struct resolver_entity *entity = NULL;
        if (resolver_result_ok(result))
        {
            entity = resolver_result_entity(result);
            struct variable *var = &variable_node(entity->node)->var;
            if (var->type.flags & DATATYPE_FLAG_IS_CONST)
            {
                // Okay its constant
                return var->val->llnum;
            }
        }
    }
    else if (node->type == NODE_TYPE_NUMBER)
    {
        return node->llnum;
    }
    return -1;
}

bool node_is_expression(struct node *node, const char *op)
{
    return node->type == NODE_TYPE_EXPRESSION && S_EQ(node->exp.op, op);
}

/**
 * Returns true if this node can be used in an expression
 */
bool node_is_expressionable(struct node *node)
{
    return node->type == NODE_TYPE_EXPRESSION || node->type == NODE_TYPE_EXPRESSION_PARENTHESIS || node->type == NODE_TYPE_UNARY || node->type == NODE_TYPE_IDENTIFIER || node->type == NODE_TYPE_NUMBER || node->type == NODE_TYPE_STRING;
}
bool node_is_expression_or_parentheses(struct node *node)
{
    return node->type == NODE_TYPE_EXPRESSION_PARENTHESIS || node->type == NODE_TYPE_EXPRESSION;
}

bool node_is_value_type(struct node *node)
{
    return node_is_expression_or_parentheses(node) || node->type == NODE_TYPE_IDENTIFIER || node->type == NODE_TYPE_NUMBER || node->type == NODE_TYPE_UNARY || node->type == NODE_TYPE_TENARY || node->type == NODE_TYPE_STRING;
}

const char *node_var_type_str(struct node *var_node)
{
    return var_node->var.type.type_str;
}

const char *node_var_name(struct node *var_node)
{
    return var_node->var.name;
}

bool is_pointer_node(struct node *node)
{
    if (node->type == NODE_TYPE_VARIABLE && node->var.type.flags & DATATYPE_FLAG_IS_POINTER)
    {
        return true;
    }

    return false;
}

struct vector *node_vector_clone(struct vector *vec)
{
    struct vector *new_vector = vector_create(sizeof(struct node *));
    vector_set_peek_pointer_end(vec);
    vector_set_flag(vec, VECTOR_FLAG_PEEK_DECREMENT);

    struct node *vec_node = vector_peek_ptr(vec);
    while (vec_node)
    {
        vector_push(new_vector, node_clone(vec_node));
        vec_node = vector_peek_ptr(vec);
    }

    return new_vector;
}

struct node *node_clone_memory(struct node *node)
{
    struct node *new_node = calloc(sizeof(struct node), 1);
    memcpy(new_node, node, sizeof(struct node));
    return new_node;
}

struct node *node_clone_identifier_or_number(struct node *node)
{
    struct node *new_node = node_clone_memory(node);
    return new_node;
}

struct node *node_clone_expression(struct node *node)
{
    struct node *new_exp_node = node_clone_memory(node);
    new_exp_node->exp.left = node_clone_memory(node->exp.left);
    new_exp_node->exp.right = node_clone_memory(node->exp.right);
    return new_exp_node;
}

struct node *node_clone_expression_parenthesis(struct node *node)
{
    struct node *new_exp_parenthesis_node = node_clone_memory(node);
    new_exp_parenthesis_node->parenthesis.exp = node_clone(node->parenthesis.exp);
    return new_exp_parenthesis_node;
}

struct node *node_clone_bracket(struct node *node)
{
    struct node *new_bracket_node = node_clone_memory(node);
    new_bracket_node->bracket.inner = node_clone(node->bracket.inner);
    return new_bracket_node;
}

struct node *node_clone_function(struct node *node)
{
    FAIL_ERR("We do not allow cloning of functions yet");
}

struct node *node_clone_return(struct node *node)
{
    struct node *return_node = node_clone_memory(node);
    return_node->stmt.ret.exp = node_clone_memory(node->stmt.ret.exp);
    return return_node;
}

struct node *node_clone_variable(struct node *node)
{
    struct node *variable_node = node_clone_memory(node);
    // struct_node and union_node share the union memory, only need to clone once.
    variable_node->var.type.struct_node = node_clone(node);
    variable_node->var.val = node_clone(node->var.val);
    return variable_node;
}

struct node *node_clone_struct(struct node *node)
{
    struct node *struct_node = node_clone_memory(node);
    struct_node->_struct.body_n = node_clone(node->_struct.body_n);
    return struct_node;
}

struct node *node_clone_body(struct node *node)
{
    struct node *body_node = node_clone_memory(node);
    body_node->body.largest_var_node = node_clone(node->body.largest_var_node);
    body_node->body.statements = node_vector_clone(node->body.statements);
    return body_node;
}
struct node *node_clone(struct node *node)
{
    if (!node)
        return NULL;

    struct node *cloned_node = NULL;
    switch (node->type)
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

size_t node_sum_scope_size(struct node *node)
{
    if (!node->binded.owner)
    {
        return 0;
    }

    size_t result = node_sum_scope_size(node->binded.owner) + node->binded.owner->body.size;
    return result;
}

size_t function_node_stack_size(struct node *node)
{
    assert(node->type == NODE_TYPE_FUNCTION);
    return node->func.stack_size;
}

bool function_node_is_prototype(struct node *node)
{
    return node->func.body_n == NULL;
}


struct node* node_find_inner(struct node* node)
{
    struct node* result_node= node;
    switch(node->type)
    {
        case NODE_TYPE_UNARY:
            result_node = node->unary.operand;
        break;
    }

    return result_node;
}

struct node *variable_node_or_list(struct node *node)
{
    if (node->type == NODE_TYPE_VARIABLE_LIST)
        return node;

    return variable_node(node);
}

struct node *variable_node(struct node *node)
{
    if (!node)
    {
        return NULL;
    }

    struct node *var_node = NULL;
    if (node->type == NODE_TYPE_VARIABLE)
    {
        var_node = node;
    }
    else if (node->type == NODE_TYPE_STRUCT)
    {
        var_node = node->_struct.var;
    }
    else if (node->type == NODE_TYPE_UNION)
    {
        var_node = node->_union.var;
    }

    return var_node;
}

struct node *node_function_get_final_argument(struct node *func_node)
{
    assert(func_node->type == NODE_TYPE_FUNCTION);
    vector_set_peek_pointer(function_node_argument_vec(func_node), 0);
    struct node *node = vector_peek_ptr(function_node_argument_vec(func_node));
    struct node *last_node = node;
    while (node)
    {
        last_node = node;
        node = vector_peek_ptr(function_node_argument_vec(func_node));
    }

    return last_node;
}


size_t function_node_argument_stack_addition(struct node* node)
{
    assert(node->type == NODE_TYPE_FUNCTION);
    return node->func.args.stack_addition;
}

struct vector* function_node_argument_vec(struct node* node)
{
    assert(node->type == NODE_TYPE_FUNCTION);
    return node->func.args.vector;
}

bool is_node_assignment(struct node *node)
{
    return S_EQ(node->exp.op, "=") ||
           S_EQ(node->exp.op, "+=") ||
           S_EQ(node->exp.op, "-=") ||
           S_EQ(node->exp.op, "*=") ||
           S_EQ(node->exp.op, "/=") ||
           S_EQ(node->exp.op, ">>=") ||
           S_EQ(node->exp.op, "<<=");
}

