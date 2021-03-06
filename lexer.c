#include "compiler.h"
#include "helpers/buffer.h"
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <stdarg.h>

static char error_buf[2056];
static char *pos_string(struct pos *p) {
     sprintf(error_buf, "%s:%d:%d", "(unknown)", p->line, p->col);
     return error_buf;
}

#define errorp(p, ...) errorf(__FILE__ ":" STR(__LINE__), pos_string(&p), __VA_ARGS__)
#define warnp(p, ...)  warnf(__FILE__ ":" STR(__LINE__), pos_string(&p), __VA_ARGS__)

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

static struct token* token_create(struct token* token_tmp)
{
    struct token* token = malloc(sizeof(struct token));
    *token = *token_tmp;
    token->pos = compiler_file_position();
    return token;
}

static struct token* token_make_string()
{
    struct buffer* buf = buffer_create();
    char c = nextc();
    for ( ; c != '"' && c != EOF; c = nextc())
    {
        buffer_write(buf, c);
    }
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
    }

    return token;
}

int lex(struct compile_process* process)
{
    current_process = process;

    struct token* token = read_next_token();
    while(token)
    {
        token = read_next_token();
    }

    return LEXICAL_ANALYSIS_ALL_OK;
}

