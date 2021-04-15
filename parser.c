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
// This array is special, its essentially a group of arrays
// Format goes as follow

#define TOTAL_OPERATOR_GROUPS 13
#define MAX_OPERATORS_IN_GROUP 12
/**
 * Format: 
 * {operator1, operator2, operator3, NULL}
 * 
 * end each group with NULL.
 * 
 * Also end the collection of groups with a NULL pointer
 */

static char *op_precedence[TOTAL_OPERATOR_GROUPS][MAX_OPERATORS_IN_GROUP] = {
    {"++", "--", "()", "[", "]", ".", "->", NULL},
    {"*", "/", "%%", NULL},
    {"+", "-", NULL},
    {"<<", ">>", NULL},
    {"<", "<=", ">", ">=", NULL},
    {"==", "!=", NULL},
    {"&", NULL},
    {"^", NULL},
    {"|", NULL},
    {"&&", NULL},
    {"||", NULL},
    {"?", ":", NULL},
    {"=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=", NULL}};

static struct compile_process *current_process;
int parse_next();
void parse_statement();

#define parse_err(...) \
    compiler_error(current_process, __VA_ARGS__)

static int parser_get_precedence_for_operator(const char *op)
{
    for (int i = 0; i < TOTAL_OPERATOR_GROUPS; i++)
    {
        for (int b = 0; op_precedence[i][b]; b++)
        {
            const char *_op = op_precedence[i][b];
            if (S_EQ(op, _op))
            {
                return i;
            }
        }
    }

    return -1;
}

static bool parser_left_op_has_priority(const char *op_left, const char *op_right)
{
    int precedence_left = parser_get_precedence_for_operator(op_left);
    int precedence_right = parser_get_precedence_for_operator(op_right);
    return precedence_left <= precedence_right;
}

static struct token *token_next()
{
    return vector_peek(current_process->token_vec);
}

static struct token *token_peek_next()
{
    return vector_peek_no_increment(current_process->token_vec);
}

static bool token_next_is_operator(const char *op)
{
    struct token *token = token_peek_next();
    return token && token->type == TOKEN_TYPE_OPERATOR && S_EQ(token->sval, op);
}

static bool token_next_is_symbol(char sym)
{
    struct token *token = token_peek_next();
    return token && token->type == TOKEN_TYPE_SYMBOL && token->cval == sym;
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

static void expect_op(const char *op)
{
    struct token *next_token = token_next();
    if (next_token == NULL || next_token->type != TOKEN_TYPE_OPERATOR || !S_EQ(next_token->sval, op))
        parse_err("Expecting the operator %s but something else was provided", op);
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

void make_function_node(struct datatype *ret_type, const char *name, struct vector *arguments, struct node *body)
{
    node_create(&(struct node){NODE_TYPE_FUNCTION, .func.rtype = *ret_type, .func.name = name, .func.argument_vector = arguments, .func.body_node = body});
}

void make_body_node(struct vector *body_vec)
{
    node_create(&(struct node){NODE_TYPE_BODY, .body.statements = body_vec});
}

void make_variable_node(struct datatype *datatype, struct token *name_token, struct node *value_node)
{
    node_create(&(struct node){NODE_TYPE_VARIABLE, .var.type = *datatype, .var.name = name_token->sval, .var.val = value_node});
}

void parse_expressionable();
void parse_for_parentheses();

void parse_exp()
{
    if (S_EQ(token_peek_next()->sval, "("))
    {
        parse_for_parentheses();
        
        return;
    }

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
        parse_single_token_to_node();
        res = 0;
        break;
    case TOKEN_TYPE_IDENTIFIER:
        parse_single_token_to_node();
        res = 0;
        break;
    case TOKEN_TYPE_OPERATOR:
        parse_exp();
        res = 0;
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
    expect_op("(");
    parse_expressionable();
    expect_sym(')');

    struct node* left_node = node_pop();
    struct node* right_node = NULL;
    
    // DO we have another node? If we do its the left node 
    struct node* onode = node_peek_or_null();
    if (onode)
    {
        node_pop();
        right_node = left_node;
        left_node = onode;
    }
    make_exp_node(left_node, right_node, "()");
}

void parse_for_symbol()
{
    struct token *token = token_peek_next();
    parse_err("not supported yet");
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
        datatype->size = 1;
    }
    else if (S_EQ(datatype_token->sval, "short"))
    {
        datatype->type = DATA_TYPE_SHORT;
        datatype->size = 2;
    }
    else if (S_EQ(datatype_token->sval, "int"))
    {
        datatype->type = DATA_TYPE_INTEGER;
        datatype->size = 4;
    }
    else if (S_EQ(datatype_token->sval, "long"))
    {
        datatype->type = DATA_TYPE_LONG;
        datatype->size = 8;
    }
    else if (S_EQ(datatype_token->sval, "float"))
    {
        datatype->type = DATA_TYPE_FLOAT;
        datatype->size = 4;
    }
    else if (S_EQ(datatype_token->sval, "double"))
    {
        datatype->type = DATA_TYPE_DOUBLE;
        datatype->size = 8;
    }

    datatype->type_str = datatype_token->sval;
}

void parse_variable(struct datatype *dtype, struct token *name_token)
{
    struct node *value_node = NULL;
    // We have a datatype and a variable name but we still need to parse a value if their is one
    // Lets do that now
    if (token_next_is_operator("="))
    {
        // Yeah we got an assignment with the variable declaration
        // so it looks something like this "int a = 50;".
        // Now we now we are at the "=" lets pop it off the token stack
        token_next();
        // Parse the value expression i.e the "50"
        parse_expressionable();
        value_node = node_pop();
    }

    make_variable_node(dtype, name_token, value_node);
}

/**
 * Unlike the "parse_variable" function this function does not expect you 
 * to know the datatype or the name of the variable, it parses that for you
 */
void parse_variable_full()
{
    // Null by default, making this an unsigned non-static variable
    struct datatype dtype;
    parse_datatype_modifiers(&dtype);
    parse_datatype_type(&dtype);

    // Ok great we have a datatype at this point, next comes the variable name
    // or the function name.. we don't know which one yet ;)

    struct token *name_token = token_next();
    parse_variable(&dtype, name_token);
}

void parse_function_argument()
{
    parse_variable_full();
}

/**
 * Parses the function arguments and returns a vector of function arguments
 * that were parsed succesfully
 */
struct vector *parse_function_arguments()
{
    struct vector *arguments_vec = vector_create(sizeof(struct node *));
    // If we see a right bracket we are at the end of the function arguments i.e (int a, int b)
    while (!token_next_is_symbol(')'))
    {
        parse_variable_full();
        // Push the parsed argument variable into the arguments vector
        struct node *argument_node = node_pop();
        vector_push(arguments_vec, &argument_node);

        // Loop until no more function arguments are present
        if (!token_next_is_symbol(','))
        {
            break;
        }

        // Skip the comma
        token_next();
    }

    return arguments_vec;
}

void parse_body_single_statement(struct vector *body_vec)
{
    struct node *stmt_node = NULL;
    parse_statement();
    stmt_node = node_pop();
    vector_push(body_vec, &stmt_node);

    // Let's make the body node for this one statement.
    make_body_node(body_vec);
}

void parse_body_multiple_statements(struct vector *body_vec)
{
    struct node *stmt_node = NULL;
    // Ok we are parsing a full body with many statements.
    expect_sym('{');
    while (!token_next_is_symbol('}'))
    {
        parse_statement();
        stmt_node = node_pop();
        vector_push(body_vec, &stmt_node);
    }
    // bodies must end with a right curley bracket!
    expect_sym('}');

    // Let's make the body node now we have parsed all statements.
    make_body_node(body_vec);
}

void parse_body()
{
    struct vector *body_vec = vector_create(sizeof(struct node *));
    // We don't have a left curly? Then this body composes of only one statement
    if (!token_peek_next('{'))
    {
        parse_body_single_statement(body_vec);
        return;
    }

    // We got a couple of statements between curly braces {int a; int b;}
    parse_body_multiple_statements(body_vec);
}

void parse_function_body()
{
    parse_body();
}

void parse_function(struct datatype *dtype, struct token *name_token)
{
    struct vector *arguments_vector = NULL;
    // We expect a left bracket for functions.
    // Let us not forget we already have the return type and name of the function i.e int abc
    expect_op("(");
    arguments_vector = parse_function_arguments();
    expect_sym(')');

    // Do we have a function body or is this a declaration?
    if (token_next_is_symbol('{'))
    {
        // Parse the function body
        parse_function_body();
        struct node *body_node = node_pop();
        // Create the function node
        make_function_node(dtype, name_token->sval, arguments_vector, body_node);
        return;
    }

    // Ok then this is a function declaration wtihout a body, in which case
    // we expect a semicolon
    expect_sym(';');
    make_function_node(dtype, name_token->sval, arguments_vector, NULL);
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

    struct token *name_token = token_next();

    // If we have a left bracket then this must be a function i.e int abc()
    // Let's handle the function
    if (token_next_is_operator("("))
    {
        parse_function(&dtype, name_token);
        return;
    }

    // Since this is not a function it has to be a variable
    // Let's handle the variable
    parse_variable(&dtype, name_token);
    // We expect variable declarations to end with ";"
    expect_sym(';');
}

void parse_keyword()
{
    struct token *token = token_peek_next();
    // keyword_is_datatype is temporary because custom types can exist
    // Therefore variable declarations will be the appropaite action
    // if all other keywords are not present.
    // This will be changed soon
    if (is_keyword_variable_modifier(token->sval) || keyword_is_datatype(token->sval))
    {
        parse_variable_or_function();
        return;
    }

    parse_err("Unexpected keyword %s\n", token->sval);
}

/**
 * Statements are essentially assignments, variable declarations, for loops
 * if statements and so on. THeir is no node type of statment, there are however
 * nodes that fit in this statement category and should be parsed as such.
 */
void parse_statement()
{
    // Statements are composed of keywords or expressions
    if (token_peek_next()->type == TOKEN_TYPE_KEYWORD)
    {
        parse_keyword();
        return;
    }

    // This must be an expression as its not a keyword
    // I.e a = 50;
    parse_expressionable();
    // Expression statements must end with a semicolon
    expect_sym(';');
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
    case TOKEN_TYPE_OPERATOR:
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
    parser_get_all_nodes_of_type_for_vector(vector, node->func.body_node->body.statements, type, ignore_childtypes_for_type);
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

/**
 * Reorders the given expression and its children, based on operator priority. I.e 
 * multiplication takes priority over addition.
 */
void parser_reorder_expression(struct node **node_out)
{
    struct node *node = *node_out;
    // We must first reorder the children

    if (node->exp.left->type == NODE_TYPE_EXPRESSION)
    {
        parser_reorder_expression(&node->exp.left);
    }

    if (node->exp.right && node->exp.right->type == NODE_TYPE_EXPRESSION)
    {
        // Don't forget to reorder the right node first
        parser_reorder_expression(&node->exp.right);

        struct node *right_node = node->exp.right;
        if (parser_left_op_has_priority(node->exp.op, right_node->exp.op))
        {
            // Left has priority so we must take the left node of the right node.
            // and reorder the expression
            make_exp_node(node->exp.left, right_node->exp.left, node->exp.op);
            struct node *new_root_exp_node = node_pop();
            right_node->exp.left = new_root_exp_node;
            node = right_node;
            *node_out = node;
        }
    }
}
void parser_reorder_expressions(struct compile_process *process)
{
    struct vector *exp_nodes = parser_get_all_nodes_of_type(process, NODE_TYPE_EXPRESSION, true);
    struct node **node_out = NULL;
    while (!vector_empty(exp_nodes))
    {
        node_out = vector_back_ptr(exp_nodes);
        parser_reorder_expression(node_out);
        vector_pop(exp_nodes);
    }
    vector_free(exp_nodes);
}
int parse(struct compile_process *process)
{

    struct vector *vec1 = vector_create(sizeof(int *));
    struct vector *vec2 = vector_create(sizeof(int **));

    int a = 50;
    vector_push(vec1, &a);
    int **ptr = vector_back(vec1);
    vector_push(vec2, &ptr);

    int **ptr2 = (int **)(vector_back(vec2));
    **ptr2 = 90;
    printf("%i\n", **(int **)(vector_back(vec2)));

    printf("%i\n", *(int *)(vector_back(vec1)));

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

    // Now that we have all we need lets loop through the nodes and we will reavaluate the expressions
  //  parser_reorder_expressions(process);
    return PARSE_ALL_OK;
}