#include "compiler.h"
#include "helpers/vector.h"
#include <assert.h>

#define TOTAL_OPERATOR_GROUPS 14
#define MAX_OPERATORS_IN_GROUP 12

// Expression flags

enum
{
    ASSOCIATIVITY_LEFT_TO_RIGHT,
    ASSOCIATIVITY_RIGHT_TO_LEFT
};
struct expressionable_op_precedence_group
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
static struct expressionable_op_precedence_group op_precedence[TOTAL_OPERATOR_GROUPS] = {
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


void expressionable_parse(struct expressionable *expressionable);

struct expressionable_callbacks *expressionable_callbacks(struct expressionable *expressionable)
{
    return &expressionable->config.callbacks;
}

void expressionable_node_push(struct expressionable *expressionable, void *node_ptr)
{
    vector_push(expressionable->node_vec_out, node_ptr);
}

struct token *expressionable_token_next(struct expressionable *expressionable)
{
    struct token *next_token = vector_peek_no_increment(expressionable->token_vec);
    while (next_token && next_token->type == TOKEN_TYPE_NEWLINE)
    {
        vector_peek(expressionable->token_vec);
        // The parser does not care about new lines, only the preprocessor has to care about that
        next_token = vector_peek_no_increment(expressionable->token_vec);
    }
    return vector_peek(expressionable->token_vec);
}

static struct node *expressionable_node_peek_or_null(struct expressionable *expressionable)
{
    return vector_back_ptr_or_null(expressionable->node_vec_out);
}

struct token *expressionable_peek_next(struct expressionable *expressionable)
{
    struct token *next_token = vector_peek_no_increment(expressionable->token_vec);
    while (next_token && next_token->type == TOKEN_TYPE_NEWLINE)
    {
        vector_peek(expressionable->token_vec);
        // The parser does not care about new lines, only the preprocessor has to care about that
        next_token = vector_peek_no_increment(expressionable->token_vec);
    }
    return vector_peek_no_increment(expressionable->token_vec);
}

static void expressionable_expect_sym(struct expressionable *expressionable, char c)
{
    struct token *next_token = expressionable_token_next(expressionable);
    if (next_token == NULL || !token_is_symbol(next_token, c))
        expressionable_parse_err("Expecting the symbol %c but something else was provided", c);
}

static void expressionable_expect_op(struct expressionable *expressionable, const char *op)
{
    struct token *next_token = expressionable_token_next(expressionable);
    if (next_token == NULL || !token_is_operator(next_token, op))
        expressionable_parse_err("Expecting the operator %s but something else was provided", op);
}

/**
 * Pops the last node we pushed to the vector
 */
struct node *expressionable_node_pop(struct expressionable *expressionable)
{
    struct node *last_node = vector_back_ptr(expressionable->node_vec_out);
    vector_pop(expressionable->node_vec_out);
    return last_node;
}

void expressionable_init(struct expressionable *expressionable, struct vector *token_vector, struct vector *node_vector, struct expressionable_config *config)
{
    memset(expressionable, 0, sizeof(struct expressionable));
    memcpy(&expressionable->config, config, sizeof(struct expressionable_config));
    // We don't actually know anything about nodes, we are an abstraction
    // We have the size that is enough.
    expressionable->token_vec = token_vector;
    expressionable->node_vec_out = node_vector;
}

struct expressionable *expressionable_create(struct expressionable_config* config, struct vector *token_vector, struct vector *node_vector)
{
    assert(vector_element_size(token_vector) == sizeof(struct token));
    struct expressionable *expressionable = calloc(sizeof(struct expressionable), 1);
    expressionable_init(expressionable, token_vector, node_vector, config);
    return expressionable;
}


int expressionable_parse_number(struct expressionable *expressionable)
{
    void *node_ptr = expressionable_callbacks(expressionable)->handle_number_callback(expressionable);
    if (!node_ptr)
        return -1;

    expressionable_node_push(expressionable, node_ptr);
    return 0;
}

int expressionable_parse_identifier(struct expressionable *expressionable)
{
    void *node_ptr = expressionable_callbacks(expressionable)->handle_identifier_callback(expressionable);
    if (!node_ptr)
        return -1;

    expressionable_node_push(expressionable, node_ptr);
    return 0;
}

void expressionable_parse_parentheses(struct expressionable *expressionable)
{
    // We must check to see if we have a left node i.e "test(50+20)". Left node = test
    // If we have a left node we will have to create an expression
    // otherwise we can just create a parentheses node
    struct node *left_node = expressionable_node_peek_or_null(expressionable);
    if (left_node)
    {
        expressionable_node_pop(expressionable);
    }

    expressionable_expect_op(expressionable, "(");
    expressionable_parse(expressionable);
    expressionable_expect_sym(expressionable, ')');

    struct node *exp_node = expressionable_node_pop(expressionable);
    expressionable_callbacks(expressionable)->make_parentheses_node(expressionable, exp_node);

    // Do we have a left node from earlier before we parsed the parentheses?
    if (left_node)
    {
        // Ok we do so we must create an expression node, whose left node is the left node
        // and whose right node is the parentheses node
        struct node *parentheses_node = expressionable_node_pop(expressionable);
        expressionable_callbacks(expressionable)->make_parentheses_node(expressionable, parentheses_node);
    }
}

int expressionable_parse_exp(struct expressionable *expressionable)
{
    if (S_EQ(expressionable_peek_next(expressionable)->sval, "("))
    {
        expressionable_parse_parentheses(expressionable);
        return 0;
    }

    return 0;
}

int expressionable_parse_single(struct expressionable *expressionable)
{
    struct token *token = expressionable_peek_next(expressionable);
    if (!token)
        return -1;

    int res = -1;
    switch (token->type)
    {
    case TOKEN_TYPE_NUMBER:
        expressionable_parse_number(expressionable);
        res = 0;
        break;
    case TOKEN_TYPE_IDENTIFIER:
        expressionable_parse_identifier(expressionable);
        res = 0;
        break;
    case TOKEN_TYPE_OPERATOR:
        expressionable_parse(expressionable);
        res = 0;
        break;
    }
    return res;
}

/**
 * Shifts the children of the node to the left.
 * 
 * I.e 50*E(20+120) will become E(50*20)+120
 */
void expressionable_parser_node_shift_children_left(struct expressionable* expressionable, void* node)
{

    void* left_node = expressionable_callbacks(expressionable)->get_left_node(expressionable, node);
    void* right_node = expressionable_callbacks(expressionable)->get_right_node(expressionable, node);

    const char *right_op = expressionable_callbacks(expressionable)->get_node_operator(expressionable, right_node);
    void* new_exp_left_node = node;
    void* *new_exp_right_node = expressionable_callbacks(expressionable)->get_right_node(expressionable, left_node);
    
    const char* node_op = expressionable_callbacks(expressionable)->get_node_operator(expressionable, node);
    // Make the new left operand
    expressionable_callbacks(expressionable)->make_expression_node(expressionable, new_exp_left_node, new_exp_right_node, node_op);

    void* new_left_operand = expressionable_node_pop(expressionable);
    struct node *new_right_operand = expressionable_callbacks(expressionable)->get_right_node(expressionable, right_node);
    expressionable_callbacks(expressionable)->set_exp_node(expressionable, node, new_left_operand, new_right_operand, right_op);
}


static int expressionable_parser_get_precedence_for_operator(const char *op, struct expressionable_op_precedence_group **group_out)
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

static bool expressionable_parser_left_op_has_priority(const char *op_left, const char *op_right)
{
    struct expressionable_op_precedence_group *group_left = NULL;
    struct expressionable_op_precedence_group *group_right = NULL;

    // Same operator? Then they have equal priority!
    if (S_EQ(op_left, op_right))
        return false;

    int precedence_left = expressionable_parser_get_precedence_for_operator(op_left, &group_left);
    int precedence_right = expressionable_parser_get_precedence_for_operator(op_right, &group_right);
    if (group_left->associativity == ASSOCIATIVITY_RIGHT_TO_LEFT)
    {
        // Right to left associativity in the left group? and right group left_to_right?
        // Then right group takes priority
        return false;
    }

    return precedence_left <= precedence_right;
}


/**
 * Reorders the given expression and its children, based on operator priority. I.e 
 * multiplication takes priority over addition.
 */
void expressionable_parser_reorder_expression(struct expressionable *expressionable, void **node_out)
{
    void *node = *node_out;

    int node_type = expressionable_callbacks(expressionable)->get_node_type(expressionable, node);
    // The node passed to us has to be an expression
    if (node_type != EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION)
    {
        return;
    }

    void *left_node = expressionable_callbacks(expressionable)->get_left_node(expressionable, node);
    void *right_node = expressionable_callbacks(expressionable)->get_right_node(expressionable, node);

    int left_node_type = -1;
    int right_node_type = -1;
    if (left_node)
    {
        left_node_type = expressionable_callbacks(expressionable)->get_node_type(expressionable, left_node);
    }

    if (right_node)
    {
        right_node_type = expressionable_callbacks(expressionable)->get_node_type(expressionable, right_node);
    }

    // No expressions nothing to do
    if (left_node_type != EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION && right_node &&
        right_node_type != EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION)
    {
        return;
    }

    // If we have a right expression but left is not an expression
    // then some reordering may be needed
    if (left_node_type != EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION && right_node &&
        right_node_type == EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION)
    {
        const char *right_op = expressionable_callbacks(expressionable)->get_node_operator(expressionable, right_node);
        const char *main_op = expressionable_callbacks(expressionable)->get_node_operator(expressionable, node);
        // We have something like 50+E(20+90)
        // We must find the priority operator
        if (expressionable_parser_left_op_has_priority(main_op, right_op))
        {
            // We have something like 50*E(20+120)
            // We must produce the result E(50*20)+120
            expressionable_parser_node_shift_children_left(expressionable, node);

            // Reorder the shifted children.

            void **address_of_left = expressionable_callbacks(expressionable)->get_left_node_address(expressionable, left_node);
            void **address_of_right = expressionable_callbacks(expressionable)->get_right_node_address(expressionable, right_node);

            expressionable_parser_reorder_expression(expressionable, address_of_left);
            expressionable_parser_reorder_expression(expressionable, address_of_right);
        }
    }
}

void expressionable_parse(struct expressionable *expressionable)
{
    while (expressionable_parse_single(expressionable) == 0)
    {
    }
}
