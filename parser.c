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
extern struct node *parser_current_body;

// The current function we are in.
extern struct node *parser_current_function;

// NODE_TYPE_BLANK - Represents a node that does nothing, used so that we don't
// have to check for a NULL. when working with the tree.
struct node *parser_blank_node;

// The last token parsed by the parser, may be NULL
extern struct token *parser_last_token;

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
    HISTORY_FLAG_INSIDE_FUNCTION_BODY = 0b01000000,
    HISTORY_FLAG_PARENTHESES_IS_NOT_A_FUNCTION_CALL = 0b10000000,
    HISTORY_FLAG_INSIDE_UNION = 0b100000000
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
 *
 * Found in expressionable.c
 */
extern struct op_precedence_group op_precedence[TOTAL_OPERATOR_GROUPS];

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
static struct fixup_system *parser_fixup_sys;
int parse_next();
void parse_statement(struct history *history);
void parse_expressionable_root(struct history *history);
void parse_expressionable_for_op(struct history *history, const char *op);
void parse_variable_function_or_struct_union(struct history *history);
void parse_keyword_return(struct history *history);
void parse_datatype_type(struct datatype *datatype);
void parse_datatype(struct datatype *datatype);

static struct history *history_down(struct history *history, int flags)
{
    struct history *new_history = calloc(sizeof(struct history), 1);
    memcpy(new_history, history, sizeof(struct history));
    new_history->flags = flags;
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

#define parser_scope_last_entity_stop_global_scope() \
    scope_last_entity_stop_at(current_process, current_process->scope.root)

#define parser_scope_current() \
    scope_current(current_process)

int size_of_struct(const char *struct_name);
void parse_variable(struct datatype *dtype, struct token *name_token, struct history *history);

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

int parser_get_random_type_index()
{
    static int x = 0;
    x++;
    return x;
}

struct token *parser_build_random_type_name()
{
    char tmp_name[25];
    sprintf(tmp_name, "customtypeamenNI_%i", parser_get_random_type_index());
    char *sval = malloc(sizeof(tmp_name));
    strncpy(sval, tmp_name, sizeof(tmp_name));
    struct token *token = calloc(sizeof(struct token), 1);
    token->type = TOKEN_TYPE_IDENTIFIER;
    token->sval = sval;
    return token;
}

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
    struct parser_scope_entity *last_entity = parser_scope_last_entity_stop_global_scope();
    bool upward_stack = history->flags & HISTORY_FLAG_IS_UPWARD_STACK;
    int offset = -variable_size(node);
    if (upward_stack)
    {
        // Do not use the variable size for the offset on an upward stack.
        // The reason is because PUSH instructions consume 4 bytes of the stack,
        // therefore we cannot trust anything that does not modulo into four.
        // We will have to access everything as an integer and cast it down into
        // the correct data type.
        // Are we also the first entity? If so then the offset must start at EIGHT Bytes
        size_t stack_addition = function_node_argument_stack_addition(parser_current_function);
        offset = stack_addition;
        if (last_entity)
        {
            offset = datatype_size(&variable_node(last_entity->node)->var.type);
        }
    }

    if (last_entity)
    {
        offset += variable_node(last_entity->node)->var.aoffset;
        if (variable_node_is_primative(node))
        {
            variable_node(node)->var.padding = padding(upward_stack ? offset : -offset, node->var.type.size);
            variable_node(last_entity->node)->var.padding_after = node->var.padding;
        }
    }

    bool first_entity = !last_entity;

    // If this is a structure variable then we must align the padding to a 4-byte boundary so long
    // as their was any padding in the original structure scope
    // \attention Maybe make a new function for second operand, a bit long...
    if (node_is_struct_or_union_variable(node) && variable_struct_or_union_body_node(node)->body.padded)
    {
        variable_node(node)->var.padding = padding(upward_stack ? offset : -offset, DATA_SIZE_DWORD);
    }
    variable_node(node)->var.aoffset = offset + (upward_stack ? variable_node(node)->var.padding : -variable_node(node)->var.padding);
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

static bool token_is_nl_or_comment_or_newline_seperator(struct token *token)
{
    return token->type == TOKEN_TYPE_NEWLINE ||
           token->type == TOKEN_TYPE_COMMENT ||
           token_is_symbol(token, '\\');
}

static void parser_ignore_nl_or_comment(struct token *next_token)
{
    while (next_token && token_is_nl_or_comment_or_newline_seperator(next_token))
    {
        // Skip the token
        vector_peek(current_process->token_vec);
        // The parser does not care about new lines, only the preprocessor has to care about that
        next_token = vector_peek_no_increment(current_process->token_vec);
    }
}
static struct token *token_next()
{
    struct token *next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    current_process->pos = next_token->pos;
    parser_last_token = next_token;
    return vector_peek(current_process->token_vec);
}

static struct token *token_peek_next()
{
    struct token *next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
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

static bool is_keyword_variable_modifier(const char *val)
{
    return S_EQ(val, "unsigned") ||
           S_EQ(val, "signed") ||
           S_EQ(val, "static") ||
           S_EQ(val, "const") ||
           S_EQ(val, "extern") ||
           S_EQ(val, "__ignore_typecheck__");
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

struct datatype_struct_node_fix_private
{
    // The variable node whose data type must be fixed as the structure is now present.
    struct node *node;
};

bool datatype_struct_node_fix(struct fixup *fixup)
{
    struct datatype_struct_node_fix_private *private = fixup_private(fixup);
    struct datatype *dtype = &private->node->var.type;
    dtype->type = DATA_TYPE_STRUCT;
    dtype->size = size_of_struct(dtype->type_str);
    dtype->struct_node = struct_node_for_name(current_process, dtype->type_str);

    // Still couldnt resolve it? Then they haven't declared it anywhere and lied to us!
    // There is no structure with the name dtype->type_str.
    if (!dtype->struct_node)
    {
        return false;
    }

    return true;
}

void datatype_struct_node_fix_end(struct fixup *fixup)
{
    // Let's free the fixup private
    free(fixup_private(fixup));
}

void make_variable_node(struct datatype *datatype, struct token *name_token, struct node *value_node)
{
    const char *name_str = NULL;
    if (name_token)
    {
        name_str = name_token->sval;
    }
    node_create(&(struct node){NODE_TYPE_VARIABLE, .var.type = *datatype, .var.name = name_str, .var.val = value_node});
    struct node *var_node = node_peek_or_null();
    assert(var_node);
    // Is our struct node NULL? Then a fixup is required a forward declaration was present
    // Could argue that this is not the most sensible place to put it
    // but this is a gauranteed way that a fixup will be registered.
    if (var_node->var.type.type == DATA_TYPE_STRUCT && !var_node->var.type.struct_node)
    {
        struct datatype_struct_node_fix_private *private = calloc(sizeof(struct datatype_struct_node_fix_private), 1);
        private
            ->node = var_node;
        fixup_register(parser_fixup_sys, &(struct fixup_config){.fix = datatype_struct_node_fix, .end = datatype_struct_node_fix_end, .private = private});
    }
}

void make_variable_node_and_register(struct history *history, struct datatype *datatype, struct token *name_token, struct node *value_node)
{
    make_variable_node(datatype, name_token, value_node);
    struct node *var_node = node_pop();
    // Calculate scope offset
    parser_scope_offset(var_node, history);
    parser_scope_push(parser_new_scope_entity(var_node, var_node->var.aoffset, 0), var_node->var.type.size);
    resolver_default_new_scope_entity(current_process->resolver, var_node, var_node->var.aoffset, 0);
    // Push the variable node back to the stack
    node_push(var_node);
}

void make_variable_list_node(struct vector *var_list)
{
    node_create(&(struct node){NODE_TYPE_VARIABLE_LIST, .var_list.list = var_list});
}
void make_bracket_node(struct node *inner_node)
{
    node_create(&(struct node){NODE_TYPE_BRACKET, .bracket.inner = inner_node});
}

void parse_expressionable(struct history *history);
void parse_for_parentheses(struct history *history);

static void parser_append_size_for_node_struct_union(struct history *history, size_t *_variable_size, struct node *node)
{
    *_variable_size += variable_size(node);
    if (node->var.type.flags & DATATYPE_FLAG_IS_POINTER)
    {
        // A pointer struct? Well we don't want to do anything else than this
        return;
    }

    struct node *largest_var_node = variable_struct_or_union_body_node(node)->body.largest_var_node;
    if (largest_var_node)
    {
        // Great we need to align to its largest datatype boundary ((Way to large, make a function for that mess))
        *_variable_size = align_value(*_variable_size, largest_var_node->var.type.size);
    }
}
void parser_append_size_for_node(struct history *history, size_t *_variable_size, struct node *node);
void parser_append_size_for_variable_list(struct history *history, size_t *_variable_size, struct vector *var_list)
{
    vector_set_peek_pointer(var_list, 0);
    struct node *node = vector_peek_ptr(var_list);
    while (node)
    {
        parser_append_size_for_node(history, _variable_size, node);
        node = vector_peek_ptr(var_list);
    }
}

void parser_append_size_for_node(struct history *history, size_t *_variable_size, struct node *node)
{
    if (!node)
        return;

    if (node->type == NODE_TYPE_VARIABLE)
    {
        // Is this a structure variable?
        if (node_is_struct_or_union_variable(node))
        {
            parser_append_size_for_node_struct_union(history, _variable_size, node);
            return;
        }

        // Normal variable, okay lets append the size.
        // Ok we have a variable lets adjust the variable_size.
        // *variable_size += node->var.type.size;
        // Test new with all possibilities.
        *_variable_size += variable_size(node);
    }
    else if (node->type == NODE_TYPE_VARIABLE_LIST)
    {
        parser_append_size_for_variable_list(history, _variable_size, node->var_list.list);
    }
}

void parser_finalize_body(struct history *history, struct node *body_node, struct vector *body_vec, size_t *_variable_size, struct node *largest_align_eligible_var_node, struct node *largest_possible_var_node)
{
    if (history->flags & HISTORY_FLAG_INSIDE_UNION)
    {
        // Unions variable size is equal to the largest variable node size
        if (largest_possible_var_node)
        {
            *_variable_size = variable_size(largest_possible_var_node);
        }
    }

    // Variable size should be adjusted to + the padding of all the body variables padding
    int padding = compute_sum_padding(body_vec);
    *_variable_size += padding;

    // Our own variable size must pad to the largest member
    if (largest_align_eligible_var_node)
    {
        *_variable_size = align_value(*_variable_size, largest_align_eligible_var_node->var.type.size);
    }
    // Let's make the body node now we have parsed all statements.
    bool padded = padding != 0;
    body_node->body.largest_var_node = largest_align_eligible_var_node;
    body_node->body.padded = padded;
    body_node->body.size = *_variable_size;
    body_node->body.statements = body_vec;
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
    parser_append_size_for_node(history, variable_size, variable_node_or_list(stmt_node));

    struct node *largest_var_node = NULL;
    if (stmt_node->type == NODE_TYPE_VARIABLE)
    {
        largest_var_node = stmt_node;
    }

    parser_finalize_body(history, body_node, body_vec, variable_size, largest_var_node, largest_var_node);

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
    struct node *largest_align_eligible_var_node = NULL;
    struct node *largest_possible_var_node = NULL;
    // Ok we are parsing a full body with many statements.
    expect_sym('{');
    while (!token_next_is_symbol('}'))
    {
        parse_statement(history_down(history, history->flags));
        stmt_node = node_pop();

        if (stmt_node->type == NODE_TYPE_VARIABLE)
        {
            if (!largest_possible_var_node ||
                (largest_possible_var_node->var.type.size <= stmt_node->var.type.size))
            {
                largest_possible_var_node = stmt_node;
            }
        }
        if (stmt_node->type == NODE_TYPE_VARIABLE && variable_node_is_primative(stmt_node))
        {
            if (!largest_align_eligible_var_node ||
                (largest_align_eligible_var_node->var.type.size <= stmt_node->var.type.size))
            {
                largest_align_eligible_var_node = stmt_node;
            }
        }
        vector_push(body_vec, &stmt_node);

        // Change the variable_size if this statement is a variable.
        // Incrementing it by the size of our variable
        parser_append_size_for_node(history, variable_size, variable_node_or_list(stmt_node));
    }

    // bodies must end with a right curley bracket!
    expect_sym('}');

    // We must finalize the body
    parser_finalize_body(history, body_node, body_vec, variable_size, largest_align_eligible_var_node, largest_possible_var_node);

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

void parser_node_move_right_left_to_left(struct node *node)
{
    make_exp_node(node->exp.left, node->exp.right->exp.left, node->exp.op);
    struct node *completed_node = node_pop();

    // Now we still have the right node to worry about
    const char *new_op = node->exp.right->exp.op;
    node->exp.left = completed_node;
    node->exp.right = node->exp.right->exp.right;
    node->exp.op = new_op;
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

    // Should optimize the priority array rather than statics
    // Todo...
    if ((is_array_node(node->exp.left) && is_node_assignment(node->exp.right)) || ((node_is_expression(node->exp.left, "()") || node_is_expression(node->exp.left, "[]")) &&
                                                                                   node_is_expression(node->exp.right, ",")))
    {
        // We have a comma here and an expression to the left, therefore left operand
        // of right node must be moved
        parser_node_move_right_left_to_left(node);
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
    parse_expressionable(history_begin(&history, EXPRESSION_IS_UNARY));

    struct node *unary_operand_node = node_pop();
    make_unary_node("*", unary_operand_node, 0);

    struct node *unary_node = node_pop();
    unary_node->unary.indirection.depth = depth;
    node_push(unary_node);
}

void parse_for_cast()
{
    // ( is parsed by the caller.
    struct datatype dtype;
    parse_datatype(&dtype);
    expect_sym(')');

    struct history history;
    parse_expressionable(&history);

    struct node *operand_node = node_pop();
    make_cast_node(&dtype, operand_node);
}

void parse_for_normal_unary()
{
    const char *unary_op = token_next()->sval;
    // Now lets parse the expression after this unary operator
    struct history history;
    parse_expressionable(history_begin(&history, EXPRESSION_IS_UNARY));
    struct node *unary_operand_node = node_pop();
    make_unary_node(unary_op, unary_operand_node, 0);
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

void parse_struct_no_new_scope(struct datatype *dtype, bool is_forward_declaration)
{
    // We already have the structure name parsed, its inside dtype.
    // Parse the body of the structure "struct abc {body_here}"
    struct node *body_node = NULL;
    size_t body_variable_size = 0;

    struct history history;
    if (!is_forward_declaration)
    {
        parse_body(&body_variable_size, history_begin(&history, HISTORY_FLAG_INSIDE_STRUCTURE));
        body_node = node_pop();
    }

    make_struct_node(dtype->type_str, body_node);
    struct node *struct_node = node_pop();
    if (body_node)
    {
        dtype->size = body_node->body.size;
    }
    dtype->struct_node = struct_node;

    // Do we have an identifier? Then we are also creating a variable
    // for this structure.
    if (token_peek_next()->type == TOKEN_TYPE_IDENTIFIER)
    {
        // alright parse the name of this structure variable
        struct token *var_name = token_next();
        struct_node->flags |= NODE_FLAG_HAS_VARIABLE_COMBINED;

        if (dtype->flags & DATATYPE_FLAG_STRUCT_UNION_NO_NAME)
        {
            // No type name has been set yet, however as we have an identifier here
            // this is the type
            dtype->type_str = var_name->sval;
            dtype->flags &= ~DATATYPE_FLAG_STRUCT_UNION_NO_NAME;
            // Don't forget to update the structure name with the new one.
            struct_node->_struct.name = var_name->sval;
        }

        // We must create a variable for this structure
        make_variable_node_and_register(history_begin(&history, 0), dtype, var_name, NULL);
        struct_node->_struct.var = node_pop();
    }

    // Structures must end with semicolons
    expect_sym(';');

    // Push the structure node back to the stack
    node_push(struct_node);
}

void parse_union_no_scope(struct datatype *dtype, bool is_forward_declaration)
{
    // We already have the structure name parsed, its inside dtype.
    // Parse the body of the structure "struct abc {body_here}"
    struct node *body_node = NULL;
    size_t body_variable_size = 0;

    struct history history;
    if (!is_forward_declaration)
    {
        parse_body(&body_variable_size, history_begin(&history, HISTORY_FLAG_INSIDE_UNION));
        body_node = node_pop();
    }

    make_union_node(dtype->type_str, body_node);
    struct node *union_node = node_pop();
    if (body_node)
    {
        dtype->size = body_node->body.size;
    }
    dtype->union_node = union_node;

    // Do we have an identifier? Then we are also creating a variable
    // for this union.
    if (token_peek_next()->type == TOKEN_TYPE_IDENTIFIER)
    {
        // alright parse the name of this union variable
        struct token *var_name = token_next();
        union_node->flags |= NODE_FLAG_HAS_VARIABLE_COMBINED;

        // We must create a variable for this union
        make_variable_node_and_register(history_begin(&history, 0), dtype, var_name, NULL);
        union_node->_union.var = node_pop();
    }

    // Unions must end with semicolons
    expect_sym(';');

    // Push the union node back to the stack
    node_push(union_node);
}

void parse_union(struct datatype *dtype)
{
    bool is_forward_declaration = !token_is_symbol(token_peek_next(), '{');

    if (!is_forward_declaration)
    {
        parser_scope_new();
        resolver_default_new_scope(current_process->resolver, 0);
    }
    parse_union_no_scope(dtype, is_forward_declaration);
    if (!is_forward_declaration)
    {
        resolver_default_finish_scope(current_process->resolver);
        parser_scope_finish();
    }
}
void parse_struct(struct datatype *dtype)
{
    bool is_forward_declaration = !token_is_symbol(token_peek_next(), '{');

    if (!is_forward_declaration)
    {
        parser_scope_new();
        resolver_default_new_scope(current_process->resolver, 0);
    }
    parse_struct_no_new_scope(dtype, is_forward_declaration);
    if (!is_forward_declaration)
    {
        resolver_default_finish_scope(current_process->resolver);
        parser_scope_finish();
    }
}

void parse_struct_or_union(struct datatype *dtype)
{
    switch (dtype->type)
    {
    case DATA_TYPE_STRUCT:
        parse_struct(dtype);
        break;

    case DATA_TYPE_UNION:
        parse_union(dtype);
        break;

    default:
        parse_err("What is that your providing.. struct or union only! BUG!");
    };
}

void parse_identifier(struct history *history)
{
    assert(token_peek_next()->type == TOKEN_TYPE_IDENTIFIER);
    // Its just a single token.
    parse_single_token_to_node();
}

void parse_sizeof(struct history *history)
{
    // Get rid of the sizeof
    expect_keyword("sizeof");
    expect_op("(");
    
    // Now for our expression
    struct datatype dtype;
    parse_datatype(&dtype);

    // Alright we got the size perfect, lets inject a number to represent the size
    node_create(&(struct node){NODE_TYPE_NUMBER, .llnum = datatype_size(&dtype)});

    expect_sym(')');
}

void parse_keyword_parentheses_expression(const char *keyword)
{
    struct history history;
    expect_keyword(keyword);
    expect_op("(");
    parse_expressionable_root(history_begin(&history, 0));
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

    parse_expressionable_root(history);
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

    parse_expressionable_root(history);
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
    parse_expressionable_root(history);
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
    parse_expressionable_root(history);
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

void parse_for_right_operanded_unary(struct node *left_operand_node, const char *unary_op)
{
    make_unary_node(unary_op, left_operand_node, UNARY_FLAG_IS_RIGHT_OPERANDED_UNARY);
}

void parse_exp_normal(struct history *history)
{
    struct token *op_token = token_peek_next();
    const char *op = op_token->sval;

    // We must pop the last node as this will be the left operand
    struct node *node_left = node_peek_expressionable_or_null();
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

    if (is_right_operanded_unary_operator(op))
    {
        parse_for_right_operanded_unary(node_left, op);
        return;
    }
    // Left node is now apart of an expression
    node_left->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    int additional_flags = 0;

    // We have another operator? Then this one must be a unary or possibly parentheses
    if (token_peek_next()->type == TOKEN_TYPE_OPERATOR)
    {
        if (S_EQ(token_peek_next()->sval, "("))
        {
            parse_for_parentheses(history_down(history, history->flags | HISTORY_FLAG_PARENTHESES_IS_NOT_A_FUNCTION_CALL));
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
        parse_expressionable_for_op(history_down(history, history->flags | additional_flags), op);
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
    parse_expressionable_root(history);
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

void parse_for_tenary(struct history *history)
{
    // At this point we have parsed the condition of the tenary
    // i.e 50 ? 20 : 10  we are now at the ? 20 bit.

    struct node *condition_operand = node_pop();
    expect_op("?");

    // Let's parse the TRUE result of this tenary
    parse_expressionable_root(history_down(history, HISTORY_FLAG_PARENTHESES_IS_NOT_A_FUNCTION_CALL));
    struct node *true_result_node = node_pop();
    // Now comes the colon
    expect_sym(':');

    // Finally the false result
    parse_expressionable_root(history_down(history, HISTORY_FLAG_PARENTHESES_IS_NOT_A_FUNCTION_CALL));
    struct node *false_result_node = node_pop();

    // Now to craft the tenary
    make_tenary_node(true_result_node, false_result_node);

    // We may need to make this into an expression node later on..
    // Not sure how this is going to turn out.. lets try and make an expression
    struct node *tenary_node = node_pop();
    make_exp_node(condition_operand, tenary_node, "?");
}

void parse_for_comma(struct history *history)
{
    // Skip comma
    token_next();
    struct node *node_left = node_pop();

    // Parse next node as root.
    parse_expressionable_root(history);

    struct node *node_right = node_pop();
    make_exp_node(node_left, node_right, ",");
}

int parse_exp(struct history *history)
{
    // Unary expression may only nest with particular operators.
    if (history->flags & EXPRESSION_IS_UNARY &&
        !unary_operand_compatiable(token_peek_next()))
    {
        return -1;
    }

    if (S_EQ(token_peek_next()->sval, ","))
    {
        parse_for_comma(history);
    }
    else if (S_EQ(token_peek_next()->sval, "("))
    {
        parse_for_parentheses(history);
    }
    else if (S_EQ(token_peek_next()->sval, "["))
    {
        parse_for_array(history);
    }
    else if (S_EQ(token_peek_next()->sval, "?"))
    {
        parse_for_tenary(history);
    }
    else
    {
        parse_exp_normal(history);
    }

    return 0;
}

void parse_symbol()
{
    struct history history;

    struct token *token = token_peek_next();
    if (token->cval == ':')
    {
        // We have a label here, lets deal with it
        parse_label(history_begin(&history, 0));
        return;
    }

    parse_err("Not expecting the symbol %c", token->cval);
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
        res = parse_exp(history);
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

struct node *parser_evaluate_exp_to_numerical_node(struct node *node)
{
    assert(node->type == NODE_TYPE_EXPRESSION);
    struct node *left_node = node->exp.left;
    struct node *right_node = node->exp.right;
    const char *op = node->exp.op;

    assert(node_is_constant(current_process->resolver, left_node) && node_is_constant(current_process->resolver, right_node));

    long left_val = node_pull_literal(current_process->resolver, left_node);
    long right_val = node_pull_literal(current_process->resolver, right_node);

    bool success = false;
    long result = arithmetic(current_process, left_val, right_val, op, &success);
    if (!success)
    {
        return node;
    }
    // We must create a number node for this expression

    node_create(&(struct node){.type = NODE_TYPE_NUMBER, .llnum = result});
    return node_pop();
}

struct node *parser_evaluate_identifier_to_numerical_node(struct node *node)
{
    assert(node->type == NODE_TYPE_IDENTIFIER);

    struct resolver_result *result = resolver_follow(current_process->resolver, node);
    if (!resolver_result_ok(result))
    {
        return node;
    }

    struct resolver_entity *entity = NULL;
    entity = resolver_result_entity(result);
    struct variable *var = NULL;

    if (entity && entity->node)
    {
        struct node* var_node = variable_node(entity->node);
        if (var_node)
        {
            var = &var_node->var;
        }
    }
    // Only if the constant is not a pointer will we pull a literal and push a number node
    // this is to prevent const char* ptr being interpreted as a literal number.
    if (var && var->type.flags & DATATYPE_FLAG_IS_CONST && !(var->type.flags & DATATYPE_FLAG_IS_POINTER))
    {
        // Okay its constant
        long literal_val = var->val->llnum;
        if (node_is_constant(current_process->resolver, node) && !(node->flags & DATATYPE_FLAG_IS_POINTER))
        {
            long val = node_pull_literal(current_process->resolver, node);
            node_create(&(struct node){.type = NODE_TYPE_NUMBER, .llnum = literal_val});
            return node_pop();
        }
    }

    return node;
}
struct node *parser_const_to_literal(struct node *node);

struct node *parser_exp_const_to_literal(struct node *node)
{
    assert(node->type == NODE_TYPE_EXPRESSION);

    node->exp.left = parser_const_to_literal(node->exp.left);
    node->exp.right = parser_const_to_literal(node->exp.right);

    if (node_is_constant(current_process->resolver, node->exp.left) && node_is_constant(current_process->resolver, node->exp.right))
    {
        // We must compute the constant expression into a single literal node
        return parser_evaluate_exp_to_numerical_node(node);
    }
    return node;
}

struct node *parser_exp_parenthesis_const_to_literal(struct node *node)
{
    return parser_const_to_literal(node->parenthesis.exp);
}

/**
 * Convert all constant expressions to a single numerical node
 *
 * \return Returns the resulting node.
 */
struct node *parser_const_to_literal(struct node *node)
{
    struct node *result_node = node;
    switch (node->type)
    {
    case NODE_TYPE_EXPRESSION:
        result_node = parser_exp_const_to_literal(node);
        break;

    case NODE_TYPE_EXPRESSION_PARENTHESIS:
        result_node = parser_exp_parenthesis_const_to_literal(node);
        break;

    case NODE_TYPE_IDENTIFIER:
        result_node = parser_evaluate_identifier_to_numerical_node(node);
        break;
    }
    return result_node;
}

void parse_expressionable_root(struct history *history)
{
    parse_expressionable(history);
    //struct node *result_node = parser_const_to_literal(node_pop());
   // node_push(result_node);
}

void parse_expressionable(struct history *history)
{
    while (parse_expressionable_single(history) == 0)
    {
    }
}

void parse_expressionable_for_op(struct history *history, const char *op)
{
    struct op_precedence_group *op_group = NULL;
    parser_get_precedence_for_operator(op, &op_group);
    if (op_group->associativity == ASSOCIATIVITY_RIGHT_TO_LEFT)
    {
        // Right to left associativity? Then this is a root expression
        parse_expressionable_root(history);
        return;
    }

    // Normal expression.
    parse_expressionable(history);
}

void parse_for_parentheses(struct history *history)
{
    expect_op("(");
    if (token_peek_next()->type == TOKEN_TYPE_KEYWORD)
    {
        // We have a cast here... I believe.
        parse_for_cast();
        return;
    }

    struct node *left_node = NULL;
    struct node *tmp_node = node_peek_or_null();

    // We must check to see if we have a left node i.e "test(50+20)". Left node = test
    // If we have a left node we will have to create an expression
    // otherwise we can just create a parentheses node
    if (tmp_node && node_is_value_type(tmp_node))
    {
        left_node = tmp_node;
        node_pop();
    }

    struct node *exp_node = parser_blank_node;
    if (!token_next_is_symbol(')'))
    {
        // We want a new history for parentheses
        parse_expressionable_root(history_begin(history, 0));
        exp_node = node_pop();
    }
    expect_sym(')');

    make_exp_parentheses_node(exp_node);
    // Do we have a left node from earlier before we parsed the parentheses?
    if (left_node)
    {
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

    struct token *token = token_peek_next();
    // Datatypes can have many modifiers.
    while (token && token->type == TOKEN_TYPE_KEYWORD)
    {
        if (!is_keyword_variable_modifier(token->sval))
        {
            break;
        }

        if (S_EQ(token->sval, "signed"))
        {
            datatype->flags |= DATATYPE_FLAG_IS_SIGNED;
        }
        else if (S_EQ(token->sval, "unsigned"))
        {
            datatype->flags &= ~DATATYPE_FLAG_IS_SIGNED;
        }
        else if (S_EQ(token->sval, "static"))
        {
            datatype->flags |= DATATYPE_FLAG_IS_STATIC;
        }
        else if (S_EQ(token->sval, "const"))
        {
            datatype->flags |= DATATYPE_FLAG_IS_CONST;
        }
        else if (S_EQ(token->sval, "extern"))
        {
            datatype->flags |= DATATYPE_FLAG_IS_EXTERN;
        }
        else if (S_EQ(token->sval, "__ignore_typecheck__"))
        {
            datatype->flags |= DATATYPE_FLAG_IGNORE_TYPE_CHECKING;
        }

        // We dealt with this modifier token, move along.
        token_next();
        token = token_peek_next();
    }
}

int size_of_struct(const char *struct_name)
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

int size_of_union(const char *union_name)
{
    // We must pull the structure symbol for the given structure name
    // then we will know what the size is.
    struct symbol *sym = symresolver_get_symbol(current_process, union_name);
    if (!sym)
    {
        return 0;
    }

    assert(sym->type == SYMBOL_TYPE_NODE);

    struct node *node = sym->data;
    assert(node->type == NODE_TYPE_UNION);

    return node->_union.body_n->body.size;
}

bool parser_datatype_is_secondary_allowed_for_type(const char *type)
{
    return S_EQ(type, "long") || S_EQ(type, "short") || S_EQ(type, "double") || S_EQ(type, "float");
}

void parser_datatype_init_type_and_size_for_primitive(struct token *datatype_token, struct token *datatype_secondary_token, struct datatype *datatype_out);

void parser_datatype_adjust_size_for_secondary(struct datatype *datatype, struct token *datatype_secondary_token)
{
    if (!datatype_secondary_token)
    {
        return;
    }

    struct datatype *secondary_data_type = calloc(sizeof(struct datatype), 1);
    parser_datatype_init_type_and_size_for_primitive(datatype_secondary_token, NULL, secondary_data_type);
    datatype->size += secondary_data_type->size;
    datatype->secondary = secondary_data_type;
    datatype->flags |= DATATYPE_FLAG_SECONDARY;
}

void parser_datatype_init_type_and_size_for_primitive(struct token *datatype_token, struct token *datatype_secondary_token, struct datatype *datatype_out)
{
    if (!parser_datatype_is_secondary_allowed_for_type(datatype_token->sval) && datatype_secondary_token)
    {
        // no secondary is allowed
        compiler_error(current_process, "Your not allowed a secondary datatype here for the given datatype %s", datatype_token->sval);
    }

    if (S_EQ(datatype_token->sval, "void"))
    {
        datatype_out->type = DATA_TYPE_VOID;
        datatype_out->size = 0;
    }
    else if (S_EQ(datatype_token->sval, "char"))
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
        parse_err("Bug unexpected primitive variable\n");
    }

    parser_datatype_adjust_size_for_secondary(datatype_out, datatype_secondary_token);
}

bool parser_datatype_is_secondary_allowed(int expected_type)
{
    return expected_type == DATA_TYPE_EXPECT_PRIMITIVE;
}

void parser_datatype_init_type_and_size(struct token *datatype_token, struct token *datatype_secondary_token, struct datatype *datatype_out, int pointer_depth, int expected_type)
{

    if (!parser_datatype_is_secondary_allowed(expected_type) && datatype_secondary_token)
    {
        compiler_error(current_process, "You provided an extra datatype yet this is not a primitive variable");
    }

    switch (expected_type)
    {
    case DATA_TYPE_EXPECT_PRIMITIVE:
        parser_datatype_init_type_and_size_for_primitive(datatype_token, datatype_secondary_token, datatype_out);
        break;

    case DATA_TYPE_EXPECT_UNION:
        datatype_out->type = DATA_TYPE_UNION;
        datatype_out->size = size_of_union(datatype_token->sval);
        datatype_out->union_node = union_node_for_name(current_process, datatype_token->sval);
        break;

    case DATA_TYPE_EXPECT_STRUCT:
        datatype_out->type = DATA_TYPE_STRUCT;
        datatype_out->size = size_of_struct(datatype_token->sval);
        datatype_out->struct_node = struct_node_for_name(current_process, datatype_token->sval);
        break;

    default:
        parser_err("Compiler bug unexpected data type expectation");
    }

    if (pointer_depth > 0)
    {
        datatype_out->flags |= DATATYPE_FLAG_IS_POINTER;
        datatype_out->pointer_depth = pointer_depth;
    }
}

void parser_datatype_init(struct token *datatype_token, struct token *datatype_secondary_token, struct datatype *datatype_out, int pointer_depth, int expected_type)
{
    parser_datatype_init_type_and_size(datatype_token, datatype_secondary_token, datatype_out, pointer_depth, expected_type);
    datatype_out->type_str = datatype_token->sval;

    if (S_EQ(datatype_token->sval, "long") && datatype_secondary_token && S_EQ(datatype_secondary_token->sval, "long"))
    {
        compiler_warning(current_process, "Our compiler does not support 64 bit long long so it will be treated as a 32 bit type not 64 bit\n");
        datatype_out->size = DATA_SIZE_DWORD;
    }
}

int parser_datatype_expected_for_type_string(const char *s)
{
    int type = DATA_TYPE_EXPECT_PRIMITIVE;
    if (S_EQ(s, "union"))
    {
        type = DATA_TYPE_EXPECT_UNION;
    }
    else if (S_EQ(s, "struct"))
    {
        type = DATA_TYPE_EXPECT_STRUCT;
    }
    return type;
}

void parser_get_datatype_tokens(struct token **datatype_token_out, struct token **datatype_secondary_token_out)
{
    *datatype_token_out = token_next();
    // Let's check if we have a secondary datatype.
    struct token *next_token = token_peek_next();
    if (token_is_primitive_keyword(next_token))
    {
        // Okay we have a secondary datatype token
        *datatype_secondary_token_out = next_token;
        token_next();
    }
}
/**
 * Parses the type part of the datatype. I.e "int", "long"
 *
 */
void parse_datatype_type(struct datatype *datatype)
{
    struct token *datatype_token = NULL;
    struct token *datatype_secondary_token = NULL;
    parser_get_datatype_tokens(&datatype_token, &datatype_secondary_token);
    int expected_type = parser_datatype_expected_for_type_string(datatype_token->sval);
    if (datatype_is_struct_or_union_for_name(datatype_token->sval))
    {
        // Since we parased a "struct" keyword the actual data type will be the next token.
        if (token_peek_next()->type == TOKEN_TYPE_IDENTIFIER)
        {
            datatype_token = token_next();
        }
        else
        {
            // We have no name for this structure? THen we must make one
            // as the compiler needs to be able to identify this structure
            datatype_token = parser_build_random_type_name();
            datatype->flags |= DATATYPE_FLAG_STRUCT_UNION_NO_NAME;
        }
    }

    // Get the pointer depth i.e "int*** abc;" would have a pointer depth of 3.
    // If this is a normal variable i.e "int abc" then pointer_depth will equal zero
    int pointer_depth = parser_get_pointer_depth();

    parser_datatype_init(datatype_token, datatype_secondary_token, datatype, pointer_depth, expected_type);
}

void parse_datatype(struct datatype *datatype)
{
    memset(datatype, 0, sizeof(struct datatype));
    // By default all variables are signed
    datatype->flags |= DATATYPE_FLAG_IS_SIGNED;

    parse_datatype_modifiers(datatype);
    parse_datatype_type(datatype);
    // We can have modifiers to the right too.
    parse_datatype_modifiers(datatype);
}

struct array_brackets *parse_array_brackets(struct history *history)
{
    struct array_brackets *brackets = array_brackets_new();
    while (token_next_is_operator("["))
    {
        expect_op("[");
        if (token_is_symbol(token_peek_next(), ']'))
        {
            // Nothing between the brackets?
            // Okay
            expect_sym(']');
            break;
        }
        parse_expressionable_root(history);
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
        parse_expressionable_root(history);
        value_node = node_pop();
    }

    make_variable_node_and_register(history, dtype, name_token, value_node);
}

/**
 * Unlike the "parse_variable" function this function does not expect you
 * to know the datatype or the name of the variable, it parses that for you
 */
void parse_variable_full(struct history *history)
{
    // Null by default, making this an unsigned non-static variable
    struct datatype dtype;
    parse_datatype(&dtype);

    struct token *name_token = NULL;
    if (token_peek_next()->type == TOKEN_TYPE_IDENTIFIER)
    {
        name_token = token_next();
    }
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
            parser_scope_finish();
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
    parser_scope_new();

    // Create the function node
    make_function_node(dtype, name_token->sval, NULL, NULL);
    struct node *function_node = node_peek();
    parser_current_function = function_node;

    // Is the return type a structure or union? Then we need to reserve four bytes
    // for a pointer.... to be accessed when returning the structure from this function
    if (datatype_is_struct_or_union_non_pointer(dtype))
    {
        // Okay lets reserve room for the pointer we will pass. (invisible argument)
        function_node->func.args.stack_addition += DATA_SIZE_DWORD;
    }

    // We expect a left bracket for functions.
    // Let us not forget we already have the return type and name of the function i.e int abc
    expect_op("(");
    arguments_vector = parse_function_arguments(history_begin(&new_history, 0));
    expect_sym(')');

    // Set the arguments vector... Maybe change this to a function..
    function_node->func.args.vector = arguments_vector;

    if (symresolver_get_symbol_for_native_function(current_process, name_token->sval))
    {
        // This is a native function, lets apply that flag
        function_node->func.flags |= FUNCTION_NODE_FLAG_IS_NATIVE;
    }

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
    parser_scope_finish();
}

void parse_forward_declaration_struct(struct datatype *dtype)
{
    // Okay lets parse the structure
    parse_struct(dtype);
}

void parse_forward_declaration(struct datatype *dtype)
{
    if (dtype->type == DATA_TYPE_STRUCT)
    {
        // Struct forward declaration
        parse_forward_declaration_struct(dtype);
        return;
    }

    FAIL_ERR("BUG with forward declaration");
}

bool parser_is_int_valid_after_datatype(struct datatype *dtype)
{
    return dtype->type == DATA_TYPE_LONG || dtype->type == DATA_TYPE_FLOAT || dtype->type == DATA_TYPE_DOUBLE;
}

/**
 * C allows you to abbrevative a datatype.
 *
 * i.e long and long int are the same
 * Let's ignore the int if present
 */
void parser_ignore_int(struct datatype *dtype)
{
    if (!token_is_keyword(token_peek_next(), "int"))
    {
        // no "int" to ignore
        return;
    }

    if (!parser_is_int_valid_after_datatype(dtype))
    {
        compiler_error(current_process, "You provided an int abbrevation however this datatype does not support such action");
    }

    // Let's ignore it
    token_next();
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
    parse_datatype(&dtype);

    // If we have a body then we have defined a structure or union.
    if (datatype_is_struct_or_union(&dtype) && token_next_is_symbol('{'))
    {
        // Ok we have defined a structure or union such as
        // struct abc {}
        // Therefore this is not a variable
        // We should parse the structure
        parse_struct_or_union(&dtype);

        struct node *su_node = node_pop();
        // It's possible we have a sub-structure or sub-union that needs registering
        // with the symresolver
        symresolver_build_for_node(current_process, su_node);
        node_push(su_node);
        return;
    }

    // Is this a struct/union forward declaration?.
    if (token_is_symbol(token_peek_next(), ';'))
    {
        // It's a forward declaration Then we are done.
        parse_forward_declaration(&dtype);
        // Build forward declaration will handle the semicolon.
        return;
    }

    // Ignore the protential int keyword after the data type
    // i.e long int
    parser_ignore_int(&dtype);

    // Ok great we have a datatype at this point, next comes the variable name
    // or the function name.. we don't know which one yet ;)
    struct token *name_token = token_next();
    if (name_token->type != TOKEN_TYPE_IDENTIFIER)
    {
        parse_err("Expecting a name for the given variable declaration");
    }

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

    // Anymore variables?
    if (token_is_operator(token_peek_next(), ","))
    {
        // As we have more variables we want to create a variable list for this
        struct vector *var_list = vector_create(sizeof(struct node *));
        // Pop off original node that was parsed and add it to the list
        struct node *var_node = node_pop();
        vector_push(var_list, &var_node);
        while (token_is_operator(token_peek_next(), ","))
        {
            // Get rid of the comma
            token_next();

            // We have another variable, we know its type.
            // get the name
            name_token = token_next();
            parse_variable(&dtype, name_token, history);
            var_node = node_pop();
            vector_push(var_list, &var_node);
        }

        // Since we now have a variable list lets create a variable list node.
        make_variable_list_node(var_list);
    }

    // We expect variable declarations to end with ";"
    expect_sym(';');
}

void parse_keyword_return(struct history *history)
{
    expect_keyword("return");

    if (token_next_is_symbol(';'))
    {
        // We don't have an expression for this return statement. We are finished
        expect_sym(';');
        make_return_node(NULL);
        return;
    }

    // Ok we parsed the return keyword, lets now parse the expression of the return
    // keyword and then we expect a semicolon ;)
    parse_expressionable_root(history);

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
    case NODE_TYPE_UNION:
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
    parse_expressionable_root(history);

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
    struct vector *func_args = function_node_argument_vec(node);
    parser_get_all_nodes_of_type_for_vector(vector, function_node_argument_vec(node), type, ignore_childtypes_for_type);
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
    parser_blank_node = node_create(&(struct node){.type = NODE_TYPE_BLANK});
    parser_fixup_sys = fixup_sys_new();

    vector_set_peek_pointer(process->token_vec, 0);
    struct node *node = NULL;
    while (parse_next() == 0)
    {
        node = node_peek();
        // Push the root element to the tree
        vector_push(process->node_tree_vec, &node);
    }

    // Let's fix the fixups
    assert(fixups_resolve(parser_fixup_sys));
    scope_free_root(process);

    return PARSE_ALL_OK;
}