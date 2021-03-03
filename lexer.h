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
 * States can sometimes stack, because of this we need to have a lexcial analysis
 * stack that we can push and pop from
 */
struct lex_stack
{   
    struct lex_stack_entry* current;
    struct lex_stack_entry* prev;
    struct lex_stack_entry* next;
};

/**
 * Represents a lexical analysis stream that can be manipulated
 */
struct lex_stream
{
    const char* input;
    size_t* len;
}

struct lex_process
{
    // Current lex stack
    struct lex_stack stack;

    // The root token of the lexcial process.
    struct token* token;

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

/**
 * Gets the next token from the given input
 * 
 * \param input The current input pointer, it will be adjusted after this function is executed
 * \param input_len The length of whats left of the input. This is adjusted when new token has been parsed
 */
struct token* dragon_lex_new_token(const char** input, size_t* input_len);

void dragon_lex_free(struct lex_process* process);

/**
 * Returns the given flag state for the current lexer stack.
 * 
 * If the flag is set then LEX_FLAG_STATE_ON is returned. Otherwise LEG_FLAG_STATE_OFF
 */
LEX_FLAG_STATE dragon_lex_get_flag(LEX_FLAG flag);

struct lex_stack_entry* dragon_lex_get_current_stack(struct lex_process* process);

/**
 * Creates a new stream for the given input
 */
struct lex_stream* dragon_new_stream(const char* input);
void dragon_free_stream(struct lex_stream* stream);

struct dragon_stream* dragon_stream(struct lex_process* process);


#endif