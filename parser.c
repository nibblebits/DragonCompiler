#include "compiler.h"
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <stdarg.h>
#include <memory.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

// First in the array = higher priority
static const char *op_precedence[] = {"*", "/", "%%", "+", "-"};
static struct compile_process *current_process;
int parse_next();
#define parse_err(...) \
    compiler_error(current_process, __VA_ARGS__)

static struct token *token_next()
{
    return vector_peek(current_process->token_vec);
}

static struct token *token_peek_next()
{
    return vector_peek_no_increment(current_process->token_vec);
}
static struct token *token_next_expected(int type)
{
    struct token *token = token_next();
    if (token->type != type)
        parse_err("Unexpected token\n");

    return token;
}

static void expect_sym(char c)
{
    struct token *next_token = token_next();
    if (next_token == NULL || next_token->type != TOKEN_TYPE_SYMBOL || next_token->cval != c)
        parse_err("Expecting the symbol %c but something else was provided", c);
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
    struct node *last_node = *((struct node **)(vector_back(current_process->node_vec)));
    struct node *last_node_root = *((struct node **)(vector_back(current_process->node_tree_vec)));

    vector_pop(current_process->node_vec);

    if (last_node == last_node_root)
    {
        // We also have pushed this node to the tree root so we need to pop from here too
        vector_pop(current_process->node_tree_vec);
    }

    return last_node;
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
    node_push(node);
    return node;
}

static bool is_keyword_variable_modifier(const char *val)
{
    return S_EQ(val, "unsigned") ||
           S_EQ(val, "signed") ||
           S_EQ(val, "static");
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

    default:
        parse_err("Problem converting token to node. No valid node exists for token of type %i\n", token->type);
    }
}

void make_exp_node(struct node *node_left, struct node *node_right, const char *op)
{
    node_create(&(struct node){NODE_TYPE_EXPRESSION, .exp.op = op, .exp.left = node_left, .exp.right = node_right});
}

void make_variable_node(struct datatype* datatype, struct token* name_token, struct node* value_node)
{
    node_create(&(struct node){NODE_TYPE_VARIABLE, .var.type=*datatype, .var.name=name_token->sval, .var.val=value_node});
}

void parse_expressionable();
void parse_for_parentheses();

void parse_exp()
{
    struct token *op_token = token_next();
    const char *op = op_token->sval;
    // We must pop the last node as this will be the left operand
    struct node *node_left = node_pop();
    // We must parse the right operand
    parse_expressionable();
    struct node *node_right = node_pop();

    make_exp_node(node_left, node_right, op);
}

int parse_expressionable_single()
{
    struct token *token = token_peek_next();
    if (!token)
        return -1;

    int res = -1;
    switch (token->type)
    {
    case TOKEN_TYPE_NUMBER:
    case TOKEN_TYPE_IDENTIFIER:
        parse_single_token_to_node();
        res = 0;
        break;
    case TOKEN_TYPE_OPERATOR:
        parse_exp();
        res = 0;
        break;

    case TOKEN_TYPE_SYMBOL:
        if (token->cval == '(')
        {
            parse_for_parentheses();
            res = 0;
        }
        break;
    }
    return res;
}
void parse_expressionable()
{
    struct node *last_node = NULL;
    while (parse_expressionable_single() == 0)
    {
    }
}

void parse_for_parentheses()
{
    expect_sym('(');
    parse_expressionable();
    expect_sym(')');
}

void parse_for_symbol()
{
    struct token *token = token_peek_next();
    switch (token->cval)
    {
    case '(':
        parse_for_parentheses();
        break;
    default:
        parse_err("Unexpected symbol %c\n", token->cval);
    }
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
        // We dealt with this modifier token, move along.
        token_next();
        token = token_peek_next();
    }
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

    // consider changing to an array that we can just map index too ;)
    // too many ifs...
    if (S_EQ(datatype_token->sval, "char"))
    {
        datatype->type = DATA_TYPE_CHAR;
    }
    else if (S_EQ(datatype_token->sval, "short"))
    {
        datatype->type = DATA_TYPE_SHORT;
    }
    else if (S_EQ(datatype_token->sval, "int"))
    {
        datatype->type = DATA_TYPE_INTEGER;
    }
    else if (S_EQ(datatype_token->sval, "long"))
    {
        datatype->type = DATA_TYPE_LONG;
    }
    else if (S_EQ(datatype_token->sval, "float"))
    {
        datatype->type = DATA_TYPE_FLOAT;
    }
    else if (S_EQ(datatype_token->sval, "double"))
    {
        datatype->type = DATA_TYPE_DOUBLE;
    }

    datatype->type_str = datatype_token->sval;
}

void parse_variable(struct datatype* dtype, struct token* name_token)
{
    // We have a datatype and a variable name but we still need to parse a value.
    // Lets do that now
    parse_expressionable();
    struct node* value_node = node_pop();
    make_variable_node(dtype, name_token, value_node);
}
/**
 * Parses a variable or function, at this point the parser should be certain
 * that the tokens coming up will form a variable or a function
 */
void parse_variable_or_function()
{
    //  Variables have datatypes, and so do functions have return types.
    //  lets parse the data type

    // Null by default, making this an unsigned non-static variable
    struct datatype dtype;
    parse_datatype_modifiers(&dtype);
    parse_datatype_type(&dtype);

    // Ok great we have a datatype at this point, next comes the variable name
    // or the function name.. we don't know which one yet ;)

    struct token* name_token = token_next();
    parse_variable(&dtype, name_token);

}

void parse_keyword()
{
    struct token *token = token_peek_next();
    if (is_keyword_variable_modifier(token->sval) || keyword_is_datatype(token->sval))
    {
        parse_variable_or_function();
        return;
    }

    parse_err("Unexpected keyword %s\n", token->sval);
}

int parse_next()
{
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
        parse_expressionable();
        break;

    case TOKEN_TYPE_SYMBOL:
        parse_for_symbol();
        break;

    case TOKEN_TYPE_KEYWORD:
        parse_keyword();
        break;

    default:
        parse_err("Unexpected token\n");
    }

    return res;
}

int parse(struct compile_process *process)
{
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

    return PARSE_ALL_OK;
}