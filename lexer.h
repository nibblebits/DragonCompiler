#ifndef LEXER_H
#define LEXER_H
#include <stdint.h>
#include <stddef.h>
#include "misc.h"

enum
{
    TOKEN_TYPE_IDENTIFIER,
    TOKEN_TYPE_KEYWORD,
    TOKEN_TYPE_STRING,
    
};

enum
{
    LEX_FLAG_IN_STRING = 0b0000000000000001,
    LEX_FLAG_KEYWORD = 0b0000000000000010,
};

enum
{
    LEX_FLAG_STATE_OFF,
    LEX_FLAG_STATE_ON,
};

typedef int TOKEN_TYPE;
typedef unsigned int LEX_FLAG;
typedef unsigned int LEX_FLAG_STATE;

struct lex_stack_entry
{
    LEX_FLAG flags;
};


/**
 * Represents a lexical analysis stream that can be manipulated
 */
struct lex_stream
{
    // The current input for the next byte in the stream
    const char* input;
    // The length that is left for this stream until its read.
    size_t len;
};

struct lex_process
{
    // A stack of tokens performed by lexical analysis.
    struct stack* token_stack;

    // The input stream for this lexcial analysis.
    struct lex_stream* stream;
};

struct token
{
    TOKEN_TYPE type;
    const char* data;
    size_t len;
};


/**
 * Converts the input into a series of tokens. Lexcial analysis is performed
 * \param process The process to be used for this lexcial analysis.
 */
int dragon_lex(struct lex_process* process, const char* input);


#endif