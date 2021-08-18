#include "compiler.h"
#include "misc.h"
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <stdarg.h>
#include <memory.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

/**
 * Specifies that this part of the history is a non coneable entity,
 * the memory will be created on the heap to prevent it being cloned
 */
#define NON_CLONEABLE_HISTORY_VARIABLE(type, name) type *name;

/**
 * Accesses this non cloneable history variable value
 */
#define NON_CLONEABLE_HISTORY_VARIABLE_ACCESS(name) *name

/**
 * Initializes the non cloneable history variable by creating new memory on the heap
 * This can safetly be passed down the stack without being cloned as the pointer
 * will remain in tact on the heap
 */
#define NON_CLONEABLE_HISTORY_VARIABLE_INITIALIZE(name) name = calloc(sizeof(*name), 1)

// The current body that the parser is in
// Note: The set body may be uninitialized and should be used as reference only
// don't use functionality
struct node *parser_current_body = NULL;

// The current function we are in.
struct node *parser_current_function = NULL;

// First in the array = higher priority
// This array is special, its essentially a group of arrays

#define TOTAL_OPERATOR_GROUPS 14
#define MAX_OPERATORS_IN_GROUP 12

// Flags to represent history to try to make it easier to know what stage the parse process is at.
enum
{
    // Signifies we are at the right operand of a structure access. I.e a.b "b" would be the right operand
    HISTORY_FLAG_STRUCTURE_ACCESS_RIGHT_OPERAND = 0b00000001,
    // Signifies the current code flow is inside of a structure right now.
    // We are parsing a structure at this exact moment in time if this flag is set.
    HISTORY_FLAG_INSIDE_STRUCTURE = 0b00000010,
    // Specifies that we are outside of a function in the parsing process
    // You can think of this as where global variables would be.
    // Does not include situations where we are inside of a structure
    HISTORY_FLAG_IS_GLOBAL_SCOPE = 0b00000100,
    // This flag is set if the stack is growing upwards, i.e function arguments
    HISTORY_FLAG_IS_UPWARD_STACK = 0b00001000,
    HISTORY_FLAG_IS_EXPRESSION = 0b00010000,
    HISTORY_FLAG_IN_SWITCH_STATEMENT = 0b00100000,
    HISTORY_FLAG_INSIDE_FUNCTION_BODY = 0b01000000
};

// Expression flags

enum
{
    ASSOCIATIVITY_LEFT_TO_RIGHT,
    ASSOCIATIVITY_RIGHT_TO_LEFT
};
struct op_precedence_group
{
    char *operators[MAX_OPERATORS_IN_GROUP];
    int associativity;
};

/**
 * Format: 
 * {operator1, operator2, operator3, NULL}
 * 
 * end each group with NULL.
 * 
 * Also end the collection of groups with a NULL pointer
 */
static struct op_precedence_group op_precedence[TOTAL_OPERATOR_GROUPS] = {
    {.operators = {"++", "--", "()", "[]", "(", "[", "]", ".", "->", NULL}, .associativity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"*", "/", "%%", NULL}, .associativity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"+", "-", NULL}, .associativity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"<<", ">>", NULL}, .associativity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"<", "<=", ">", ">=", NULL}, .associativity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"==", "!=", NULL}, .associativity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"&", NULL}, .associativity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"^", NULL}, .associativity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"|", NULL}, .associativity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"&&", NULL}, .associativity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"||", NULL}, .associativity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"?", ":", NULL}, .associativity = ASSOCIATIVITY_RIGHT_TO_LEFT},
    {.operators = {"=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=", NULL}, .associativity = ASSOCIATIVITY_RIGHT_TO_LEFT},
    {.operators = {",", NULL}, .associativity = ASSOCIATIVITY_LEFT_TO_RIGHT},
};

struct history_cases
{
    // A vector of struct parsed_switch_case
    struct vector *cases;
    // Do we have a default case in the switch statement
    bool has_default_case;
};

struct history
{
    // Flags for this history.
    int flags;

    struct parser_history_switch
    {
        // We don't want our history cases to be cloned down stream
        // This way we can change the memory where ever we are in the parse process.
        NON_CLONEABLE_HISTORY_VARIABLE(struct history_cases, case_data);
    } _switch;
};

static struct compile_process *current_process;
int parse_next();
void parse_statement(struct history *history);

static struct history *history_down(struct history *history, int flags)
{
    struct history *new_history = calloc(sizeof(struct history), 1);
    history->flags = flags;
    memcpy(new_history, history, sizeof(struct history));
    return new_history;
}

static struct history *history_begin(struct history *history_out, int flags)
{
    struct history *new_history = calloc(sizeof(struct history), 1);
    new_history->flags = flags;
    return new_history;
}

#define parse_err(...) \
    compiler_error(current_process, __VA_ARGS__)

#define parser_err(...) \
    compiler_error(current_process, __VA_ARGS__)

#define parser_scope_new() \
    scope_new(current_process, 0)

#define parser_scope_finish() \
    scope_finish(current_process)

#define parser_scope_push(value, elem_size) \
    scope_push(current_process, value, elem_size)

#define parser_scope_last_entity() \
    scope_last_entity(current_process)

#define parser_scope_current() \
    scope_current(current_process)

struct parser_history_switch parser_new_switch_statement(struct history *history)
{
    memset(&history->_switch, 0x00, sizeof(history->_switch));
    NON_CLONEABLE_HISTORY_VARIABLE_INITIALIZE(history->_switch.case_data);
    history->_switch.case_data->cases = vector_create(sizeof(struct parsed_switch_case));
    history->flags |= HISTORY_FLAG_IN_SWITCH_STATEMENT;
    return history->_switch;
}

void parser_end_switch_statement(struct parser_history_switch *switch_history)
{
    // Do nothing.
}

void parser_register_case(struct history *history, struct node *case_node)
{
    assert(history->flags & HISTORY_FLAG_IN_SWITCH_STATEMENT);
    struct parsed_switch_case scase;
    scase.index = case_node->stmt._case.exp->llnum;
    vector_push(history->_switch.case_data->cases, &scase);
}

// Simple bitmask for scope entity rules.
enum
{
    PARSER_SCOPE_ENTITY_ON_STACK = 0b00000001,
    PARSER_SCOPE_ENTITY_STRUCTURE_SCOPE = 0b00000010,
};

struct parser_scope_entity
{
    // The flags for this scope entity
    int flags;

    // The stack offset this scope entity can be accessed at.
    // i.e -4, -8 -12
    // If this scope entity has no stack entity as its a global scope
    // then this value should be ignored.
    int stack_offset;

    // A node to a variable declaration.
    struct node *node;
};

struct parser_scope_entity *parser_new_scope_entity(struct node *node, int stack_offset, int flags)
{
    struct parser_scope_entity *entity = calloc(sizeof(struct parser_scope_entity), 1);
    entity->node = node;
    entity->flags = flags;
    entity->stack_offset = stack_offset;
    return entity;
}

void parser_scope_offset_for_stack(struct node *node, struct history *history)
{
    struct parser_scope_entity *last_entity = parser_scope_last_entity();
    bool upward_stack = history->flags & HISTORY_FLAG_IS_UPWARD_STACK;
    int offset = -variable_size(node);
    if (upward_stack)
    {
        // Do not use the variable size for the offset on an upward stack.
        // The reason is because PUSH instructions consume 4 bytes of the stack,
        // therefore we cannot trust anything that does not modulo into four.
        // We will have to access everything as an integer and cast it down into
        // the correct data type.

        // The correct offset is 4 bytes since 32 bit arch's PUSH is 4 bytes long.
        offset = DATA_SIZE_DWORD;

        // Are we also the first entity? If so then the offset must start at EIGHT Bytes
        if (!last_entity)
        {
            // I don't like this solution think of something better.
            offset += DATA_SIZE_DWORD;
        }
    }

    if (last_entity)
    {
        offset += last_entity->node->var.aoffset;
        if (variable_node_is_primative(node))
        {
            node->var.padding = padding(upward_stack ? offset : -offset, node->var.type.size);
            last_entity->node->var.padding_after = node->var.padding;
        }
    }

    bool first_entity = !last_entity;

    // If this is a structure variable then we must align the padding to a 4-byte boundary so long
    // as their was any padding in the original structure scope
    // \attention Maybe make a new function for second operand, a bit long...
    if (!variable_node_is_primative(node) && variable_struct_node(node)->_struct.body_n->body.padded)
    {
        node->var.padding = padding(upward_stack ? offset : -offset, DATA_SIZE_DWORD);
    }
    node->var.aoffset = offset + (upward_stack ? node->var.padding : -node->var.padding);
}

void parser_scope_offset_for_structure(struct node *node, struct history *history)
{
    int offset = 0;
    struct parser_scope_entity *last_entity = parser_scope_last_entity();

    if (last_entity)
    {
        offset += last_entity->stack_offset + last_entity->node->var.type.size;
        if (variable_node_is_primative(node))
        {
            node->var.padding = padding(offset, node->var.type.size);
            last_entity->node->var.padding_after = node->var.padding;
        }
        node->var.aoffset = offset + node->var.padding;
    }
}

int parser_scope_offset_for_global(struct node *node, struct history *history)
{
    // We don't have global scopes. Each variable is independant
    return 0;
}

void parser_scope_offset(struct node *node, struct history *history)
{

    if (history->flags & HISTORY_FLAG_INSIDE_STRUCTURE)
    {
        parser_scope_offset_for_structure(node, history);
        return;
    }

    if (history->flags & HISTORY_FLAG_IS_GLOBAL_SCOPE)
    {
        parser_scope_offset_for_global(node, history);
        return;
    }

    parser_scope_offset_for_stack(node, history);
}

static int parser_get_precedence_for_operator(const char *op, struct op_precedence_group **group_out)
{
    *group_out = NULL;
    for (int i = 0; i < TOTAL_OPERATOR_GROUPS; i++)
    {
        for (int b = 0; op_precedence[i].operators[b]; b++)
        {
            const char *_op = op_precedence[i].operators[b];
            if (S_EQ(op, _op))
            {
                *group_out = &op_precedence[i];
                return i;
            }
        }
    }

    return -1;
}

static bool parser_left_op_has_priority(const char *op_left, const char *op_right)
{
    struct op_precedence_group *group_left = NULL;
    struct op_precedence_group *group_right = NULL;

    // Same operator? Then they have equal priority!
    if (S_EQ(op_left, op_right))
        return false;

    int precedence_left = parser_get_precedence_for_operator(op_left, &group_left);
    int precedence_right = parser_get_precedence_for_operator(op_right, &group_right);
    if (group_left->associativity == ASSOCIATIVITY_RIGHT_TO_LEFT)
    {
        // Right to left associativity in the left group? and right group left_to_right?
        // Then right group takes priority
        return false;
    }

    return precedence_left <= precedence_right;
}

static bool parser_ops_equal_priority(const char *op_left, const char *op_right)
{
    struct op_precedence_group *group_left = NULL;
    struct op_precedence_group *group_right = NULL;
    int precedence_left = parser_get_precedence_for_operator(op_left, &group_left);
    int precedence_right = parser_get_precedence_for_operator(op_right, &group_right);
    if (group_left->associativity == ASSOCIATIVITY_RIGHT_TO_LEFT)
    {
        // Right to left associativity in the left group? and right group left_to_right?
        // Then right group takes priority
        return false;
    }

    return precedence_left == precedence_right;
}
static bool parser_is_unary_operator(const char *op)
{
    return is_unary_operator(op);
}

static bool token_is_nl_or_comment(struct token* token)
{
    return token->type == TOKEN_TYPE_NEWLINE || token->type == TOKEN_TYPE_COMMENT;
}

static struct token *token_next()
{
    struct token *next_token = vector_peek_no_increment(current_process->token_vec);
    while (next_token && token_is_nl_or_comment(next_token))
    {
        vector_peek(current_process->token_vec);
        // The parser does not care about new lines, only the preprocessor has to care about that
        next_token = vector_peek_no_increment(current_process->token_vec);
    }
    return vector_peek(current_process->token_vec);
}

static struct token *token_peek_next()
{
    struct token *next_token = vector_peek_no_increment(current_process->token_vec);
    while (next_token && token_is_nl_or_comment(next_token))
    {
        vector_peek(current_process->token_vec);
        // The parser does not care about new lines, only the preprocessor has to care about that
        next_token = vector_peek_no_increment(current_process->token_vec);
    }
    return vector_peek_no_increment(current_process->token_vec);
}

static bool token_next_is_operator(const char *op)
{
    struct token *token = token_peek_next();
    return token_is_operator(token, op);
}

static bool token_next_is_keyword(const char *keyword)
{
    struct token *token = token_peek_next();
    return token_is_keyword(token, keyword);
}

static bool token_next_is_symbol(char sym)
{
    struct token *token = token_peek_next();
    return token_is_symbol(token, sym);
}

static struct token *token_next_expected(int type)
{
    struct token *token = token_next();

    if (token->type != type)
        parse_err("Unexpected token\n");

    return token;
}

int parser_get_pointer_depth()
{
    int depth = 0;
    while (token_next_is_operator("*"))
    {
        depth += 1;
        token_next();
    }

    return depth;
}

static void expect_sym(char c)
{
    struct token *next_token = token_next();
    if (next_token == NULL || next_token->type != TOKEN_TYPE_SYMBOL || next_token->cval != c)
        parse_err("Expecting the symbol %c but something else was provided", c);
}

static void expect_op(const char *op)
{
    struct token *next_token = token_next();
    if (next_token == NULL || next_token->type != TOKEN_TYPE_OPERATOR || !S_EQ(next_token->sval, op))
        parse_err("Expecting the operator %s but something else was provided", op);
}

static void expect_keyword(const char *keyword)
{
    struct token *next_token = token_next();
    assert(next_token);
    assert(next_token->type == TOKEN_TYPE_KEYWORD);
    assert(S_EQ(next_token->sval, keyword));

    if (next_token == NULL || next_token->type != TOKEN_TYPE_KEYWORD || !S_EQ(next_token->sval, keyword))
        parse_err("Expecting keyword %s however something else was provided", keyword);
}

void token_read_dots(size_t total)
{
    for (int i = 0; i < total; i++)
    {
        expect_op(".");
    }
}

static struct node *node_peek_or_null()
{
    return vector_back_ptr_or_null(current_process->node_vec);
}

/**
 * Peeks at the last node pushed to the stack
 */
static struct node *node_peek()
{
    return *((struct node **)(vector_back(current_process->node_vec)));
}

/**
 * Pops the last node we pushed to the vector
 */
static struct node *node_pop()
{
    struct node *last_node = vector_back_ptr(current_process->node_vec);
    struct node *last_node_root = vector_empty(current_process->node_tree_vec) ? NULL : vector_back_ptr(current_process->node_tree_vec);

    vector_pop(current_process->node_vec);

    if (last_node == last_node_root)
    {
        // We also have pushed this node to the tree root so we need to pop from here too
        vector_pop(current_process->node_tree_vec);
    }

    return last_node;
}

/**
 * Peeks at the node on the node_tree_vec, root of the tree basically.
 * returns the next node the peek pointer is at then moves to the next node
 */
static struct node **node_next()
{
    return vector_peek(current_process->node_tree_vec);
}

static void node_push(struct node *node)
{
    vector_push(current_process->node_vec, &node);
}

static void node_swap(struct node **f_node, struct node **s_node)
{
    struct node *tmp_node = *f_node;
    *f_node = *s_node;
    *s_node = tmp_node;
}

static struct node *node_create(struct node *_node)
{
    struct node *node = malloc(sizeof(struct node));
    memcpy(node, _node, sizeof(struct node));
    node->binded.owner = parser_current_body;
    node->binded.function = parser_current_function;
    node_push(node);
    return node;
}

static bool is_keyword_variable_modifier(const char *val)
{
    return S_EQ(val, "unsigned") ||
           S_EQ(val, "signed") ||
           S_EQ(val, "static") ||
           S_EQ(val, "const");
}

void parse_single_token_to_node()
{
    struct token *token = token_next();
    struct node *node = NULL;
    switch (token->type)
    {
    case TOKEN_TYPE_NUMBER:
        node = node_create(&(struct node){NODE_TYPE_NUMBER, .llnum = token->llnum});
        break;

    case TOKEN_TYPE_IDENTIFIER:
        node = node_create(&(struct node){NODE_TYPE_IDENTIFIER, .sval = token->sval});
        break;

    case TOKEN_TYPE_STRING:
        node = node_create(&(struct node){NODE_TYPE_STRING, .sval = token->sval});
        break;

    default:
        parse_err("Problem converting token to node. No valid node exists for token of type %i\n", token->type);
    }
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

void make_exp_node(struct node *node_left, struct node *node_right, const char *op)
{
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

void make_struct_node(const char *struct_name, struct node *body_node)
{
    node_create(&(struct node){NODE_TYPE_STRUCT, ._struct.name = struct_name, ._struct.body_n = body_node});
}

void make_unary_node(const char *unary_op, struct node *operand_node)
{
    node_create(&(struct node){NODE_TYPE_UNARY, .unary.op = unary_op, .unary.operand = operand_node});
}

void make_return_node(struct node *exp_node)
{
    node_create(&(struct node){NODE_TYPE_STATEMENT_RETURN, .stmt.ret.exp = exp_node});
}

void make_function_node(struct datatype *ret_type, const char *name, struct vector *arguments, struct node *body)
{
    node_create(&(struct node){NODE_TYPE_FUNCTION, .func.rtype = *ret_type, .func.name = name, .func.argument_vector = arguments, .func.body_n = body});
}

void make_body_node(struct vector *body_vec, size_t size, bool padded, struct node *largest_var_node)
{
    node_create(&(struct node){NODE_TYPE_BODY, .body.statements = body_vec, .body.size = size, .body.padded = padded, .body.largest_var_node = largest_var_node});
}

void make_variable_node(struct datatype *datatype, struct token *name_token, struct node *value_node)
{
    node_create(&(struct node){NODE_TYPE_VARIABLE, .var.type = *datatype, .var.name = name_token->sval, .var.val = value_node});
}

void make_bracket_node(struct node *inner_node)
{
    node_create(&(struct node){NODE_TYPE_BRACKET, .bracket.inner = inner_node});
}

void parse_expressionable(struct history *history);
void parse_for_parentheses(struct history* history);

#warning "Bug, variable_struct_node points to uninitialized value"
static void parser_append_size_for_node(size_t *_variable_size, struct node *node)
{
    if (node->type == NODE_TYPE_VARIABLE)
    {
        // Ok we have a variable lets adjust the variable_size.
        // *variable_size += node->var.type.size;
        // Test new with all possibilities.
        *_variable_size += variable_size(node);

        // If this variable is a structure and we have padding then it must be 4-byte aligned
        // as we are a 32 bit C compiler..
        if (node->var.type.type == DATA_TYPE_STRUCT)
        {
            // Great we need to align to its largest datatype boundary ((Way to large, make a function for that mess))
            *_variable_size = align_value(*_variable_size, variable_struct_node(node)->_struct.body_n->body.largest_var_node->var.type.size);
        }
    }
}

/**
 * Parses a single body statement and in the event the statement is a variable
 * the variable_size variable will be incremented by the size of the variable
 * in this statement
 */
void parse_body_single_statement(size_t *variable_size, struct vector *body_vec, struct history *history)
{

    // We will create a blank body node here as we need it as a reference
    make_body_node(NULL, 0, NULL, NULL);
    struct node *body_node = node_pop();
    body_node->binded.owner = parser_current_body;
    parser_current_body = body_node;

    struct node *stmt_node = NULL;
    parse_statement(history_down(history, history->flags));
    stmt_node = node_pop();
    vector_push(body_vec, &stmt_node);

    // Change the variable_size if this statement is a variable.
    // Incrementing it by the size of our variable
    parser_append_size_for_node(variable_size, stmt_node);

    struct node *largest_var_node = NULL;
    if (stmt_node->type == NODE_TYPE_VARIABLE)
    {
        largest_var_node = stmt_node;
    }

    // Let's make the body node for this one statement.
    make_body_node(body_vec, *variable_size, 0, largest_var_node);
    body_node->body.largest_var_node = largest_var_node;
    body_node->body.size = *variable_size;
    body_node->body.statements = body_vec;

    // Set the parser body node back to the previous one now that we are done.
    parser_current_body = body_node->binded.owner;

    // Push the body node back to the stack
    node_push(body_node);
}

/**
 * Parses the body_vec vector and for any variables the variable size is calculated
 * and added to the variable_size variable
 */
void parse_body_multiple_statements(size_t *variable_size, struct vector *body_vec, struct history *history)
{

    // We will create a blank body node here as we need it as a reference
    make_body_node(NULL, 0, NULL, NULL);
    struct node *body_node = node_pop();
    body_node->binded.owner = parser_current_body;
    parser_current_body = body_node;

    struct node *stmt_node = NULL;
    struct node *largest_primative_var_node = NULL;
    // Ok we are parsing a full body with many statements.
    expect_sym('{');
    while (!token_next_is_symbol('}'))
    {
        parse_statement(history_down(history, history->flags));
        stmt_node = node_pop();

        if (stmt_node->type == NODE_TYPE_VARIABLE && variable_node_is_primative(stmt_node))
        {
            if (!largest_primative_var_node ||
                (largest_primative_var_node->var.type.size <= stmt_node->var.type.size))
            {
                largest_primative_var_node = stmt_node;
            }
        }
        vector_push(body_vec, &stmt_node);

        // Change the variable_size if this statement is a variable.
        // Incrementing it by the size of our variable
        parser_append_size_for_node(variable_size, stmt_node);
    }

    // bodies must end with a right curley bracket!
    expect_sym('}');

    // Variable size should be adjusted to + the padding of all the body variables padding
    int padding = compute_sum_padding(body_vec);
    *variable_size += padding;

    // Our own variable size must pad to the largest member
    if (largest_primative_var_node)
    {
        *variable_size = align_value(*variable_size, largest_primative_var_node->var.type.size);
    }
    // Let's make the body node now we have parsed all statements.
    bool padded = padding != 0;
    body_node->body.largest_var_node = largest_primative_var_node;
    body_node->body.padded = padded;
    body_node->body.size = *variable_size;
    body_node->body.statements = body_vec;

    // Let's not forget to set the old body back now that we are done with this body
    parser_current_body = body_node->binded.owner;

    // Push the body node back to the stack
    node_push(body_node);
}

/**
 * Parses the body and stores the collective variable size in the "variable_size" variable
 * This can be useful for parsing a structures body and having the size returned of the
 * combined variables.
 * 
 * Note the size will not be 16-bit aligned as required in the C standard.
 */
void parse_body(size_t *variable_size, struct history *history)
{
    parser_scope_new();
    resolver_default_new_scope(current_process->resolver, 0);

    // We must always have a variable size pointer
    // if the caller doesn't care about the size we will make our own.
    size_t tmp_size = 0x00;
    if (!variable_size)
    {
        variable_size = &tmp_size;
    }

    struct vector *body_vec = vector_create(sizeof(struct node *));
    // We don't have a left curly? Then this body composes of only one statement
    if (!token_next_is_symbol('{'))
    {
        parse_body_single_statement(variable_size, body_vec, history);
        return;
    }

    // We got a couple of statements between curly braces {int a; int b;}
    parse_body_multiple_statements(variable_size, body_vec, history);

    resolver_default_finish_scope(current_process->resolver);
    parser_scope_finish();

    // If this scope is inside a function lets add the variable size to the stack size
    // of the given function
    if (variable_size)
    {
        if (history->flags & HISTORY_FLAG_INSIDE_FUNCTION_BODY)
        {
            parser_current_function->func.stack_size += *variable_size;
        }
    }
}

/**
 * Shifts the children of the node to the left.
 * 
 * I.e 50*E(20+120) will become E(50*20)+120
 */
void parser_node_shift_children_left(struct node *node)
{
    assert(node->type == NODE_TYPE_EXPRESSION);
    assert(node->exp.right->type == NODE_TYPE_EXPRESSION);

    const char *right_op = node->exp.right->exp.op;
    struct node *new_exp_left_node = node->exp.left;
    struct node *new_exp_right_node = node->exp.right->exp.left;
    // Make the new left operand
    make_exp_node(new_exp_left_node, new_exp_right_node, node->exp.op);

    struct node *new_left_operand = node_pop();
    struct node *new_right_operand = node->exp.right->exp.right;
    node->exp.left = new_left_operand;
    node->exp.right = new_right_operand;
    node->exp.op = right_op;
}

/**
 * Swaps the left node with the right node.
 */
void parser_node_flip_children(struct node *node)
{
    struct node *tmp = node->exp.left;
    node->exp.left = node->exp.right;
    node->exp.right = tmp;
}

/**
 * Reorders the given expression and its children, based on operator priority. I.e 
 * multiplication takes priority over addition.
 */
void parser_reorder_expression(struct node **node_out)
{
    struct node *node = *node_out;
    // The node passed to us has to be an expression
    if (node->type != NODE_TYPE_EXPRESSION)
    {
        return;
    }

    // No expressions nothing to do
    if (node->exp.left->type != NODE_TYPE_EXPRESSION && node->exp.right &&
        node->exp.right->type != NODE_TYPE_EXPRESSION)
    {
        return;
    }

    // If we have a right expression but left is not an expression
    // then some reordering may be needed
    if (node->exp.left->type != NODE_TYPE_EXPRESSION && node->exp.right &&
        node->exp.right->type == NODE_TYPE_EXPRESSION)
    {
        const char *right_op = node->exp.right->exp.op;
        // We have something like 50+E(20+90)
        // We must find the priority operator
        if (parser_left_op_has_priority(node->exp.op, right_op))
        {
            // We have something like 50*E(20+120)
            // We must produce the result E(50*20)+120
            parser_node_shift_children_left(node);

            // Reorder the shifted children.
            parser_reorder_expression(&node->exp.left);
            parser_reorder_expression(&node->exp.right);
        }
    }
}

int parse_expressionable_single(struct history *history);
/**
 * Used for pointer access unary i.e ***abc = 50;
 */
void parse_for_indirection_unary()
{
    // We have an indirection operator.
    // Let's calculate the pointer depth
    int depth = parser_get_pointer_depth();

    // Now lets parse the expression after this unary operator
    struct history history;
    parse_expressionable_single(history_begin(&history, 0));

    struct node *unary_operand_node = node_pop();
    make_unary_node("*", unary_operand_node);

    struct node *unary_node = node_pop();
    unary_node->unary.indirection.depth = depth;
    node_push(unary_node);
}

void parse_for_normal_unary()
{
    const char *unary_op = token_next()->sval;
    // Now lets parse the expression after this unary operator
    struct history history;
    parse_expressionable_single(history_begin(&history, 0));

    struct node *unary_operand_node = node_pop();
    make_unary_node(unary_op, unary_operand_node);
}

void parser_deal_with_additional_expression()
{
    // We got an operator? If so theirs an expression after this
    if (is_operator_token(token_peek_next()))
    {
        // Alright lets deal with it.
        struct history history;

        // parse_expressionable will deal with the operator
        // as the unary has now been pushed to the stack
        // it shall be popped off and merged into a new expression.
        parse_expressionable(history_begin(&history, 0));
    }
}

void parse_for_unary()
{
    // Let's get the unary operator
    const char *unary_op = token_peek_next()->sval;

    if (op_is_indirection(unary_op))
    {
        parse_for_indirection_unary();
        return;
    }

    // Read the normal unary
    parse_for_normal_unary();

    // We should deal with any additional expression if there is one.
    parser_deal_with_additional_expression();
}

void parse_struct(struct datatype *dtype)
{
    parser_scope_new();
    resolver_default_new_scope(current_process->resolver, 0);
    // We already have the structure name parsed, its inside dtype.
    // Parse the body of the structure "struct abc {body_here}"

    struct history history;
    size_t body_variable_size = 0;
    parse_body(&body_variable_size, history_begin(&history, HISTORY_FLAG_INSIDE_STRUCTURE));
    struct node *body_node = node_pop();
    make_struct_node(dtype->type_str, body_node);
    struct node *struct_node = node_pop();

    dtype->size = body_node->body.size;
    dtype->struct_node = struct_node;

    // Push the structure node back to the stack
    node_push(struct_node);

    // Structures must end with semicolons
    expect_sym(';');
    resolver_default_finish_scope(current_process->resolver);
    parser_scope_finish();
}

void parse_struct_or_union(struct datatype *dtype)
{
    if (dtype->type == DATA_TYPE_STRUCT)
    {
        parse_struct(dtype);
        return;
    }

    parse_err("Unions are not yet supported");
}

void parse_variable_function_or_struct_union(struct history *history);
void parse_keyword_return(struct history *history);
void parse_datatype_type(struct datatype *datatype);

void parse_identifier(struct history *history)
{
    parse_single_token_to_node();
}

void parse_sizeof(struct history *history)
{
    // Get rid of the sizeof
    expect_keyword("sizeof");
    expect_op("(");
    // Now for our expression
    struct datatype dtype;
    parse_datatype_type(&dtype);

    // Alright we got the size perfect, lets inject a number to represent the size
    struct node *node = node_create(&(struct node){NODE_TYPE_NUMBER, .llnum = datatype_size(&dtype)});
    node_push(node);

    expect_sym(')');
}

void parse_keyword_parentheses_expression(const char *keyword)
{
    struct history history;
    expect_keyword(keyword);
    expect_op("(");
    parse_expressionable(history_begin(&history, 0));
    expect_sym(')');
}

void parse_if(struct history *history);
struct node *parse_else_or_else_if(struct history *history)
{
    struct node *node = NULL;
    if (token_next_is_keyword("else"))
    {
        // Ok we have an else keyword is this an else if?
        token_next();
        if (token_next_is_keyword("if"))
        {
            // Ok we have an "else if" lets parse the if statement
            parse_if(history_down(history, 0));
            node = node_pop();
            return node;
        }

        // This is just an else statement
        size_t var_size = 0;
        parse_body(&var_size, history);
        struct node *body_node = node_pop();
        make_else_node(body_node);
        node = node_pop();
    }

    return node;
}

void parse_while(struct history *history)
{
    parse_keyword_parentheses_expression("while");
    struct node *cond_node = node_pop();
    size_t var_size = 0;
    parse_body(&var_size, history);
    struct node *body_node = node_pop();
    make_while_node(cond_node, body_node);
}

bool parse_for_loop_part(struct history *history)
{
    if (token_next_is_symbol(';'))
    {
        // We have nothing here i.e "for (; "
        // Ignore it
        token_next();
        return false;
    }

    parse_expressionable(history);
    // We must ignore the semicolon after each expression
    expect_sym(';');
    return true;
}

bool parse_for_loop_part_loop(struct history *history)
{
    if (token_next_is_symbol(')'))
    {
        return false;
    }

    parse_expressionable(history);
    return true;
}

void parse_for(struct history *history)
{
    struct node *init_node = NULL;
    struct node *cond_node = NULL;
    struct node *loop_node = NULL;
    struct node *body_node = NULL;
    expect_keyword("for");
    expect_op("(");
    if (parse_for_loop_part(history))
    {
        init_node = node_pop();
    }

    if (parse_for_loop_part(history))
    {
        cond_node = node_pop();
    }

    if (parse_for_loop_part_loop(history))
    {
        loop_node = node_pop();
    }
    expect_sym(')');

    // For loops will have a body
    size_t variable_size = 0;
    parse_body(&variable_size, history);

    body_node = node_pop();
    make_for_node(init_node, cond_node, loop_node, body_node);
}

void parse_case(struct history *history)
{
    expect_keyword("case");
    parse_expressionable(history);
    struct node *case_exp_node = node_pop();
    expect_sym(':');
    make_case_node(case_exp_node);

    if (case_exp_node->type != NODE_TYPE_NUMBER)
    {
        parse_err("We do not support non numeric cases at this time, constant numerical values without expressions are required for cases");
    }

    struct node *case_node = node_peek();
    parser_register_case(history, case_node);
}

void parse_default(struct history *history)
{
    expect_keyword("default");
    expect_sym(':');
    make_default_node();

    history->_switch.case_data->has_default_case = true;
}

void parse_switch(struct history *history)
{
    struct parser_history_switch switch_history = parser_new_switch_statement(history);

    parse_keyword_parentheses_expression("switch");
    struct node *switch_exp_node = node_pop();
    size_t variable_size = 0;
    parse_body(&variable_size, history_down(history, history->flags));
    struct node *body_node = node_pop();
    make_switch_node(switch_exp_node, body_node, switch_history.case_data->cases, switch_history.case_data->has_default_case);
    parser_end_switch_statement(&switch_history);
}

void parse_label(struct history *history)
{
    // Parse the colon
    expect_sym(':');

    // Pop off the previous node
    struct node *label_name_node = node_pop();
    // Let's ensure that its an identifier
    if (label_name_node->type != NODE_TYPE_IDENTIFIER)
    {
        parse_err("Labels must have a left operand of type identifier, but something else was provided. Alphanumeric names only");
    }

    make_label_node(label_name_node);
}

void parse_goto(struct history *history)
{
    expect_keyword("goto");
    parse_identifier(history_begin(history, 0));
    // Goto expects a semicolon
    expect_sym(';');

    struct node *label_node = node_pop();
    make_goto_node(label_node);
}

void parse_break(struct history *history)
{
    expect_keyword("break");
    expect_sym(';');

    make_break_node();
}

void parse_continue(struct history *history)
{
    expect_keyword("continue");
    expect_sym(';');

    make_continue_node();
}

void parse_do_while(struct history *history)
{
    expect_keyword("do");
    size_t var_size = 0;
    parse_body(&var_size, history);
    struct node *body_node = node_pop();
    parse_keyword_parentheses_expression("while");
    struct node *cond_node = node_pop();
    // We always have a semicolon after a do while ;)
    expect_sym(';');

    make_do_while_node(body_node, cond_node);
}

void parse_if(struct history *history)
{
    expect_keyword("if");
    expect_op("(");
    parse_expressionable(history);
    expect_sym(')');

    struct node *cond_node = node_pop();
    size_t var_size = 0;
    parse_body(&var_size, history);
    struct node *body_node = node_pop();
    struct node *next_node = parse_else_or_else_if(history);

    make_if_node(cond_node, body_node, next_node);
}

void parse_string(struct history *history)
{
    parse_single_token_to_node();
}

void parse_keyword(struct history *history)
{
    struct token *token = token_peek_next();

    if (S_EQ(token->sval, "sizeof"))
    {
        // This should be in the preprocessor but its not advacned enough
        // I hope we can get away with it here in the parser, time will tell
        parse_sizeof(history);
        return;
    }

    // keyword_is_datatype is temporary because custom types can exist
    // Therefore variable declarations will be the appropaite action
    // if all other keywords are not present.
    // This will be changed soon
    if (is_keyword_variable_modifier(token->sval) || keyword_is_datatype(token->sval))
    {
        parse_variable_function_or_struct_union(history);
        return;
    }

    if (S_EQ(token->sval, "return"))
    {
        parse_keyword_return(history);
        return;
    }
    else if (S_EQ(token->sval, "if"))
    {
        parse_if(history);
        return;
    }
    else if (S_EQ(token->sval, "while"))
    {
        parse_while(history);
        return;
    }
    else if (S_EQ(token->sval, "for"))
    {
        parse_for(history);
        return;
    }
    else if (S_EQ(token->sval, "do"))
    {
        parse_do_while(history);
        return;
    }
    else if (S_EQ(token->sval, "break"))
    {
        parse_break(history);
        return;
    }
    else if (S_EQ(token->sval, "continue"))
    {
        parse_continue(history);
        return;
    }
    else if (S_EQ(token->sval, "switch"))
    {
        parse_switch(history);
        return;
    }
    else if (S_EQ(token->sval, "case"))
    {
        parse_case(history);
        return;
    }
    else if (S_EQ(token->sval, "default"))
    {
        parse_default(history);
        return;
    }
    else if (S_EQ(token->sval, "goto"))
    {
        parse_goto(history);
        return;
    }

    parse_err("Unexpected keyword %s\n", token->sval);
}

void parse_exp_normal(struct history *history)
{
    struct token *op_token = token_peek_next();
    const char *op = op_token->sval;
    // We must pop the last node as this will be the left operand
    struct node *node_left = node_peek_or_null();
    if (!node_left)
    {
        // If we have a NULL Left node then this expression has no left operand
        // I.e it looks like this "-a" or "*b". These are unary operators of course
        // Let's deal with it
        if (!parser_is_unary_operator(op))
        {
            compiler_error(current_process, "The given expression has no left operand");
        }

        parse_for_unary();
        return;
    }

    // Pop operator token
    token_next();
    // Pop left node
    node_pop();
    // Left node is now apart of an expression
    node_left->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    int additional_flags = 0;

    // We have another operator? Then this one must be a unary or possibly parentheses
    if (token_peek_next()->type == TOKEN_TYPE_OPERATOR)
    {
        if (S_EQ(token_peek_next()->sval, "("))
        {
            parse_for_parentheses(history_down(history, history->flags | NODE_FLAG_INSIDE_EXPRESSION));
        }
        else if (parser_is_unary_operator(token_peek_next()->sval))
        {
            // Parse the unary
            parse_for_unary();
        }
        else
        {
            compiler_error(current_process, "Two operators are not expected for a given expression for operator %s\n", token_peek_next()->sval);
        }
    }
    else
    {
        // We must parse the right operand
        parse_expressionable(history_down(history, history->flags | additional_flags));
    }

    struct node *node_right = node_pop();
    // Right node is now apart of an expression
    node_right->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    make_exp_node(node_left, node_right, op);
    struct node *exp_node = node_pop();

    // We must reorder the expression if possible
    parser_reorder_expression(&exp_node);

    node_push(exp_node);
}

void parse_for_array(struct history *history)
{
    struct node *left_node = node_peek_or_null();
    if (left_node)
    {
        node_pop();
    }

    expect_op("[");
    parse_expressionable(history);
    expect_sym(']');

    struct node *exp_node = node_pop();
    make_bracket_node(exp_node);

    // Do we have a left node from earlier before we parsed the array
    if (left_node)
    {
        // Ok we do so we must create an expression node, whose left node is the left node
        // and whose right node is the array bracket node
        struct node *bracket_node = node_pop();
        make_exp_node(left_node, bracket_node, "[]");
    }
}

void parse_exp(struct history *history)
{
    if (S_EQ(token_peek_next()->sval, "("))
    {
        parse_for_parentheses(history);
        return;
    }

    if (S_EQ(token_peek_next()->sval, "["))
    {
        parse_for_array(history);
        return;
    }

    parse_exp_normal(history);
}

void parse_symbol()
{
    struct history history;

    struct token *token = token_peek_next();
    if (token->cval == ':')
    {
        // We have a label here, lets deal with it
        parse_label(history_begin(&history, 0));
    }
}
int parse_expressionable_single(struct history *history)
{
    struct token *token = token_peek_next();
    if (!token)
        return -1;

    history->flags |= NODE_FLAG_INSIDE_EXPRESSION;
    int res = -1;
    switch (token->type)
    {
    case TOKEN_TYPE_NUMBER:
        parse_single_token_to_node();
        res = 0;
        break;
    case TOKEN_TYPE_IDENTIFIER:
        parse_identifier(history);
        res = 0;
        break;
    case TOKEN_TYPE_OPERATOR:
        parse_exp(history);
        res = 0;
        break;

    case TOKEN_TYPE_KEYWORD:
        parse_keyword(history);
        res = 0;
        break;

    case TOKEN_TYPE_STRING:
        parse_string(history);
        res = 0;
        break;
    }

    return res;
}
void parse_expressionable(struct history *history)
{
    while (parse_expressionable_single(history) == 0)
    {
    }
}

void parse_for_parentheses(struct history* history)
{
    struct node* left_node = NULL;

    // If we are part of an expression then how can we have a left node
    // We would have something like 50 + (20*40). 20*40 being us
    if (!(history->flags & NODE_FLAG_INSIDE_EXPRESSION))
    {
        // We must check to see if we have a left node i.e "test(50+20)". Left node = test
        // If we have a left node we will have to create an expression
        // otherwise we can just create a parentheses node
        left_node = node_peek_or_null();
        if (left_node)
        {
            node_pop();
        }
    }
    // We want a new history for parentheses
    expect_op("(");
    parse_expressionable(history_begin(history, 0));
    expect_sym(')');

    struct node *exp_node = node_pop();
    make_exp_parentheses_node(exp_node);
    
    // Do we have a left node from earlier before we parsed the parentheses?
    if (left_node)
    {
        assert(left_node->type != NODE_TYPE_FUNCTION);

        // Ok we do so we must create an expression node, whose left node is the left node
        // and whose right node is the parentheses node
        struct node *parentheses_node = node_pop();
        make_exp_node(left_node, parentheses_node, "()");
    }

    // We got anything else?
    parser_deal_with_additional_expression();
}

/**
 * Parses the modifiers of a datatype and sets them in the structure pointer provided
 */
void parse_datatype_modifiers(struct datatype *datatype)
{
    memset(datatype, 0, sizeof(struct datatype));
    struct token *token = token_peek_next();
    // Datatypes can have many modifiers.
    while (token)
    {
        if (!is_keyword_variable_modifier(token->sval))
        {
            break;
        }

        if (S_EQ(token->sval, "signed"))
        {
            datatype->flags |= DATATYPE_FLAG_IS_SIGNED;
        }
        else if (S_EQ(token->sval, "static"))
        {
            datatype->flags |= DATATYPE_FLAG_IS_STATIC;
        }
        else if (S_EQ(token->sval, "const"))
        {
            datatype->flags |= DATATYPE_FLAG_IS_CONST;
        }
        // We dealt with this modifier token, move along.
        token_next();
        token = token_peek_next();
    }
}

static int size_of_struct(const char *struct_name)
{
    // We must pull the structure symbol for the given structure name
    // then we will know what the size is.

    struct symbol *sym = symresolver_get_symbol(current_process, struct_name);
    if (!sym)
    {
        return 0;
    }

    assert(sym->type == SYMBOL_TYPE_NODE);

    struct node *node = sym->data;
    assert(node->type == NODE_TYPE_STRUCT);

    return node->_struct.body_n->body.size;
}

void parser_datatype_init(struct token *datatype_token, struct datatype *datatype_out, int pointer_depth)
{
    // consider changing to an array that we can just map index too ;)
    // too many ifs...
    if (S_EQ(datatype_token->sval, "char"))
    {
        datatype_out->type = DATA_TYPE_CHAR;
        datatype_out->size = 1;
    }
    else if (S_EQ(datatype_token->sval, "short"))
    {
        datatype_out->type = DATA_TYPE_SHORT;
        datatype_out->size = 2;
    }
    else if (S_EQ(datatype_token->sval, "int"))
    {
        datatype_out->type = DATA_TYPE_INTEGER;
        datatype_out->size = 4;
    }
    else if (S_EQ(datatype_token->sval, "long"))
    {
        // We are a 32 bit compiler so long is 4 bytes.
        datatype_out->type = DATA_TYPE_LONG;
        datatype_out->size = 4;
    }
    else if (S_EQ(datatype_token->sval, "float"))
    {
        datatype_out->type = DATA_TYPE_FLOAT;
        datatype_out->size = 4;
    }
    else if (S_EQ(datatype_token->sval, "double"))
    {
        datatype_out->type = DATA_TYPE_DOUBLE;
        datatype_out->size = 4;
    }
    else
    {
        // I hate else, change this later. We will assume its a struct
        // in future we need to check for this as we have unions.
        datatype_out->type = DATA_TYPE_STRUCT;
        datatype_out->size = size_of_struct(datatype_token->sval);
        datatype_out->struct_node = struct_node_for_name(current_process, datatype_token->sval);
    }

    if (pointer_depth > 0)
    {
        datatype_out->flags |= DATATYPE_FLAG_IS_POINTER;
        datatype_out->pointer_depth = pointer_depth;
    }

    datatype_out->type_str = datatype_token->sval;
}

/**
 * Parses the type part of the datatype. I.e "int", "long"
 * 
 * "long long", "int long" ect ect will need to be implemented but for now
 * we support only one datatype for a series of tokens.
 */
void parse_datatype_type(struct datatype *datatype)
{
    struct token *datatype_token = token_next();
    if (S_EQ(datatype_token->sval, "struct"))
    {
        // Since we parased a "struct" keyword the actual data type will be the next token.
        datatype_token = token_next();
    }

    // Get the pointer depth i.e "int*** abc;" would have a pointer depth of 3.
    // If this is a normal variable i.e "int abc" then pointer_depth will equal zero
    int pointer_depth = parser_get_pointer_depth();

    parser_datatype_init(datatype_token, datatype, pointer_depth);
}

struct array_brackets *parse_array_brackets(struct history *history)
{
    struct array_brackets *brackets = array_brackets_new();
    while (token_next_is_operator("["))
    {
        expect_op("[");
        parse_expressionable(history);
        expect_sym(']');

        struct node *exp_node = node_pop();
        make_bracket_node(exp_node);

        struct node *bracket_node = node_pop();
        array_brackets_add(brackets, bracket_node);
    }

    return brackets;
}

void parse_variable(struct datatype *dtype, struct token *name_token, struct history *history)
{
    struct node *value_node = NULL;
    struct array_brackets *brackets = NULL;
    if (token_next_is_operator("["))
    {
        // We have an array declaration by the looks of it.
        brackets = parse_array_brackets(history);
        dtype->array.brackets = brackets;
        dtype->array.size = array_brackets_calculate_size(dtype, brackets);
        dtype->flags |= DATATYPE_FLAG_IS_ARRAY;
    }

    // We have a datatype and a variable name but we still need to parse a value if their is one
    // Lets do that now
    if (token_next_is_operator("="))
    {
        // Yeah we got an assignment with the variable declaration
        // so it looks something like this "int a = 50;".
        // Now we now we are at the "=" lets pop it off the token stack
        token_next();
        // Parse the value expression i.e the "50"
        parse_expressionable(history);
        value_node = node_pop();
    }

    make_variable_node(dtype, name_token, value_node);

    struct node *var_node = node_pop();
    // Calculate scope offset
    parser_scope_offset(var_node, history);
    parser_scope_push(parser_new_scope_entity(var_node, var_node->var.aoffset, 0), var_node->var.type.size);
    resolver_default_new_scope_entity(current_process->resolver, var_node, var_node->var.aoffset, 0);

    // Push the variable node back to the stack
    node_push(var_node);
}

/**
 * Unlike the "parse_variable" function this function does not expect you 
 * to know the datatype or the name of the variable, it parses that for you
 */
void parse_variable_full(struct history *history)
{
    // Null by default, making this an unsigned non-static variable
    struct datatype dtype;
    parse_datatype_modifiers(&dtype);
    parse_datatype_type(&dtype);

    struct token *name_token = token_next();
    parse_variable(&dtype, name_token, history);
}

/**
 * Parses the function arguments and returns a vector of function arguments
 * that were parsed succesfully
 */
struct vector *parse_function_arguments(struct history *history)
{
    parser_scope_new();
    struct vector *arguments_vec = vector_create(sizeof(struct node *));
    // If we see a right bracket we are at the end of the function arguments i.e (int a, int b)
    while (!token_next_is_symbol(')'))
    {
        // Do we have a "..." if so then we have infinite arguments.
        if (token_next_is_operator("."))
        {
            // Read the 3 dots.
            token_read_dots(3);
            // Okay since we have infinite arguments we can't have any more arguments
            // after this, so just return
            return arguments_vec;
        }

        // Function arguments grow upwards on the stack
        parse_variable_full(history_down(history, history->flags | HISTORY_FLAG_IS_UPWARD_STACK));
        // Push the parsed argument variable into the arguments vector
        struct node *argument_node = node_pop();
        vector_push(arguments_vec, &argument_node);

        // Loop until no more function arguments are present
        if (!token_next_is_operator(","))
        {
            break;
        }

        // Skip the comma
        token_next();
    }
    parser_scope_finish();

    return arguments_vec;
}

void parse_function_body(struct history *history)
{
    parse_body(0, history_down(history, history->flags | HISTORY_FLAG_INSIDE_FUNCTION_BODY));
}

void parse_function(struct datatype *dtype, struct token *name_token, struct history *history)
{
    struct vector *arguments_vector = NULL;

    struct history new_history;

    // Scope for function arguments
    resolver_default_new_scope(current_process->resolver, 0);

    // We expect a left bracket for functions.
    // Let us not forget we already have the return type and name of the function i.e int abc
    expect_op("(");
    arguments_vector = parse_function_arguments(history_begin(&new_history, 0));
    expect_sym(')');

    // Create the function node
    make_function_node(dtype, name_token->sval, arguments_vector, NULL);
    struct node *function_node = node_peek();
    parser_current_function = function_node;

    // Do we have a function body or is this a declaration?
    if (token_next_is_symbol('{'))
    {
        // Parse the function body
        parse_function_body(history_begin(&new_history, 0));
        struct node *body_node = node_pop();
        function_node->func.body_n = body_node;
    }
    else
    {
        // Ok then this is a function declaration wtihout a body, in which case
        // we expect a semicolon
        expect_sym(';');
    }

    parser_current_function = NULL;
    // We are done with function arguments scope
    resolver_finish_scope(current_process->resolver);
}

static bool is_datatype_struct_or_union(struct datatype *dtype)
{
    return dtype->type == DATA_TYPE_STRUCT || dtype->type == DATA_TYPE_UNION;
}
/**
 * Parses a variable or function, at this point the parser should be certain
 * that the tokens coming up will form a variable or a function
 */
void parse_variable_function_or_struct_union(struct history *history)
{
    //  Variables have datatypes, and so do functions have return types.
    //  lets parse the data type

    // Null by default, making this an unsigned non-static variable
    struct datatype dtype;
    parse_datatype_modifiers(&dtype);
    parse_datatype_type(&dtype);

    // If we have a body then we have defined a structure or union.
    if (is_datatype_struct_or_union(&dtype) && token_next_is_symbol('{'))
    {
        // Ok we have defined a structure or union such as
        // struct abc {}
        // Therefore this is not a variable
        // We should parse the structure
        parse_struct_or_union(&dtype);
        return;
    }

    // Ok great we have a datatype at this point, next comes the variable name
    // or the function name.. we don't know which one yet ;)

    struct token *name_token = token_next();

    // If we have a left bracket then this must be a function i.e int abc()
    // Let's handle the function
    if (token_next_is_operator("("))
    {
        parse_function(&dtype, name_token, history);
        return;
    }

    // Since this is not a function it has to be a variable
    // Let's handle the variable
    parse_variable(&dtype, name_token, history);
    // We expect variable declarations to end with ";"
    expect_sym(';');
}

void parse_keyword_return(struct history *history)
{
    expect_keyword("return");

    // Ok we parsed the return keyword, lets now parse the expression of the return
    // keyword and then we expect a semicolon ;)
    parse_expressionable(history);

    struct node *ret_expr = node_pop();
    make_return_node(ret_expr);

    // We expect a semicolon all the time when it comes to return keywords
    expect_sym(';');
}

void parse_keyword_for_global()
{
    struct history history;
    parse_keyword(history_begin(&history, HISTORY_FLAG_IS_GLOBAL_SCOPE));

    // Global variables and functions must be registered as symbols.
    struct node *node = node_pop();
    switch (node->type)
    {
    case NODE_TYPE_VARIABLE:
    case NODE_TYPE_FUNCTION:
    case NODE_TYPE_STRUCT:
        symresolver_build_for_node(current_process, node);
        break;
    }

    node_push(node);
}

/**
 * Statements are essentially assignments, variable declarations, for loops
 * if statements and so on. THeir is no node type of statment, there are however
 * nodes that fit in this statement category and should be parsed as such.
 */
void parse_statement(struct history *history)
{
    // Statements are composed of keywords or expressions
    if (token_peek_next()->type == TOKEN_TYPE_KEYWORD)
    {
        parse_keyword(history);
        return;
    }
    // This must be an expression as its not a keyword
    // I.e a = 50;
    parse_expressionable(history);

    // Do we have a symbol here that is not a semicolon then it may be treated differently if we do
    if (token_peek_next()->type == TOKEN_TYPE_SYMBOL && !token_is_symbol(token_peek_next(), ';'))
    {
        parse_symbol();
        return;
    }

    // Expression statements must end with a semicolon
    expect_sym(';');
}

int parse_next()
{
    struct history history;

    struct token *token = token_peek_next();
    if (!token)
    {
        return -1;
    }
    int res = 0;
    switch (token->type)
    {

    case TOKEN_TYPE_NUMBER:
    case TOKEN_TYPE_IDENTIFIER:
    case TOKEN_TYPE_OPERATOR:
        parse_expressionable(history_begin(&history, 0));
        break;

    case TOKEN_TYPE_SYMBOL:
        parse_symbol();
        break;

    case TOKEN_TYPE_KEYWORD:
        parse_keyword_for_global();
        break;

    default:
        parse_err("Unexpected token\n");
    }

    return res;
}

bool parser_should_allow_for_child_search(int type, int node_type, bool ignore_childtypes_for_type)
{
    return type != node_type || !ignore_childtypes_for_type;
}

void parser_get_all_nodes_of_type_single(struct vector *vector, struct node **node_out, int type, bool ignore_childtypes_for_type);

void parser_get_all_nodes_of_type_for_vector(struct vector *vector_out, struct vector *vector_in, int type, bool ignore_childtypes_for_type)
{
    vector_set_peek_pointer(vector_in, 0);
    struct node **node_out = vector_peek(vector_in);
    while (node_out)
    {
        parser_get_all_nodes_of_type_single(vector_out, node_out, type, ignore_childtypes_for_type);
        node_out = vector_peek(vector_in);
    }
}

void parser_get_all_nodes_of_type_for_function(struct vector *vector, struct node *node, int type, bool ignore_childtypes_for_type)
{
    struct vector *func_args = node->func.argument_vector;
    parser_get_all_nodes_of_type_for_vector(vector, node->func.argument_vector, type, ignore_childtypes_for_type);
    parser_get_all_nodes_of_type_for_vector(vector, node->func.body_n->body.statements, type, ignore_childtypes_for_type);
}

void parser_get_all_nodes_of_type_single(struct vector *vector, struct node **node_out, int type, bool ignore_childtypes_for_type)
{
    struct node *node = *node_out;
    if (!node)
    {
        // Nothing to do for the given node its NULL.
        return;
    }

    if (node->type == type)
    {
        vector_push(vector, &node_out);
        if (!parser_should_allow_for_child_search(type, node->type, ignore_childtypes_for_type))
        {
            // We are not allowed to search further, ignore this
            return;
        }
    }

    switch (node->type)
    {
    case NODE_TYPE_EXPRESSION:
        parser_get_all_nodes_of_type_single(vector, &node->exp.left, type, ignore_childtypes_for_type);
        parser_get_all_nodes_of_type_single(vector, &node->exp.right, type, ignore_childtypes_for_type);
        break;

    case NODE_TYPE_VARIABLE:
        parser_get_all_nodes_of_type_single(vector, &node->var.val, type, ignore_childtypes_for_type);
        break;

    case NODE_TYPE_FUNCTION:
        parser_get_all_nodes_of_type_for_function(vector, node, type, ignore_childtypes_for_type);
        break;

    // Numbers and identifiers have no children
    case NODE_TYPE_NUMBER:
    case NODE_TYPE_IDENTIFIER:

        break;

    default:
        assert(0 == 1 && "Compiler bug");
    }
}

/**
 * Returns a pointer of all the nodes parsed in the parser that are of a given type.
 * iterates through all the child nodes not just the root of the tree. Points to the address
 * of their location on the stack
 * 
 * If you ask for nodes of variable types every variable node will be returned in the
 * entire parse process
 * 
 * \param ignore_childtypes_for_type If this is true then when a given node type is found it will not look for children in that node.
 */
struct vector *parser_get_all_nodes_of_type(struct compile_process *process, int type, bool ignore_childtypes_for_type)
{
    struct vector *vector = vector_create(sizeof(struct node **));
    vector_set_peek_pointer(process->node_tree_vec, 0);
    struct node **node_out = NULL;
    while ((node_out = node_next()) != NULL)
    {
        parser_get_all_nodes_of_type_single(vector, node_out, type, ignore_childtypes_for_type);
    }

    return vector;
}

int parse(struct compile_process *process)
{
    // Create the root scope for parsing.
    // This scope will help us generate static offsets to be used during compile time.
    scope_create_root(process);

    current_process = process;
    vector_set_peek_pointer(process->token_vec, 0);
    struct node *node = NULL;
    while (parse_next() == 0)
    {
        // Pop the node that was created on the stack so we can add it to the root of the tree
        // This element we are popping came from parse_next
        struct node *node = node_pop();
        // Push the root element to the tree
        vector_push(process->node_tree_vec, &node);
        // Also push it back to the main node stack that we just popped from
        node_push(node);
    }

    scope_free_root(process);

    return PARSE_ALL_OK;
}