#include "lexer.h"
#include "misc.h"
#include "stack.h"
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

// Keywords in the C programming language that we must be able to identify.
const char* keywords[] = {"int", "long", "float", "double", "char",  "unsigned", "signed", "void", "union", "struct"};
void dragon_lex_build_next_token(struct lex_process* process);
void dragon_lex_forge_token(struct lex_stream* stream, TOKEN_TYPE type, struct token* __out__ token);

struct token* dragon_lex_new_token(struct lex_process* process)
{
    struct token* token = malloc(sizeof(struct token));
    dragon_lex_build_next_token(process);
    return token;
}

struct lex_stream* dragon_new_stream(const char* input)
{
    struct lex_stream* stream = malloc(sizeof(struct lex_stream));
    stream->input = input;

    #warning "Input file may not have null terminator... Something to be aware of for later.."
    stream->len = strlen(input);

    return stream;
}

void dragon_free_stream(struct lex_stream* stream)
{
    free(stream);
}

struct lex_stream* dragon_stream(struct lex_process* process)
{
    return process->stream;
}

bool dragon_lex_is_next_keyword(struct lex_stream* stream)
{
    int res = 0;
    // Probably best to macro this thing..
    int total_keywords = sizeof(keywords) / sizeof(const char**);
    for (int i = 0; i < total_keywords; i++)
    {
        if (str_matches(stream->input, keywords[i], ' ') == 0)
        {
            res = 1;
            break;
        }
    }

    return res;
}

TOKEN_TYPE dragon_lex_identify_identifier_or_keyword(struct lex_stream* stream)
{
    if (dragon_lex_is_next_keyword(stream))
    {
        return TOKEN_TYPE_KEYWORD;
    }

    return TOKEN_TYPE_IDENTIFIER;
}

TOKEN_TYPE dragon_lex_identify_next_token(struct lex_stream* stream)
{
    TOKEN_TYPE type = -1;
    if (stream->len == 0)
        return type;

    switch(stream->input[0])
    {
        case '\"':
            type = TOKEN_TYPE_STRING;
        break;

        // Not a standard character? Alright then its an identifier or keyword..
        default:
            type = dragon_lex_identify_identifier_or_keyword(stream);
    }
}

char dragon_lex_stream_peek_char(struct lex_stream* stream)
{
    return stream->input[0];
}

void* dragon_lex_stream_data_now(struct lex_stream* stream)
{
    return stream->input;
}

void dragon_lex_increment_stream(struct lex_stream* stream)
{
    // One less input length, one more byte ahead in that buffer!
    stream->len -= 1;
    stream->input +=1;
}

void dragon_lex_forge_token(struct lex_stream* stream, TOKEN_TYPE type, struct token* __out__ token)
{
    token->type = type;
    token->len = 0;
    // A tokens data starts at the current stream position upon creating a new token.
    token->data = dragon_lex_stream_data_now(stream);
}

void dragon_lex_assert_current_char_then_increment(struct lex_stream* stream, char c)
{
    assert(dragon_lex_stream_peek_char(stream) == c);
    dragon_lex_increment_stream(stream);
}

void dragon_lex_build_string_token(struct lex_process* process, struct lex_stream* stream)
{
    struct token token;
    dragon_lex_forge_token(stream, TOKEN_TYPE_STRING, &token);
    
    // Strings must start with double quotes..
    dragon_lex_assert_current_char_then_increment(stream, '"');
    for (size_t i = 0; i < stream->len; i++)
    {
        char c = dragon_lex_stream_peek_char(stream);
        if (c == '"')
        {
            // Another double quote? Looks like we are done...
            // Let us not forget that escaping is also a thing in the "C" programming language
            // a solution will need to be considered at a later date
            break;
        }

        // One more byte has been read, lets lower the input length and increse the input pointer
        dragon_lex_increment_stream(stream);
        
        // The string is one more byte in length
        token.len += i;
    }

    // Great at this point in time we have succesfully lexified the string token
    // We know the string length and have a pointer to the original input data.
    // All that we could need :) 

    // Let's push that token to the stack
    stack_push_back(process->token_stack, &token);
}

void dragon_lex_build_next_token(struct lex_process* process)
{
    struct lex_stream* stream = dragon_stream(process);
    TOKEN_TYPE type = dragon_lex_identify_next_token(stream);
    // Possible ability to register handlers for different types
    // may or may not be a better implementation.. THink about it
    switch(type)
    {
        case TOKEN_TYPE_STRING:
            dragon_lex_build_string_token(process, stream);
        break;

    }
}


int dragon_lex(struct lex_process* process, const char* input)
{
    // Create a new dragon stream that will handle the input.
    process->stream = dragon_new_stream(input);
    process->token_stack = stack_create(sizeof(struct token));
    dragon_lex_build_next_token(process);

    return 0;
}


void dragon_lex_free(struct lex_process* process)
{

}

