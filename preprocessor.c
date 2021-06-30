#include "compiler.h"
#include "helpers/vector.h"


static struct token* preprocessor_next_token(struct compile_process* compiler)
{
    return vector_peek(compiler->token_vec);
}

static bool preprocessor_token_is_preprocessor_keyword(struct token* token)
{
    return token->type == TOKEN_TYPE_IDENTIFIER && 
        (S_EQ(token->sval, "define") || S_EQ(token->sval, "ifdef") || S_EQ(token->sval, "ifndef") || S_EQ(token->sval, "endif"));
}

static bool preprocessor_token_is_define(struct token* token)
{
    if (!preprocessor_token_is_preprocessor_keyword(token))
    {
        return false;
    }

    return (S_EQ(token->sval, "define"));
}


struct preprocessor_definition* preprocessor_get_definition(struct preprocessor* preprocessor, const char* name)
{
    vector_set_peek_pointer(preprocessor->definitions, 0);
    struct preprocessor_definition* definition = vector_peek(preprocessor->definitions);
    while(definition)
    {
        if (S_EQ(definition->name, name))
        {
            break;
        }
        definition = vector_peek(preprocessor->definitions);
    }

    return definition;
}

static bool preprocessor_token_is_definition_identifier(struct compile_process* compiler, struct token* token)
{
    if (token->type != TOKEN_TYPE_IDENTIFIER)
        return false;

    if (preprocessor_get_definition(compiler->preprocessor, token->sval))
    {
        return true;
    }

    return false;
}


struct preprocessor_definition* preprocessor_definition_create(const char* name, struct vector* value_vec)
{
    struct preprocessor_definition* definition = calloc(sizeof(struct preprocessor_definition), 1);
    definition->name = name;
    definition->value = value_vec;
    return definition;
}

static void preprocessor_handle_definition_token(struct compile_process* compiler)
{
    struct token* name_token = preprocessor_next_token(compiler);
    // Value can be composed of many tokens
    struct vector* value_token_vec = vector_create(sizeof(struct token));

    struct token* value_token = preprocessor_next_token(compiler);
    while(value_token)
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

    struct preprocessor* preprocessor = compiler->preprocessor;
    struct preprocessor_definition* definition = preprocessor_definition_create(name_token->sval, value_token_vec);
    vector_push(preprocessor->definitions, &definition);
}

static void preprocessor_handle_identifier(struct compile_process* compiler, struct token* token)
{
    // We have an identifier, it could represent a variable or a definition
    // lets check if its a definition if so we have to handle it.

    struct preprocessor_definition* definition = preprocessor_get_definition(compiler->preprocessor, token->sval);
    if(!definition)
    {
        return;
    }

    // OK its a valid definition lets handle it.
   //vector_remove_token(token);
}

static void preprocessor_handle_hashtag_token(struct compile_process* compiler, struct token* token)
{
    struct token* next_token = preprocessor_next_token(compiler);
    if (preprocessor_token_is_define(next_token))
    {
        preprocessor_handle_definition_token(compiler);
    }
    else if(preprocessor_token_is_definition_identifier(compiler, token))
    {
        preprocessor_handle_identifier(compiler, token);
    }
}

static void preprocessor_handle_token(struct compile_process* compiler, struct token* token)
{
    switch(token->type)
    {
        case TOKEN_TYPE_SYMBOL:
        {
            if(token->cval == '#')
            {
                preprocessor_handle_hashtag_token(compiler, token);
            }
        }
        break;
    }
}


void preprocessor_initialize(struct preprocessor* preprocessor)
{
    memset(preprocessor, 0, sizeof(struct preprocessor));
    preprocessor->definitions = vector_create(sizeof(struct preprocessor_definition));
}

struct preprocessor* preprocessor_create()
{
    struct preprocessor* preprocessor = calloc(sizeof(struct preprocessor), 1);
    preprocessor_initialize(preprocessor);
    return preprocessor;
}

int preprocessor_run(struct compile_process* compiler, const char* file)
{
    vector_set_peek_pointer(compiler->token_vec, 0);
    struct token* token = preprocessor_next_token(compiler);
    while(token)
    {
        preprocessor_handle_token(compiler, token);
        token = preprocessor_next_token(compiler);
    }
    return 0;
}

