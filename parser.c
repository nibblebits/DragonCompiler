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

#define TOTAL_OPERATOR_GROUPS 14
#define MAX_OPERATORS_IN_GROUP 12

// Expression flags
enum
{
    // Signifies the root of an expression i.e ((50*20)+40)
    // The root is ((E)+40) [E+40], this flag would be set
    // for that parse_expression call..
    PARSER_EXPRESSION_IS_ROOT = 0b00000001
};

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
    {.operators = {"++", "--", "()","(", "[", "]", ".", "->", NULL}, .associativity = ASSOCIATIVITY_LEFT_TO_RIGHT},
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

static struct compile_process *current_process;
int parse_next();
void parse_statement();

#define parse_err(...) \
    compiler_error(current_process, __VA_ARGS__)

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

static bool parser_is_unary_operator(const char *op)
{
    return S_EQ(op, "-") || S_EQ(op, "!") || S_EQ(op, "~");
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

static bool token_next_is_keyword(const char *keyword)
{
    struct token *token = token_peek_next();
    return token && token->type == TOKEN_TYPE_KEYWORD && S_EQ(token->sval, keyword);
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

static void expect_keyword(const char *keyword)
{
    struct token *next_token = token_next();
    assert(next_token);
    assert(next_token->type == TOKEN_TYPE_KEYWORD);
    assert(S_EQ(next_token->sval, keyword));

    if (next_token == NULL || next_token->type != TOKEN_TYPE_KEYWORD || !S_EQ(next_token->sval, keyword))
        parse_err("Expecting keyword %s however something else was provided", keyword);
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

void make_exp_parentheses_node(struct node *exp_node)
{
    node_create(&(struct node){NODE_TYPE_EXPRESSION_PARENTHESIS, .parenthesis.exp=exp_node});
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

void make_body_node(struct vector *body_vec, size_t variable_size)
{
    node_create(&(struct node){NODE_TYPE_BODY, .body.statements = body_vec, .body.variable_size = variable_size});
}

void make_variable_node(struct datatype *datatype, struct token *name_token, struct node *value_node)
{
    node_create(&(struct node){NODE_TYPE_VARIABLE, .var.type = *datatype, .var.name = name_token->sval, .var.val = value_node});
}

void parse_expressionable(int flags);
void parse_for_parentheses();

static void parser_append_size_for_node(size_t *variable_size, struct node *node)
{
    if (node->type == NODE_TYPE_VARIABLE)
    {
        // Ok we have a variable lets adjust the variable_size.
        *variable_size += node->var.type.size;
    }
}
/**
 * Parses a single body statement and in the event the statement is a variable
 * the variable_size variable will be incremented by the size of the variable
 * in this statement
 */
void parse_body_single_statement(size_t *variable_size, struct vector *body_vec)
{
    struct node *stmt_node = NULL;
    parse_statement();
    stmt_node = node_pop();
    vector_push(body_vec, &stmt_node);

    // Change the variable_size if this statement is a variable.
    // Incrementing it by the size of our variable
    parser_append_size_for_node(variable_size, stmt_node);

    // Let's make the body node for this one statement.
    make_body_node(body_vec, *variable_size);
}

/**
 * Parses the body_vec vector and for any variables the variable size is calculated
 * and added to the variable_size variable
 */
void parse_body_multiple_statements(size_t *variable_size, struct vector *body_vec)
{
    struct node *stmt_node = NULL;
    // Ok we are parsing a full body with many statements.
    expect_sym('{');
    while (!token_next_is_symbol('}'))
    {
        parse_statement();
        stmt_node = node_pop();
        vector_push(body_vec, &stmt_node);

        // Change the variable_size if this statement is a variable.
        // Incrementing it by the size of our variable
        parser_append_size_for_node(variable_size, stmt_node);
    }
    // bodies must end with a right curley bracket!
    expect_sym('}');

    // Let's make the body node now we have parsed all statements.
    make_body_node(body_vec, *variable_size);
}

/**
 * Parses the body and stores the collective variable size in the "variable_size" variable
 * This can be useful for parsing a structures body and having the size returned of the
 * combined variables.
 * 
 * Note the size will not be 16-bit aligned as required in the C standard.
 */
void parse_body(size_t *variable_size)
{
    // We must always have a variable size pointer
    // if the caller doesn't care about the size we will make our own.
    size_t tmp_size;
    if (!variable_size)
    {
        variable_size = &tmp_size;
    }

    struct vector *body_vec = vector_create(sizeof(struct node *));
    // We don't have a left curly? Then this body composes of only one statement
    if (!token_peek_next('{'))
    {
        parse_body_single_statement(variable_size, body_vec);
        return;
    }

    // We got a couple of statements between curly braces {int a; int b;}
    parse_body_multiple_statements(variable_size, body_vec);
}

/**
 * Reorders the given expression and its children, based on operator priority. I.e 
 * multiplication takes priority over addition.
 */
void parser_reorder_expression(struct node **node_out)
{
    // No reordering for a sec.
    return;

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
        const char* right_op = node->exp.right->exp.op;
        // We have something like 50+E(20+90)
        // We must find the priority operator
        if (parser_left_op_has_priority(node->exp.op, right_op))
        {
            // We have something like 50*E(20+120)
            // We must produce the result E(50*20)+120
            struct node* new_exp_left_node = node->exp.left;
            struct node* new_exp_right_node = node->exp.right->exp.left;
            // Make the new left operand 
            make_exp_node(new_exp_left_node, new_exp_right_node, node->exp.op); 

            struct node* new_left_operand = node_pop();
            struct node* new_right_operand = node->exp.right->exp.right;
            node->exp.left = new_left_operand;
            node->exp.right = new_right_operand;
            node->exp.op = right_op;

            parser_reorder_expression(&node->exp.left);
            parser_reorder_expression(&node->exp.right);
        }
    }
}

void parse_for_unary()
{
    // Let's get the unary operator
    const char *unary_op = token_next()->sval;

    // Now lets parse the expression after this unary operator
    parse_expressionable(0);

    struct node *unary_operand_node = node_pop();
    make_unary_node(unary_op, unary_operand_node);
}

void parse_struct(struct datatype *dtype)
{
    // We already have the structure name parsed, its inside dtype.
    // Parse the body of the structure "struct abc {body_here}"
    size_t body_variable_size = 0;
    parse_body(&body_variable_size);
    struct node *body_node = node_pop();
    make_struct_node(dtype->type_str, body_node);

    // Structures must end with semicolons
    expect_sym(';');
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

static void parser_exp_flags_apply_node_flags(struct node* node_out, int flags)
{
    if (flags & PARSER_EXPRESSION_IS_ROOT)
    {
        node_out->flags |= NODE_FLAG_IS_ROOT_EXPRESSION;
    }

}
void parse_exp_normal(int flags)
{
    struct token *op_token = token_next();
    const char *op = op_token->sval;
    // We must pop the last node as this will be the left operand
    struct node *node_left = node_pop();
    // Left node is now apart of an expression
    node_left->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    // We must parse the right operand
    parse_expressionable(flags & ~PARSER_EXPRESSION_IS_ROOT);

    struct node *node_right = node_pop();
    // Right node is now apart of an expression
    node_right->flags |= NODE_FLAG_INSIDE_EXPRESSION;

    make_exp_node(node_left, node_right, op);
    struct node *exp_node = node_pop();
    parser_exp_flags_apply_node_flags(exp_node, flags);

    // We must reorder the expression if possible
    parser_reorder_expression(&exp_node);
    node_push(exp_node);
}

void parse_exp(int flags)
{
    if (S_EQ(token_peek_next()->sval, "("))
    {
        parse_for_parentheses();
        return;
    }

    if (parser_is_unary_operator(token_peek_next()->sval))
    {
        parse_for_unary();
        return;
    }

    parse_exp_normal(flags);
}

int parse_expressionable_single(int flags)
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
        parse_exp(flags);
        res = 0;
        break;
    }
    return res;
}
void parse_expressionable(int flags)
{
    struct node *last_node = NULL;
    while (parse_expressionable_single(flags) == 0)
    {
    }
}

void parse_for_parentheses()
{
    // We must check to see if we have a left node i.e "test(50+20)". Left node = test
    // If we have a left node we will have to create an expression
    // otherwise we can just create a parentheses node
    struct node* left_node = node_peek_or_null();
    if (left_node)
    {
        node_pop();
    }
    expect_op("(");
    // Just because we have parentheses does not make this a root expression.
    // So no PARSER_EXPRESSION_IS_ROOT flag!
    parse_expressionable(0);
    expect_sym(')');

    struct node* exp_node = node_pop();   
    make_exp_parentheses_node(exp_node);

    // Do we have a left node from earlier before we parsed the parentheses?
    if (left_node)
    {
        // Ok we do so we must create an expression node, whose left node is the left node
        // and whose right node is the parentheses node
        struct node* parentheses_node = node_pop();
        make_exp_node(left_node, parentheses_node, "()");
        
        // At this point we have something like this test(50*20)
        // (hello)(what+50)
        // ****a(90, 30, 40)

        // Now that we know this we know that the expression of the parenthese node
        // is a root expression
        parentheses_node->parenthesis.exp->flags |= NODE_FLAG_IS_ROOT_EXPRESSION;
    }
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

    return node->_struct.body_n->body.variable_size;
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
        datatype_out->type = DATA_TYPE_LONG;
        datatype_out->size = 8;
    }
    else if (S_EQ(datatype_token->sval, "float"))
    {
        datatype_out->type = DATA_TYPE_FLOAT;
        datatype_out->size = 4;
    }
    else if (S_EQ(datatype_token->sval, "double"))
    {
        datatype_out->type = DATA_TYPE_DOUBLE;
        datatype_out->size = 8;
    }
    else
    {
        // I hate else, change this later. We will assume its a struct
        // in future we need to check for this as we have unions.
        datatype_out->type = DATA_TYPE_STRUCT;
        datatype_out->size = size_of_struct(datatype_token->sval);
    }

    datatype_out->flags |= DATATYPE_FLAG_IS_POINTER;
    datatype_out->pointer_depth = pointer_depth;
    datatype_out->type_str = datatype_token->sval;
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
        parse_expressionable(PARSER_EXPRESSION_IS_ROOT);
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
        if (!token_next_is_operator(","))
        {
            break;
        }

        // Skip the comma
        token_next();
    }

    return arguments_vec;
}

void parse_function_body()
{
    parse_body(NULL);
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

static bool is_datatype_struct_or_union(struct datatype *dtype)
{
    return dtype->type == DATA_TYPE_STRUCT || dtype->type == DATA_TYPE_UNION;
}
/**
 * Parses a variable or function, at this point the parser should be certain
 * that the tokens coming up will form a variable or a function
 */
void parse_variable_function_or_struct_union()
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
        parse_function(&dtype, name_token);
        return;
    }

    // Since this is not a function it has to be a variable
    // Let's handle the variable
    parse_variable(&dtype, name_token);
    // We expect variable declarations to end with ";"
    expect_sym(';');
}

void parse_keyword_return()
{
    expect_keyword("return");

    // Ok we parsed the return keyword, lets now parse the expression of the return
    // keyword and then we expect a semicolon ;)

    parse_expressionable(PARSER_EXPRESSION_IS_ROOT);

    struct node *ret_expr = node_pop();
    make_return_node(ret_expr);

    // We expect a semicolon all the time when it comes to return keywords
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
        parse_variable_function_or_struct_union();
        return;
    }

    if (S_EQ(token->sval, "return"))
    {
        parse_keyword_return();
        return;
    }

    parse_err("Unexpected keyword %s\n", token->sval);
}

void parse_keyword_for_global()
{
    parse_keyword();

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
    parse_expressionable(0);
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
        parse_expressionable(PARSER_EXPRESSION_IS_ROOT);
        break;

    case TOKEN_TYPE_SYMBOL:
        parse_for_symbol();
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
    //parser_reorder_expressions(process);
    return PARSE_ALL_OK;
}