#include "compiler.h"
#include "misc.h"
#include "helpers/vector.h"
#include "helpers/buffer.h"

enum
{
    TYPEDEF_TYPE_STANDARD,
    // A structure typedef that looks something like this "typedef struct ABC { int x; } AAA;"
    TYPEDEF_TYPE_STRUCTURE_TYPEDEF
};

struct typedef_type
{
    int type;
    const char *definition_name;
    struct vector *value;
    struct typedef_structure
    {
        // The structure name.
        const char *sname;
    } structure;
};

enum
{
    PREPROCESSOR_NUMBER_NODE,
    PREPROCESSOR_IDENTIFIER_NODE,
    PREPROCESSOR_UNARY_NODE,
    PREPROCESSOR_EXPRESSION_NODE,
    PREPROCESSOR_PARENTHESES_NODE
};

struct preprocessor_node
{
    int type;
    struct preprocessor_const_val
    {
        union
        {
            char cval;
            unsigned int inum;
            long lnum;
            long long llnum;
            unsigned long ulnum;
            unsigned long long ullnum;
        };
    } const_val;

    union
    {
        struct preprocessor_exp_node
        {
            struct preprocessor_node *left;
            struct preprocessor_node *right;
            const char *op;
        } exp;

        struct preprocessor_unary_node
        {
            struct preprocessor_node *operand_node;
            const char *op;
        } unary_node;

        /**
         * Represents a parenthesis expression i.e (50+20) (EXP)
         */
        struct preprocessor_parenthesis
        {
            // The expression between the brackets ()
            struct preprocessor_node *exp;
        } parenthesis;
    };

    const char *sval;
};

struct vector *preprocessor_build_value_vector_for_integer(int value)
{
    struct vector *token_vec = vector_create(sizeof(struct token));
    struct token t1 = {};
    t1.type = TOKEN_TYPE_NUMBER;
    t1.llnum = value;
    vector_push(token_vec, &t1);
    return token_vec;
}

void preprocessor_token_vec_push_keyword_and_identifier(struct vector *token_vec, const char *keyword, const char *identifier)
{
    struct token t1 = {};
    t1.type = TOKEN_TYPE_KEYWORD;
    t1.sval = keyword;
    struct token t2 = {};
    t2.type = TOKEN_TYPE_IDENTIFIER;
    t2.sval = identifier;

    vector_push(token_vec, &t1);
    vector_push(token_vec, &t2);
}
void *preprocessor_node_create(struct preprocessor_node *node)
{
    struct preprocessor_node *result = calloc(sizeof(struct preprocessor_node), 1);
    memcpy(result, node, sizeof(struct preprocessor_node));
    return result;
}

void *preprocessor_handle_number_token(struct expressionable *expressionable)
{
    struct token *token = expressionable_token_next(expressionable);
    return preprocessor_node_create(&(struct preprocessor_node){.type = PREPROCESSOR_NUMBER_NODE, .const_val.llnum = token->llnum});
}

void *preprocessor_handle_identifier_token(struct expressionable *expressionable)
{
    struct token *token = expressionable_token_next(expressionable);
    return preprocessor_node_create(&(struct preprocessor_node){.type = PREPROCESSOR_IDENTIFIER_NODE, .sval = token->sval});
}

void preprocessor_make_expression_node(struct expressionable *expressionable, void *left_node_ptr, void *right_node_ptr, const char *op)
{
    struct preprocessor_node exp_node;
    exp_node.type = PREPROCESSOR_EXPRESSION_NODE;
    exp_node.exp.left = left_node_ptr;
    exp_node.exp.right = right_node_ptr;
    exp_node.exp.op = op;

    expressionable_node_push(expressionable, preprocessor_node_create(&exp_node));
}

void preprocessor_make_parentheses_node(struct expressionable *expressionable, void *node_ptr)
{
    struct preprocessor_node *node = node_ptr;
    struct preprocessor_node parentheses_node;
    parentheses_node.type = PREPROCESSOR_PARENTHESES_NODE;
    parentheses_node.parenthesis.exp = node_ptr;
    expressionable_node_push(expressionable, preprocessor_node_create(&parentheses_node));
}

void *preprocessor_get_left_node(struct expressionable *expressionable, void *target_node)
{
    struct preprocessor_node *node = target_node;
    return node->exp.left;
}

void *preprocessor_get_right_node(struct expressionable *expressionable, void *target_node)
{
    struct preprocessor_node *node = target_node;
    return node->exp.right;
}

int preprocessor_get_node_type(struct expressionable *expressionable, void *node)
{
    int generic_type = EXPRESSIONABLE_GENERIC_TYPE_NON_GENERIC;
    struct preprocessor_node *preprocessor_node = node;
    switch (preprocessor_node->type)
    {
    case PREPROCESSOR_NUMBER_NODE:
        generic_type = EXPRESSIONABLE_GENERIC_TYPE_NUMBER;
        break;

    case PREPROCESSOR_IDENTIFIER_NODE:
        generic_type = EXPRESSIONABLE_GENERIC_TYPE_IDENTIFIER;
        break;

    case PREPROCESSOR_UNARY_NODE:
        generic_type = EXPRESSIONABLE_GENERIC_TYPE_UNARY;
        break;
    case PREPROCESSOR_EXPRESSION_NODE:
        generic_type = EXPRESSIONABLE_GENERIC_TYPE_EXPRESSION;
        break;

    case PREPROCESSOR_PARENTHESES_NODE:
        generic_type = EXPRESSIONABLE_GENERIC_TYPE_PARENTHESES;
        break;
    }
    return generic_type;
}

const char *preprocessor_get_node_operator(struct expressionable *expressionable, void *target_node)
{
    struct preprocessor_node *preprocessor_node = target_node;
    return preprocessor_node->exp.op;
}

void **preprocessor_get_left_node_address(struct expressionable *expressionable, void *target_node)
{
    return (void **)&((struct preprocessor_node *)(target_node))->exp.left;
}

void **preprocessor_get_right_node_address(struct expressionable *expressionable, void *target_node)
{
    return (void **)&((struct preprocessor_node *)(target_node))->exp.right;
}

void preprocessor_set_expression_node(struct expressionable *expressionable, void *node, void *left_node, void *right_node, const char *op)
{
    struct preprocessor_node *preprocessor_node = node;
    preprocessor_node->exp.left = left_node;
    preprocessor_node->exp.right = right_node;
    preprocessor_node->exp.op = op;
}

void *preprocessor_make_unary_node(struct expressionable *expressionable, const char *op, void *right_operand_node_ptr)
{
    struct preprocessor_node *right_operand_node = right_operand_node_ptr;
    return preprocessor_node_create(&(struct preprocessor_node){.type = PREPROCESSOR_UNARY_NODE, .unary_node.op = op, .unary_node.operand_node = right_operand_node_ptr});
}

struct expressionable_config preprocessor_expressionable_config =
    {
        .callbacks.handle_number_callback = preprocessor_handle_number_token,
        .callbacks.handle_identifier_callback = preprocessor_handle_identifier_token,
        .callbacks.make_unary_node = preprocessor_make_unary_node,
        .callbacks.make_expression_node = preprocessor_make_expression_node,
        .callbacks.make_parentheses_node = preprocessor_make_parentheses_node,
        .callbacks.get_node_type = preprocessor_get_node_type,
        .callbacks.get_left_node = preprocessor_get_left_node,
        .callbacks.get_right_node = preprocessor_get_right_node,
        .callbacks.get_node_operator = preprocessor_get_node_operator,
        .callbacks.get_left_node_address = preprocessor_get_left_node_address,
        .callbacks.get_right_node_address = preprocessor_get_right_node_address,
        .callbacks.set_exp_node = preprocessor_set_expression_node};

struct preprocessor_function_argument
{
    // Tokens for this argument struct token
    struct vector *tokens;
};

struct preprocessor_function_arguments
{
    // Vector of struct preprocessor_function_argument
    struct vector *arguments;
};

void preprocessor_handle_token(struct compile_process *compiler, struct token *token);

struct preprocessor_function_arguments *preprocessor_function_arguments_create()
{
    struct preprocessor_function_arguments *args = calloc(sizeof(struct preprocessor_function_arguments), 1);
    args->arguments = vector_create(sizeof(struct preprocessor_function_argument));
    return args;
}

int preprocessor_function_arguments_count(struct preprocessor_function_arguments *arguments)
{
    return vector_count(arguments->arguments);
}

struct vector *preprocessor_function_arguments_vector(struct preprocessor_function_arguments *arguments)
{
    return arguments->arguments;
}

void preprocessor_function_argument_free(struct preprocessor_function_argument *argument)
{
    vector_free(argument->tokens);
}

void preprocessor_function_arguments_free(struct preprocessor_function_arguments *arguments)
{
    vector_set_peek_pointer(arguments->arguments, 0);
    struct preprocessor_function_argument *argument = vector_peek(arguments->arguments);
    while (argument)
    {
        preprocessor_function_argument_free(argument);
        argument = vector_peek(arguments->arguments);
    }

    free(arguments);
}

void preprocessor_function_argument_push(struct preprocessor_function_arguments *arguments, struct vector *value_vec)
{
    struct preprocessor_function_argument arg;
    arg.tokens = vector_clone(value_vec);
    vector_push(arguments->arguments, &arg);
}

struct preprocessor *compiler_preprocessor(struct compile_process *compiler)
{
    return compiler->preprocessor;
}

struct token *preprocessor_previous_token(struct compile_process *compiler)
{
    return vector_peek_at(compiler->token_vec_original, compiler->token_vec_original->pindex - 1);
}
struct token *preprocessor_next_token(struct compile_process *compiler)
{
    return vector_peek(compiler->token_vec_original);
}

struct token *preprocessor_next_token_no_increment(struct compile_process *compiler)
{
    return vector_peek_no_increment(compiler->token_vec_original);
}

void preprocessor_token_push_semicolon(struct compile_process *compiler)
{
    struct token t1;
    t1.type = TOKEN_TYPE_SYMBOL;
    t1.cval = ';';
    vector_push(compiler->token_vec, &t1);
}

void preprocessor_token_push_dst(struct compile_process *compiler, struct token *token)
{
    struct token t = *token;
    vector_push(compiler->token_vec, &t);
}

void preprocessor_token_vec_push_dst(struct compile_process *compiler, struct vector *token_vec)
{
    vector_set_peek_pointer(token_vec, 0);
    struct token *token = vector_peek(token_vec);
    while (token)
    {
        vector_push(compiler->token_vec, token);
        token = vector_peek(token_vec);
    }
}

bool preprocessor_is_preprocessor_keyword(const char *value)
{
    return S_EQ(value, "define") ||
           S_EQ(value, "if") ||
           S_EQ(value, "ifdef") ||
           S_EQ(value, "ifndef") ||
           S_EQ(value, "endif") ||
           S_EQ(value, "include") ||
           S_EQ(value, "typedef");
}
bool preprocessor_token_is_preprocessor_keyword(struct token *token)
{
    return token->type == TOKEN_TYPE_IDENTIFIER || token->type == TOKEN_TYPE_KEYWORD && preprocessor_is_preprocessor_keyword(token->sval);
}

bool preprocessor_token_is_include(struct token *token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }
    return S_EQ(token->sval, "include");
}

bool preprocessor_token_is_typedef(struct token *token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }

    return S_EQ(token->sval, "typedef");
}

bool preprocessor_token_is_define(struct token *token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }

    return (S_EQ(token->sval, "define"));
}

bool preprocessor_token_is_ifdef(struct token *token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }

    return (S_EQ(token->sval, "ifdef"));
}

bool preprocessor_token_is_if(struct token *token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }

    return (S_EQ(token->sval, "if"));
}

struct compile_process *preprocessor_compiler(struct preprocessor *preprocessor)
{
    return preprocessor->compiler;
}

struct vector *preprocessor_definition_value_for_standard(struct preprocessor_definition *definition)
{
    return definition->standard.value;
}

struct vector *preprocessor_definition_value_for_native(struct preprocessor_definition *definition)
{
    return definition->native.value(definition);
}

struct vector *preprocessor_definition_value_for_typedef(struct preprocessor_definition *definition)
{
    return definition->_typedef.value;
}

/**
 * Returns the token value vector for this given definition
 */
struct vector *preprocessor_definition_value(struct preprocessor_definition *definition)
{
    if (definition->type == PREPROCESSOR_DEFINITION_NATIVE_CALLBACK)
    {
        return preprocessor_definition_value_for_native(definition);
    }
    else if (definition->type == PREPROCESSOR_DEFINITION_TYPEDEF)
    {
        return preprocessor_definition_value_for_typedef(definition);
    }

    return preprocessor_definition_value_for_standard(definition);
}

int preprocessor_definition_evaluated_value_for_standard(struct preprocessor_definition *definition)
{
    struct token *token = vector_back(definition->standard.value);
    if (token->type != TOKEN_TYPE_NUMBER)
    {
        compiler_error(preprocessor_compiler(definition->preprocessor), "The definition %s must hold a number value, unable to use macro IF", definition->name);
    }
    return token->llnum;
}

int preprocessor_definition_evaluated_value_for_native(struct preprocessor_definition *definition)
{
    return definition->native.evaluate(definition);
}

int preprocessor_definition_evaluated_value(struct preprocessor_definition *definition)
{
    if (definition->type == PREPROCESSOR_DEFINITION_STANDARD)
    {
        return preprocessor_definition_evaluated_value_for_standard(definition);
    }
    else if (definition->type == PREPROCESSOR_DEFINITION_NATIVE_CALLBACK)
    {
        return preprocessor_definition_evaluated_value_for_native(definition);
    }

    compiler_error(preprocessor_compiler(definition->preprocessor), "The definition %s cannot be evaluated into a number");
}

/**
 * Searches for a hashtag symbol along with the given identifier.
 * If found the hashtag token and identifier token are both popped from the stack
 * Only the target identifier token representing "str" is returned.
 * 
 * NO match return null.
 */
struct token *preprocessor_hashtag_and_identifier(struct compile_process *compiler, const char *str)
{
    // No token then how can we continue?
    if (!preprocessor_next_token_no_increment(compiler))
        return NULL;

    if (!token_is_symbol(preprocessor_next_token_no_increment(compiler), '#'))
    {
        return NULL;
    }

    vector_save(compiler->token_vec_original);
    // Ok skip the hashtag symbol
    preprocessor_next_token(compiler);

    struct token *target_token = preprocessor_next_token_no_increment(compiler);
    if (token_is_identifier(target_token, str))
    {
        // Pop off the target token
        preprocessor_next_token(compiler);
        // Purge the vector save
        vector_save_purge(compiler->token_vec_original);
        return target_token;
    }

    vector_restore(compiler->token_vec_original);

    return NULL;
}

struct preprocessor_definition *preprocessor_get_definition(struct preprocessor *preprocessor, const char *name)
{
    vector_set_peek_pointer(preprocessor->definitions, 0);
    struct preprocessor_definition *definition = vector_peek_ptr(preprocessor->definitions);
    while (definition)
    {
        if (S_EQ(definition->name, name))
        {
            break;
        }
        definition = vector_peek_ptr(preprocessor->definitions);
    }

    return definition;
}

bool preprocessor_token_is_definition_identifier(struct compile_process *compiler, struct token *token)
{
    if (token->type != TOKEN_TYPE_IDENTIFIER)
        return false;

    if (preprocessor_get_definition(compiler->preprocessor, token->sval))
    {
        return true;
    }

    return false;
}

struct preprocessor_definition *preprocessor_definition_create_native(const char *name, PREPROCESSOR_DEFINITION_NATIVE_CALL_EVALUATE evaluate, PREPROCESSOR_DEFINITION_NATIVE_CALL_VALUE value, struct preprocessor *preprocessor)
{
    struct preprocessor_definition *definition = calloc(sizeof(struct preprocessor_definition), 1);
    definition->type = PREPROCESSOR_DEFINITION_NATIVE_CALLBACK;
    definition->name = name;
    definition->native.evaluate = evaluate;
    definition->native.value = value;
    definition->preprocessor = preprocessor;

    vector_push(preprocessor->definitions, &definition);
    return definition;
}

struct preprocessor_definition *preprocessor_definition_create_typedef(const char *name, struct vector *value_vec, struct preprocessor *preprocessor)
{
    struct preprocessor_definition *definition = calloc(sizeof(struct preprocessor_definition), 1);
    definition->type = PREPROCESSOR_DEFINITION_TYPEDEF;
    definition->name = name;
    definition->_typedef.value = value_vec;
    definition->preprocessor = preprocessor;

    vector_push(preprocessor->definitions, &definition);
    return definition;
}

struct preprocessor_definition *preprocessor_definition_create(const char *name, struct vector *value_vec, struct vector *arguments, struct preprocessor *preprocessor)
{
    struct preprocessor_definition *definition = calloc(sizeof(struct preprocessor_definition), 1);
    definition->type = PREPROCESSOR_DEFINITION_STANDARD;
    definition->name = name;
    definition->standard.value = value_vec;
    definition->standard.arguments = arguments;
    definition->preprocessor = preprocessor;

    if (vector_count(definition->standard.arguments))
    {
        definition->type = PREPROCESSOR_DEFINITION_MACRO_FUNCTION;
    }

    vector_push(preprocessor->definitions, &definition);
    return definition;
}

void preprocessor_multi_value_insert_to_vector(struct compile_process *compiler, struct vector *value_token_vec)
{
    struct token *value_token = preprocessor_next_token(compiler);
    while (value_token)
    {
        if (value_token->type == TOKEN_TYPE_NEWLINE)
        {
            break;
        }

        if (token_is_symbol(value_token, '\\'))
        {
            // This allows for another line
            // Skip new line
            preprocessor_next_token(compiler);
            value_token = preprocessor_next_token(compiler);
            continue;
        }

        vector_push(value_token_vec, value_token);
        value_token = preprocessor_next_token(compiler);
    }
}
void preprocessor_handle_definition_token(struct compile_process *compiler)
{
    struct token *name_token = preprocessor_next_token(compiler);

    // Arguments vector in case this definition has function arguments
    struct vector *arguments = vector_create(sizeof(const char *));
    // Do we have a function definition
    if (token_is_operator(preprocessor_next_token_no_increment(compiler), "("))
    {
        preprocessor_next_token(compiler);
        struct token *next_token = preprocessor_next_token(compiler);
        while (!token_is_symbol(next_token, ')'))
        {
            if (next_token->type != TOKEN_TYPE_IDENTIFIER)
            {
                FAIL_ERR("You must provide an identifier in the preprocessor definition");
            }

            // Save the argument for later.
            vector_push(arguments, (void *)next_token->sval);

            next_token = preprocessor_next_token(compiler);
            if (!token_is_operator(next_token, ",") && !token_is_symbol(next_token, ')'))
            {
                FAIL_ERR("Incomplete sequence for macro arguments");
            }

            if (token_is_symbol(next_token, ')'))
            {
                break;
            }

            // Skip the operator ","
            next_token = preprocessor_next_token(compiler);
        }
    }
    // Value can be composed of many tokens
    struct vector *value_token_vec = vector_create(sizeof(struct token));
    preprocessor_multi_value_insert_to_vector(compiler, value_token_vec);

    struct preprocessor *preprocessor = compiler->preprocessor;
    preprocessor_definition_create(name_token->sval, value_token_vec, arguments, preprocessor);
}

void preprocessor_handle_include_token(struct compile_process *compiler)
{
    // We are expecting a file to include, lets check the next token

    // We have the path here
    struct token *file_path_token = preprocessor_next_token(compiler);

    char tmp_filename[512];
    sprintf(tmp_filename, "/usr/include/%s", file_path_token->sval);
    if (!file_exists(file_path_token->sval) && !file_exists(tmp_filename))
    {
        compiler_error(compiler, "The file does not exist %s unable to include", file_path_token->sval);
    }

    // Theirs a chance no string provided, check for this later i.e include <abc.h> no quotes ""
    // Alright lets load and compile the given file
    struct compile_process *new_compile_process = compile_include(file_path_token->sval, compiler);
    assert(new_compile_process);

    // Now that we have the new compile process we must merge the tokens with our own
    preprocessor_token_vec_push_dst(compiler, new_compile_process->token_vec);
}

void preprocessor_handle_typedef_body_for_brackets(struct compile_process *compiler, struct vector *token_vec)
{
    struct token *token = preprocessor_next_token(compiler);
    while (token)
    {
        if (token_is_symbol(token, '{'))
        {
            vector_push(token_vec, token);
            preprocessor_handle_typedef_body_for_brackets(compiler, token_vec);
            token = preprocessor_next_token(compiler);
            continue;
        }
        vector_push(token_vec, token);
        if (token_is_symbol(token, '}'))
        {
            break;
        }

        token = preprocessor_next_token(compiler);
    }
}
void preprocessor_handle_typedef_body(struct compile_process *compiler, struct vector *token_vec, struct typedef_type *td)
{
    memset(td, 0, sizeof(struct typedef_type));
    td->type = TYPEDEF_TYPE_STANDARD;

    struct token *token = preprocessor_next_token(compiler);
    bool next_is_struct_name = false;
    if (token_is_keyword(token, "struct"))
    {
        next_is_struct_name = true;
    }
    while (token)
    {
        if (token_is_symbol(token, '{'))
        {
            // Aha we have a body here assume a structure typedef
            td->type = TYPEDEF_TYPE_STRUCTURE_TYPEDEF;
            vector_push(token_vec, token);
            preprocessor_handle_typedef_body_for_brackets(compiler, token_vec);
            token = preprocessor_next_token(compiler);

            continue;
        }

        if (token_is_symbol(token, ';'))
        {
            break;
        }

        vector_push(token_vec, token);
        token = preprocessor_next_token(compiler);

        if (next_is_struct_name)
        {
            td->structure.sname = token->sval;
            next_is_struct_name = false;
        }
    }

    struct token *name_token = vector_back_or_null(token_vec);
    if (!name_token)
    {
        compiler_error(compiler, "We expected a name token for your typedef");
    }

    td->definition_name = name_token->sval;
}

void preprocessor_handle_typedef_token(struct compile_process *compiler)
{
    // We expect a format like "typedef unsigned int ABC;" the final identifier
    // is the name of this typedef, the rest are what is represented.

    struct vector *token_vec = vector_create(sizeof(struct token));
    struct typedef_type td;
    preprocessor_handle_typedef_body(compiler, token_vec, &td);
    // Pop off the name token
    vector_pop(token_vec);

    // If this is a typedef struct we need to push the structure body
    // to the output so it can be found.
    if (td.type == TYPEDEF_TYPE_STRUCTURE_TYPEDEF)
    {
        preprocessor_token_vec_push_dst(compiler, token_vec);
        // Let's also push a semicolon
        preprocessor_token_push_semicolon(compiler);

        // We need to create a new token vector which contains "struct struct_name"
        // for the value. This will then assign a typedef to the structure
        // that we just dealt with.
        token_vec = vector_create(sizeof(struct token));
        preprocessor_token_vec_push_keyword_and_identifier(token_vec, "struct", td.structure.sname);
    }
    struct preprocessor *preprocessor = compiler->preprocessor;
    preprocessor_definition_create_typedef(td.definition_name, token_vec, preprocessor);
}

void preprocessor_read_to_end_if(struct compile_process *compiler, bool true_clause)
{
    // We have this definition we can proceed with the rest of the body, until
    // an #endif is discovered
    while (preprocessor_next_token_no_increment(compiler) && !preprocessor_hashtag_and_identifier(compiler, "endif"))
    {
        // Read the else statement.
        if (preprocessor_hashtag_and_identifier(compiler, "else"))
        {
            preprocessor_read_to_end_if(compiler, !true_clause);
            break;
        }

        if (true_clause)
        {
            preprocessor_handle_token(compiler, preprocessor_next_token(compiler));
            continue;
        }

        // Skip the unexpected token
        preprocessor_next_token(compiler);
    }
}
void preprocessor_handle_ifdef_token(struct compile_process *compiler)
{
    struct token *condition_token = preprocessor_next_token(compiler);
    if (!condition_token)
    {
        FAIL_ERR("No condition token provided????. WIll replace later with proper error system");
    }

    // Let's see if we have a definition of the condition token name
    struct preprocessor_definition *definition = preprocessor_get_definition(compiler_preprocessor(compiler), condition_token->sval);
    // Ok if the definition exists then we will include the body.
    preprocessor_read_to_end_if(compiler, definition != NULL);
}

int preprocessor_evaluate_number(struct preprocessor_node *node)
{
    return node->const_val.llnum;
}

int preprocessor_evaluate_identifier(struct compile_process *compiler, struct preprocessor_node *node)
{
    struct preprocessor *preprocessor = compiler_preprocessor(compiler);
    struct preprocessor_definition *definition = preprocessor_get_definition(preprocessor, node->sval);
    if (!definition)
    {
        // For some reason C returns true for evaluation if a given definition does not exist. Strange..
        return true;
    }

    if (vector_count(preprocessor_definition_value(definition)) > 1)
    {
        compiler_error(compiler, "The given definition %s has over one value, unable to use macro IF", definition->name);
    }

    if (vector_count(preprocessor_definition_value(definition)) == 0)
    {
        return false;
    }

    return preprocessor_definition_evaluated_value(definition);
}

int preprocessor_arithmetic(struct compile_process *compiler, long left_operand, long right_operand, const char *op)
{
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
    else
    {
        compiler_error(compiler, "We do not support the operator %s for preprocessor arithmetic", op);
    }

    // Unary operators will be hanlded later...
    // Forgot to handle them.

    return result;
}

int preprocessor_evaluate(struct compile_process *compiler, struct preprocessor_node *root_node);

int preprocessor_evaluate_exp(struct compile_process *compiler, struct preprocessor_node *node)
{
    long left_operand = preprocessor_evaluate(compiler, node->exp.left);
    long right_operand = preprocessor_evaluate(compiler, node->exp.right);
    return preprocessor_arithmetic(compiler, left_operand, right_operand, node->exp.op);
}

int preprocessor_evaluate_unary(struct compile_process *compiler, struct preprocessor_node *node)
{
    int res = 0;
    const char *op = node->unary_node.op;
    struct preprocessor_node *right_operand = node->unary_node.operand_node;
    if (S_EQ(op, "!"))
    {
        res = !preprocessor_evaluate(compiler, right_operand);
    }
    else if (S_EQ(op, "~"))
    {
        res = ~preprocessor_evaluate(compiler, right_operand);
    }
    else
    {
        compiler_error(compiler, "The given operator %s is not supported for unary evaluation in the preprocessor", op);
    }
}

int preprocessor_evaluate_parentheses(struct compile_process *compiler, struct preprocessor_node *node)
{
    return preprocessor_evaluate(compiler, node->parenthesis.exp);
}

int preprocessor_evaluate(struct compile_process *compiler, struct preprocessor_node *root_node)
{
    struct preprocessor_node *current = root_node;
    int result = 0;
    switch (current->type)
    {
    case PREPROCESSOR_NUMBER_NODE:
        result = preprocessor_evaluate_number(current);
        break;

    case PREPROCESSOR_IDENTIFIER_NODE:
        result = preprocessor_evaluate_identifier(compiler, current);
        break;

    case PREPROCESSOR_UNARY_NODE:
        result = preprocessor_evaluate_unary(compiler, current);
        break;

    case PREPROCESSOR_EXPRESSION_NODE:
        result = preprocessor_evaluate_exp(compiler, current);
        break;

    case PREPROCESSOR_PARENTHESES_NODE:
        result = preprocessor_evaluate_parentheses(compiler, current);
        break;
    }
    return result;
}
void preprocessor_handle_if_token(struct compile_process *compiler)
{
    struct vector *node_vector = vector_create(sizeof(struct preprocessor_node *));
    // We will have an expression after the if token that needs to be preprocessed
    struct expressionable *expressionable = expressionable_create(&preprocessor_expressionable_config, compiler->token_vec_original, node_vector);
    expressionable_parse(expressionable);

    struct preprocessor_node *node = expressionable_node_pop(expressionable);
    int result = preprocessor_evaluate(compiler, node);
    preprocessor_read_to_end_if(compiler, result > 0);
}

void preprocessor_handle_identifier_macro_call_argument(struct preprocessor_function_arguments *arguments, struct vector *token_vec)
{
    preprocessor_function_argument_push(arguments, token_vec);
}

struct token *preprocessor_handle_identifier_macro_call_argument_parse(struct compile_process *compiler, struct vector *value_vec, struct preprocessor_function_arguments *arguments, struct token *token)
{
    if (token_is_symbol(token, ')'))
    {
        // We are done handle the call argument
        preprocessor_handle_identifier_macro_call_argument(arguments, value_vec);
        return NULL;
    }

    if (token_is_operator(token, ","))
    {
        preprocessor_handle_identifier_macro_call_argument(arguments, value_vec);
        // Clear the value vector ready for the next argument
        vector_clear(value_vec);
        token = preprocessor_next_token(compiler);
        return token;
    }

    // OK this token is important push it to the value vector
    vector_push(value_vec, token);

    token = preprocessor_next_token(compiler);
    return token;
}

/**
 * Returns the index of the argument or -1 if not found.
 */
int preprocessor_definition_argument_exists(struct preprocessor_definition *definition, const char *name)
{
    vector_set_peek_pointer(definition->standard.arguments, 0);
    int i = 0;
    const char *current = vector_peek(definition->standard.arguments);
    while (current)
    {
        if (S_EQ(current, name))
            return i;

        i++;
        current = vector_peek(definition->standard.arguments);
    }

    return -1;
}

struct preprocessor_function_argument *preprocessor_function_argument_at(struct preprocessor_function_arguments *arguments, int index)
{
    struct preprocessor_function_argument *argument = vector_at(arguments->arguments, index);
    return argument;
}

void preprocessor_function_argument_push_to_vec(struct preprocessor_function_argument *argument, struct vector *vector_out)
{
    vector_set_peek_pointer(argument->tokens, 0);
    struct token *token = vector_peek(argument->tokens);
    while (token)
    {
        vector_push(vector_out, token);
        token = vector_peek(argument->tokens);
    }
}
void preprocessor_macro_function_execute(struct compile_process *compiler, const char *function_name, struct preprocessor_function_arguments *arguments)
{
    struct preprocessor *preprocessor = compiler_preprocessor(compiler);
    struct preprocessor_definition *definition = preprocessor_get_definition(preprocessor, function_name);
    if (!definition)
    {
        FAIL_ERR("Definition was not found");
    }

    if (definition->type != PREPROCESSOR_DEFINITION_MACRO_FUNCTION)
    {
        FAIL_ERR("This definition is not a macro function");
    }

    if (vector_count(definition->standard.arguments) != preprocessor_function_arguments_count(arguments))
    {
        FAIL_ERR("You passed too many arguments to this macro functon, expecting %i arguments");
    }

    // Let's create a special vector for this value as its being injected with function arugments
    struct vector *value_vec_target = vector_create(sizeof(struct token));
    struct vector *definition_token_vec = preprocessor_definition_value(definition);
    vector_set_peek_pointer(definition_token_vec, 0);
    struct token *token = vector_peek(definition_token_vec);
    while (token)
    {
        if (token->type == TOKEN_TYPE_IDENTIFIER)
        {
            // Let's check if we have a function argument in the definition
            // if so this needs replacing
            int argument_index = preprocessor_definition_argument_exists(definition, token->sval);
            if (argument_index != -1)
            {
                // Ok we have an argument, we need to populate the output vector
                // a little differently.
                preprocessor_function_argument_push_to_vec(preprocessor_function_argument_at(arguments, argument_index), value_vec_target);
                token = vector_peek(definition_token_vec);
                continue;
            }
        }
        // Push the token it does not need modfiying
        vector_push(value_vec_target, token);
        token = vector_peek(definition_token_vec);
    }

    // We have our target vector, lets inject it into the output vector.
    preprocessor_token_vec_push_dst(compiler, value_vec_target);
    // vector_free(value_vec_target);
}
struct preprocessor_function_arguments *preprocessor_handle_identifier_macro_call_arguments(struct compile_process *compiler)
{
    // Skip the left bracket
    preprocessor_next_token(compiler);

    // We need room to store these arguments
    struct preprocessor_function_arguments *arguments = preprocessor_function_arguments_create();

    // Ok lets loop through all the values to form a function call argument vector
    struct token *token = preprocessor_next_token(compiler);
    struct vector *value_vec = vector_create(sizeof(struct token));
    while (token)
    {
        token = preprocessor_handle_identifier_macro_call_argument_parse(compiler, value_vec, arguments, token);
    }

    // Free the now unused value vector
    vector_free(value_vec);
    return arguments;
}

void preprocessor_handle_identifier(struct compile_process *compiler, struct token *token)
{
    // We have an identifier, it could represent a variable or a definition
    // lets check if its a definition if so we have to handle it.

    struct preprocessor_definition *definition = preprocessor_get_definition(compiler->preprocessor, token->sval);
    if (!definition)
    {
        // Not our token then it belongs on the destination token vector.
        preprocessor_token_push_dst(compiler, token);
        return;
    }

    // We have a defintion, is this a function call macro
    if (token_is_operator(preprocessor_next_token_no_increment(compiler), "("))
    {
        // Let's create a vector for these arguments
        struct preprocessor_function_arguments *arguments = preprocessor_handle_identifier_macro_call_arguments(compiler);
        const char *function_name = token->sval;
        // Let's execute the macro function
        preprocessor_macro_function_execute(compiler, function_name, arguments);
        // preprocessor_function_arguments_free(arguments);
        return;
    }

    // Normal macro function, then push its entire value stack to the destination stack
    preprocessor_token_vec_push_dst(compiler, preprocessor_definition_value(definition));
}

int preprocessor_handle_hashtag_token(struct compile_process *compiler, struct token *token)
{
    bool is_preprocessed = false;
    struct token *next_token = preprocessor_next_token(compiler);
    if (preprocessor_token_is_define(next_token))
    {
        preprocessor_handle_definition_token(compiler);
        is_preprocessed = true;
    }
    else if (preprocessor_token_is_ifdef(next_token))
    {
        preprocessor_handle_ifdef_token(compiler);
        is_preprocessed = true;
    }
    else if (preprocessor_token_is_if(next_token))
    {
        preprocessor_handle_if_token(compiler);
        is_preprocessed = true;
    }
    else if (preprocessor_token_is_include(next_token))
    {
        preprocessor_handle_include_token(compiler);
        is_preprocessed = true;
    }
    return is_preprocessed;
}

void preprocessor_handle_symbol(struct compile_process *compiler, struct token *token)
{
    int is_preprocessed = false;
    if (token->cval == '#')
    {
        is_preprocessed = preprocessor_handle_hashtag_token(compiler, token);
    }

    // The symbol was not preprocessed so just push it to the destination stack.
    if (!is_preprocessed)
    {
        preprocessor_token_push_dst(compiler, token);
    }
}

void preprocessor_handle_keyword(struct compile_process *compiler, struct token *token)
{
    if (preprocessor_token_is_typedef(token))
    {
        preprocessor_handle_typedef_token(compiler);
    }
    else
    {
        // Not for us to deal with? Then just push it to the stack
        preprocessor_token_push_dst(compiler, token);
    }
}

void preprocessor_handle_token(struct compile_process *compiler, struct token *token)
{
    switch (token->type)
    {
    case TOKEN_TYPE_SYMBOL:
    {
        preprocessor_handle_symbol(compiler, token);
    }
    break;

    case TOKEN_TYPE_KEYWORD:
        preprocessor_handle_keyword(compiler, token);
        break;

    case TOKEN_TYPE_IDENTIFIER:
        preprocessor_handle_identifier(compiler, token);
        break;

    // Ignore new lines that we dont care for.
    case TOKEN_TYPE_NEWLINE:
        break;

    default:
        preprocessor_token_push_dst(compiler, token);
    }
}

void preprocessor_initialize(struct vector *token_vec, struct preprocessor *preprocessor)
{
    memset(preprocessor, 0, sizeof(struct preprocessor));
    preprocessor->definitions = vector_create(sizeof(struct preprocessor_definition));
    preprocessor_create_definitions(preprocessor);
}

struct preprocessor *preprocessor_create(struct compile_process *compiler)
{
    assert(compiler);
    struct preprocessor *preprocessor = calloc(sizeof(struct preprocessor), 1);
    preprocessor_initialize(compiler->token_vec, preprocessor);
    preprocessor->compiler = compiler;
    return preprocessor;
}

int preprocessor_run(struct compile_process *compiler)
{
    vector_set_peek_pointer(compiler->token_vec_original, 0);
    struct token *token = preprocessor_next_token(compiler);
    while (token)
    {
        preprocessor_handle_token(compiler, token);
        token = preprocessor_next_token(compiler);
    }

    // We are done? great we dont need the original token vector anymore, lets swap it out
    // for our one
    return 0;
}
