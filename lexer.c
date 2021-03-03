#include "lexer.h"
#include <memory.h>
#include <stdlib.h>
#include <string.h>


// Keywords in the C programming language that we must be able to identify.
const char** keywords[] = {"int", "long", "float", "double", "char",  "unsigned", "signed", "void", "union", "struct"};

struct token* dragon_lex_new_token(struct lex_process* process)
{
    struct token* token = malloc(sizeof(struct token));
    dragon_lex_build_next_token(token, process);
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

struct dragon_stream* dragon_stream(struct lex_process* process)
{
    return process->stream;
}

struct lex_stack_entry* dragon_lex_get_current_stack(struct lex_process* process)
{
    return process->stack.current;
}

LEX_FLAG_STATE dragon_lex_get_flag(struct lex_process* process, LEX_FLAG flag)
{
    struct lex_stack_entry* stack = dragon_lex_get_current_stack(process);
    return (stack->flags & flag) ? LEX_FLAG_STATE_ON : LEX_FLAG_STATE_OFF;
}


LEX_FLAG_STATE dragon_lex_set_flag(struct lex_process* process, LEX_FLAG flag)
{
    struct lex_stack_entry* stack = dragon_lex_get_current_stack(process);
    return stack->flags |= flag;
}

LEX_FLAG_STATE dragon_lex_unset_flag(struct lex_process* process, LEX_FLAG flag)
{
    struct lex_stack_entry* stack = dragon_lex_get_current_stack(process);
    return stack->flags &= ~flag;
}


TOKEN_TYPE dragon_lex_identify_identifier_or_keyword(struct lex_stream* stream)
{
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

void dragon_lex_increment_stream(const char** input, size_t* input_len)
{
    // One less input length, one more byte ahead in that buffer!
    *input_len -= 1;
    *input +=1;
}

void dragon_lex_build_string_token(struct token* __out__ token, const char** input, size_t* input_len)
{
    for (int i = 0; i < *input_len; i++)
    {

        // One more byte has been read, lets lower the input length and increse the input pointer
        dragon_lex_increment_stream(input, input_len);
    }
}

void dragon_lex_build_next_token(struct token* __out__ token, struct dragon_process* process)
{
    struct lex_stream* stream = dragon_stream(process);
    TOKEN_TYPE type = dragon_lex_identify_next_token(*input, *input_len);
    // Possible ability to register handlers for different types
    // may or may not be a better implementation.. THink about it
    switch(type)
    {
        case TOKEN_TYPE_STRING:
            dragon_lex_build_string_token(token, input, input_len);
        break;

        default:
            // Lexer error... Handle this..
    }
}


int dragon_lex(struct lex_process* process, const char* input)
{
    // Create a new dragon stream that will handle the input.
    process->stream = dragon_new_stream(input);
    // Create the root token
    process->token = dragon_lex_new_token(input);
    struct token* token = process->token;
    const char* tmp_input = input;
    // Get new tokens until no new tokens are vailable
    while(token != NULL)
    {
        // tmp_input and tmp_len will be adjusted by this function
        token = dragon_lex_new_token(&tmp_input, &tmp_len);
    }
    
    return root_token;
}


void dragon_lex_free(struct lex_process* process)
{

}
