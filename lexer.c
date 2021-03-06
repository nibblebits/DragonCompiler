#include "compiler.h"
#include "helpers/buffer.h"
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <stdarg.h>
#include <memory.h>

static struct token tmp_token;
static struct compile_process* current_process;
static char nextc()
{
    return compile_process_next_char(current_process);
}

static struct compile_process* compiler_process()
{
    return current_process;
}

static struct pos compiler_file_position()
{
    return current_process->pos;
}

static char assert_next_char(char expected_char)
{
    char c = nextc();
    assert(c == expected_char);
    return c;
}

static struct token* token_create(struct token* _token)
{
    // Shared temp token for all tokens, only one token should be created
    // per build token, to avoid memory leaks.
    // Once its pushed to the token stack then its safe to call this function again
    memcpy(&tmp_token, _token, sizeof(tmp_token));
    tmp_token.pos = compiler_file_position();
    return &tmp_token;
}

static struct token* token_make_string()
{
    struct buffer* buf = buffer_create();
    char c = nextc();
    for ( ; c != '"' && c != EOF; c = nextc())
    {
        buffer_write(buf, c);
    }
    // Null terminator.
    buffer_write(buf, 0x00);
    return token_create(&(struct token){TOKEN_TYPE_STRING, .sval=buffer_ptr(buf)});
}

static struct token* read_next_token()
{
    struct token* token = NULL;
    char c = nextc();
    switch(c)
    {
        case '"':
            token = token_make_string();
        break;

        case EOF:
            // aha we are done!
        break;
        default:
            printf("Invalid token provided. Character %c\n", c);
    }

    return token;
}

int lex(struct compile_process* process)
{
    current_process = process;

    struct token* token = read_next_token();
    while(token)
    {
        vector_push(process->token_vec, token);
        token = read_next_token();
    }

    return LEXICAL_ANALYSIS_ALL_OK;
}

