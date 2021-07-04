#include "compiler.h"
#include "helpers/vector.h"

void preprocessor_handle_token(struct compile_process *compiler, struct token *token);

struct preprocessor *compiler_preprocessor(struct compile_process *compiler)
{
    return compiler->preprocessor;
}
static struct token *preprocessor_next_token(struct compile_process *compiler)
{
    return vector_peek(compiler->token_vec_original);
}

static struct token *preprocessor_next_token_no_increment(struct compile_process *compiler)
{
    return vector_peek_no_increment(compiler->token_vec_original);
}

static void preprocessor_token_push_dst(struct compile_process *compiler, struct token *token)
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

static bool preprocessor_token_is_preprocessor_keyword(struct token *token)
{
    return token->type == TOKEN_TYPE_IDENTIFIER &&
           (S_EQ(token->sval, "define") || S_EQ(token->sval, "if") || S_EQ(token->sval, "ifdef") || S_EQ(token->sval, "ifndef") || S_EQ(token->sval, "endif"));
}

static bool preprocessor_token_is_define(struct token *token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }

    return (S_EQ(token->sval, "define"));
}

static bool preprocessor_token_is_ifdef(struct token *token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }

    return (S_EQ(token->sval, "ifdef"));
}

/**
 * Searches for a hashtag symbol along with the given identifier.
 * If found the hashtag token and identifier token are both popped from the stack
 * Only the target identifier token representing "str" is returned.
 * 
 * NO match return null.
 */
static struct token *preprocessor_hashtag_and_identifier(struct compile_process *compiler, const char *str)
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

static bool preprocessor_token_is_definition_identifier(struct compile_process *compiler, struct token *token)
{
    if (token->type != TOKEN_TYPE_IDENTIFIER)
        return false;

    if (preprocessor_get_definition(compiler->preprocessor, token->sval))
    {
        return true;
    }

    return false;
}

struct preprocessor_definition *preprocessor_definition_create(const char *name, struct vector *value_vec)
{
    struct preprocessor_definition *definition = calloc(sizeof(struct preprocessor_definition), 1);
    definition->name = name;
    definition->value = value_vec;
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
static void preprocessor_handle_definition_token(struct compile_process *compiler)
{
    struct token *name_token = preprocessor_next_token(compiler);
    // Value can be composed of many tokens
    struct vector *value_token_vec = vector_create(sizeof(struct token));
    preprocessor_multi_value_insert_to_vector(compiler, value_token_vec);

    struct preprocessor *preprocessor = compiler->preprocessor;
    struct preprocessor_definition *definition = preprocessor_definition_create(name_token->sval, value_token_vec);
    vector_push(preprocessor->definitions, &definition);
}

static void preprocessor_handle_ifdef_token(struct compile_process *compiler)
{
    struct token *condition_token = preprocessor_next_token(compiler);
    if (!condition_token)
    {
        FAIL_ERR("No condition token provided????. WIll replace later with proper error system");
    }

    // Let's see if we have a definition of the condition token name
    struct preprocessor_definition *definition = preprocessor_get_definition(compiler_preprocessor(compiler), condition_token->sval);

    // We have this definition we can proceed with the rest of the body, until
    // an #endif is discovered
    while (preprocessor_next_token_no_increment(compiler) 
            && !preprocessor_hashtag_and_identifier(compiler, "endif"))
    {
        if (definition)
        {
            preprocessor_handle_token(compiler, preprocessor_next_token(compiler));
            continue;
        }

        // Skip the unexpected token
        preprocessor_next_token(compiler);
    }
}

static void preprocessor_handle_identifier(struct compile_process *compiler, struct token *token)
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

    // We have a defintion, lets push the value of the definition to the new token vector
    preprocessor_token_vec_push_dst(compiler, definition->value);
}

static int preprocessor_handle_hashtag_token(struct compile_process *compiler, struct token *token)
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

    return is_preprocessed;
}

static void preprocessor_handle_symbol(struct compile_process *compiler, struct token *token)
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
void preprocessor_handle_token(struct compile_process *compiler, struct token *token)
{
    switch (token->type)
    {
    case TOKEN_TYPE_SYMBOL:
    {
        preprocessor_handle_symbol(compiler, token);
    }
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

void preprocessor_initialize(struct preprocessor *preprocessor)
{
    memset(preprocessor, 0, sizeof(struct preprocessor));
    preprocessor->definitions = vector_create(sizeof(struct preprocessor_definition));
}

struct preprocessor *preprocessor_create()
{
    struct preprocessor *preprocessor = calloc(sizeof(struct preprocessor), 1);
    preprocessor_initialize(preprocessor);
    return preprocessor;
}

int preprocessor_run(struct compile_process *compiler, const char *file)
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
