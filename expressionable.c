#include "compiler.h"
#include "misc.h"
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
struct expressionable_op_precedence_group op_precedence[TOTAL_OPERATOR_GROUPS] = {
    {.operators = {"++", "--", "()", "[]", "(", "[", "]", ".", "->", NULL}, .associativity = ASSOCIATIVITY_LEFT_TO_RIGHT},
    {.operators = {"*", "/", "%", NULL}, .associativity = ASSOCIATIVITY_LEFT_TO_RIGHT},
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
int expressionable_parse_single(struct expressionable *expressionable);
void expressionable_parse_parentheses(struct expressionable *expressionable);

void expressionable_error(struct expressionable *expressionable, const char *str, ...)
{
    FAIL_ERR(str);
}

struct expressionable_callbacks *expressionable_callbacks(struct expressionable *expressionable)
{
    return &expressionable->config.callbacks;
}

void expressionable_node_push(struct expressionable *expressionable, void *node_ptr)
{
    vector_push(expressionable->node_vec_out, &node_ptr);
}

void expressionable_ignore_nl(struct expressionable *expressionable, struct token *next_token)
{
    while (next_token && token_is_symbol(next_token, '\\'))
    {
        // Skip the "\\"
        vector_peek(expressionable->token_vec);
        // Skip the NEWLINE token
        struct token *nl_token = vector_peek(expressionable->token_vec);
        assert(nl_token->type == TOKEN_TYPE_NEWLINE);

        // The parser does not care about new lines, only the preprocessor has to care about that
        next_token = vector_peek_no_increment(expressionable->token_vec);
    }
}

struct token *expressionable_token_next(struct expressionable *expressionable)
{
    struct token *next_token = vector_peek_no_increment(expressionable->token_vec);
    expressionable_ignore_nl(expressionable, next_token);
    return vector_peek(expressionable->token_vec);
}

static void *expressionable_node_peek_or_null(struct expressionable *expressionable)
{
    return vector_back_ptr_or_null(expressionable->node_vec_out);
}

struct token *expressionable_peek_next(struct expressionable *expressionable)
{
    struct token *next_token = vector_peek_no_increment(expressionable->token_vec);
    expressionable_ignore_nl(expressionable, next_token);
    return vector_peek_no_increment(expressionable->token_vec);
}

static bool expressionable_token_next_is_operator(struct expressionable *expressionable, const char *op)
{
    struct token *token = expressionable_peek_next(expressionable);
    return token_is_operator(token, op);
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
void *expressionable_node_pop(struct expressionable *expressionable)
{
    void *last_node = vector_back_ptr(expressionable->node_vec_out);
    vector_pop(expressionable->node_vec_out);
    return last_node;
}

void expressionable_init(struct expressionable *expressionable, struct vector *token_vector, struct vector *node_vector, struct expressionable_config *config, int flags)
{
    memset(expressionable, 0, sizeof(struct expressionable));
    memcpy(&expressionable->config, config, sizeof(struct expressionable_config));
    // We don't actually know anything about nodes, we are an abstraction
    // We have the size that is enough.
    expressionable->token_vec = token_vector;
    expressionable->node_vec_out = node_vector;
    expressionable->flags = flags;
}

struct expressionable *expressionable_create(struct expressionable_config *config, struct vector *token_vector, struct vector *node_vector, int flags)
{
    assert(vector_element_size(token_vector) == sizeof(struct token));
    struct expressionable *expressionable = calloc(sizeof(struct expressionable), 1);
    expressionable_init(expressionable, token_vector, node_vector, config, flags);
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

/**
 * Returns true if the given type can be used in an expression
 */
bool expressionable_generic_type_is_value_expressionable(int type)
{
    return type == EXPRESSIONABLE_GENERIC_TYPE_NUMBER ||
           type == EXPRESSIONABLE_GENERIC_TYPE_IDENTIFIER ||
           type == EXPRESSIONABLE_GENERIC_TYPE_UNARY ||
           type == EXPRESSIONABLE_GENERIC_TYPE_PARENTHESES ||
           type == EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION;
}

void expressionable_deal_with_additional_expression(struct expressionable *expressionable)
{
    // We got an operator? If so theirs an expression after this
    if (is_operator_token(expressionable_peek_next(expressionable)))
    {
        expressionable_parse(expressionable);
    }
}
void expressionable_parse_parentheses(struct expressionable *expressionable)
{
    void *left_node = NULL;
    // We must check to see if we have a left node i.e "test(50+20)". Left node = test
    // If we have a left node we will have to create an expression
    // otherwise we can just create a parentheses node
    void *tmp_node = expressionable_node_peek_or_null(expressionable);
    int tmp_type = tmp_node ? expressionable_callbacks(expressionable)->get_node_type(expressionable, tmp_node) : -1;
    if (tmp_node && expressionable_generic_type_is_value_expressionable(tmp_type))
    {
        left_node = tmp_node;
        expressionable_node_pop(expressionable);
    }

    expressionable_expect_op(expressionable, "(");
    expressionable_parse(expressionable);
    expressionable_expect_sym(expressionable, ')');

    void *exp_node = expressionable_node_pop(expressionable);
    expressionable_callbacks(expressionable)->make_parentheses_node(expressionable, exp_node);

    // Do we have a left node from earlier before we parsed the parentheses?
    if (left_node)
    {
        // Ok we do so we must create an expression node, whose left node is the left node
        // and whose right node is the parentheses node
        void *parentheses_node = expressionable_node_pop(expressionable);
        expressionable_callbacks(expressionable)->make_expression_node(expressionable, left_node, parentheses_node, "()");
    }

    expressionable_deal_with_additional_expression(expressionable);
}

int expressionable_get_pointer_depth(struct expressionable *expressionable)
{
    int depth = 0;
    while (expressionable_token_next_is_operator(expressionable, "*"))
    {
        depth += 1;
        expressionable_token_next(expressionable);
    }

    return depth;
}

/**
 * Used for pointer access unary i.e ***abc = 50;
 */
void expressionable_parse_for_indirection_unary(struct expressionable *expressionable)
{
    // We have an indirection operator.
    // Let's calculate the pointer depth
    int depth = expressionable_get_pointer_depth(expressionable);

    // Now lets parse the expression after this unary operator
    expressionable_parse(expressionable);

    void *unary_operand_node = expressionable_node_pop(expressionable);
    expressionable_callbacks(expressionable)->make_unary_indirection_node(expressionable, depth, unary_operand_node);
}

void expressionable_parse_for_normal_unary(struct expressionable *expressionable)
{
    const char *unary_op = expressionable_token_next(expressionable)->sval;
    expressionable_parse(expressionable);

    void *unary_operand_node = expressionable_node_pop(expressionable);
    expressionable_callbacks(expressionable)->make_unary_node(expressionable, unary_op, unary_operand_node);
}

void expressionable_parse_unary(struct expressionable *expressionable)
{
    // Let's get the unary operator
    const char *unary_op = expressionable_peek_next(expressionable)->sval;

    if (op_is_indirection(unary_op))
    {
        expressionable_parse_for_indirection_unary(expressionable);
        return;
    }

    // Read the normal unary
    expressionable_parse_for_normal_unary(expressionable);

    // We should deal with any additional expression if there is one.
    expressionable_deal_with_additional_expression(expressionable);
}

void expressionable_parse_for_operator(struct expressionable *expressionable);

void expressionable_parse_tenary(struct expressionable *expressionable)
{
    // At this point we have parsed the condition of the tenary
    // i.e 50 ? 20 : 10  we are now at the ? 20 bit.

    void *condition_operand = expressionable_node_pop(expressionable);
    expressionable_expect_op(expressionable, "?");

    // Let's parse the TRUE result of this tenary
    expressionable_parse(expressionable);
    void *true_result_node = expressionable_node_pop(expressionable);
    // Now comes the colon
    expressionable_expect_sym(expressionable, ':');

    // Finally the false result
    expressionable_parse(expressionable);
    void *false_result_node = expressionable_node_pop(expressionable);

    // Now to craft the tenary
    expressionable_callbacks(expressionable)->make_tenary_node(expressionable, true_result_node, false_result_node);

    // We may need to make this into an expression node later on..
    // Not sure how this is going to turn out.. lets try and make an expression
    void *tenary_node = expressionable_node_pop(expressionable);
    expressionable_callbacks(expressionable)->make_expression_node(expressionable, condition_operand, tenary_node, "?");
}

int expressionable_parse_exp(struct expressionable *expressionable, struct token *token)
{
    if (S_EQ(expressionable_peek_next(expressionable)->sval, "("))
    {
        expressionable_parse_parentheses(expressionable);
    }
    else if (S_EQ(expressionable_peek_next(expressionable)->sval, "?"))
    {
        expressionable_parse_tenary(expressionable);
    }
    else
    {
        // I Hate else, make sub functions avoid this .
        // Normal operator i.e a + b, 5 + 10
        expressionable_parse_for_operator(expressionable);
    }
    return 0;
}

int expressionable_parse_token(struct expressionable *expressionable, struct token *token, int flags)
{
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
        expressionable_parse_exp(expressionable, token);
        break;
    }

    return res;
}

int expressionable_parse_single_with_flags(struct expressionable *expressionable, int flags)
{
    int res = -1;
    struct token *token = expressionable_peek_next(expressionable);
    if (!token)
        return -1;

    if (expressionable_callbacks(expressionable)->is_custom_operator(expressionable, token))
    {
        token->flags |= TOKEN_FLAG_IS_CUSTOM_OPERATOR;
        expressionable_parse_exp(expressionable, token);
    }
    else
    {
        res = expressionable_parse_token(expressionable, token, flags);
    }

    void *node = expressionable_node_pop(expressionable);
  
    if (expressionable_callbacks(expressionable)->expecting_additional_node(expressionable, node))
    {
        // Okay we are expecting an extra single node here according to the implementor
        // For example in the case of "defined ABC" in a preprocessor, both the defined
        // token and ABC are important, if we have a defined keyword than we are
        // expecting an extra node to complete that equation, hense
        // why expecting_additional_node would return true in this particular case.
        // We would have dealt with "defined" at this point in time, now comes the next
        // single.
        expressionable_parse_single(expressionable);

        // Okay now we have also parsed the additional node, lets see if they require joining
        void *additional_node = expressionable_node_peek_or_null(expressionable);
        if (expressionable_callbacks(expressionable)->should_join_nodes(expressionable, node, additional_node))
        {
            void *new_node = expressionable_callbacks(expressionable)->join_nodes(expressionable, node, additional_node);
            // Pop off the node
            expressionable_node_pop(expressionable);
            node = new_node;
        }
    }

    // Push the new node to the stack
    expressionable_node_push(expressionable, node);

    return res;
}

int expressionable_parse_single(struct expressionable *expressionable)
{
    return expressionable_parse_single_with_flags(expressionable, 0);
}

/**
 * Shifts the children of the node to the left.
 * 
 * I.e 50*E(20+120) will become E(50*20)+120
 */
void expressionable_parser_node_shift_children_left(struct expressionable *expressionable, void *node)
{
    int node_type = expressionable_callbacks(expressionable)->get_node_type(expressionable, node);
    assert(node_type == EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION);

    void *left_node = expressionable_callbacks(expressionable)->get_left_node(expressionable, node);
    void *right_node = expressionable_callbacks(expressionable)->get_right_node(expressionable, node);
    int right_node_type = expressionable_callbacks(expressionable)->get_node_type(expressionable, right_node);
    assert(right_node_type == EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION);

    const char *right_op = expressionable_callbacks(expressionable)->get_node_operator(expressionable, right_node);
    void *new_exp_left_node = left_node;
    void *new_exp_right_node = expressionable_callbacks(expressionable)->get_left_node(expressionable, right_node);

    const char *node_op = expressionable_callbacks(expressionable)->get_node_operator(expressionable, node);
    // Make the new left operand
    expressionable_callbacks(expressionable)->make_expression_node(expressionable, new_exp_left_node, new_exp_right_node, node_op);

    void *new_left_operand = expressionable_node_pop(expressionable);
    void *new_right_operand = expressionable_callbacks(expressionable)->get_right_node(expressionable, right_node);
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

    int left_node_type = expressionable_callbacks(expressionable)->get_node_type(expressionable, left_node);
    int right_node_type = expressionable_callbacks(expressionable)->get_node_type(expressionable, right_node);
    assert(left_node_type >= 0);
    assert(right_node_type >= 0);

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

            void **address_of_left = expressionable_callbacks(expressionable)->get_left_node_address(expressionable, node);
            void **address_of_right = expressionable_callbacks(expressionable)->get_right_node_address(expressionable, node);

            expressionable_parser_reorder_expression(expressionable, address_of_left);
            expressionable_parser_reorder_expression(expressionable, address_of_right);
        }
    }
}

void expressionable_parse_for_operator(struct expressionable *expressionable)
{
    struct token *op_token = expressionable_peek_next(expressionable);
    const char *op = op_token->sval;
    // We must pop the last node as this will be the left operand
    void *node_left = expressionable_node_peek_or_null(expressionable);
    if (!node_left)
    {
        // If we have a NULL Left node then this expression has no left operand
        // I.e it looks like this "-a" or "*b". These are unary operators of course
        // Let's deal with it
        if (!is_unary_operator(op))
        {
            expressionable_error(expressionable, "the given expression has no left operand");
        }

        expressionable_parse_unary(expressionable);
        return;
    }

    // Pop operator token
    expressionable_token_next(expressionable);
    // Pop left node
    expressionable_node_pop(expressionable);

    // We have another operator? Then this one must be a unary or possibly parentheses
    if (expressionable_peek_next(expressionable)->type == TOKEN_TYPE_OPERATOR)
    {
        if (S_EQ(expressionable_peek_next(expressionable)->sval, "("))
        {
            expressionable_parse_parentheses(expressionable);
        }
        else if (is_unary_operator(expressionable_peek_next(expressionable)->sval))
        {
            // Parse the unary
            expressionable_parse_unary(expressionable);
        }
        else
        {
            expressionable_error(expressionable, "Two operators are not expected for a given expression for operator %s\n", expressionable_peek_next(expressionable)->sval);
        }
    }
    else
    {
        // We must parse the right operand
        expressionable_parse(expressionable);
    }
    void *node_right = expressionable_node_pop(expressionable);

    expressionable_callbacks(expressionable)->make_expression_node(expressionable, node_left, node_right, op);
    void *exp_node = expressionable_node_pop(expressionable);

    // We must reorder the expression if possible
    expressionable_parser_reorder_expression(expressionable, &exp_node);
    expressionable_node_push(expressionable, exp_node);
}

void expressionable_parse(struct expressionable *expressionable)
{
    while (expressionable_parse_single(expressionable) == 0)
    {
    }
}
