#include "compiler.h"
#include "helpers/buffer.h"
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <stdarg.h>
#include <memory.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

/**
 * Reads from the file stream and writes to the buffer if the character read
 * matches the provided expression.
 * 
 * I.e to read only number characters '0'-'9' from the stream and stop when theirs no number you would do
 * struct buffer* buffer = buffer_create();
 * char c = peekc();
 * LEX_GETC_IF(buffer, c, c >= '0' && c <= '9');
 */
#define LEX_GETC_IF(buffer, c, EXP) \
    for (c = peekc(); EXP; c = peekc()) \
    {                                          \
        buffer_write(buffer, c);            \
        nextc();                             \
    }\
    
static struct token tmp_token;
static struct compile_process *current_process;

void error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    compiler_error(current_process, fmt, args);
    va_end(args);
}

static char nextc()
{
    return compile_process_next_char(current_process);
}

static char peekc()
{
    return compile_process_peek_char(current_process);
}

static void pushc(char c)
{
    return compile_process_push_char(current_process, c);
}

bool is_keyword(const char* str)
{
    return 
           S_EQ(str, "unsigned") ||
           S_EQ(str, "signed") ||
           S_EQ(str, "char") ||
           S_EQ(str, "int") ||
           S_EQ(str, "double") ||
           S_EQ(str, "long") ||
           S_EQ(str, "void") ||
           S_EQ(str, "struct") ||
           S_EQ(str, "union") || 
           S_EQ(str, "static");
}

bool keyword_is_datatype(const char* str)
{
    return 
           S_EQ(str, "char") ||
           S_EQ(str, "int") ||
           S_EQ(str, "float") ||
           S_EQ(str, "double") ||
           S_EQ(str, "long");

}

static bool is_single_operator(char op)
{
    return op == '+' ||
           op == '-' ||
           op == '/' ||
           op == '*' ||
           op == '=' ||
           op == '>' ||
           op == '<' ||
           op == '|' ||
           op == '&' ||
           op == '^' ||
           op == '%' ||
           op == '~' ||
           op == '!';
}

static bool op_valid(const char* op)
{
    return 
           S_EQ(op, "+") ||
           S_EQ(op, "-") ||
           S_EQ(op, "*") ||
           S_EQ(op, "/") ||
           S_EQ(op, "!") ||
           S_EQ(op , "^") ||
           S_EQ(op, "+=") ||
           S_EQ(op, "-=") ||
           S_EQ(op, "*=") ||
           S_EQ(op, "/=") ||
           S_EQ(op, ">>") ||
           S_EQ(op, "<<") ||
           S_EQ(op, ">=") ||
           S_EQ(op, "<=") ||
           S_EQ(op, ">") ||
           S_EQ(op, "<") ||
           S_EQ(op, "||") ||
           S_EQ(op, "&&") ||
           S_EQ(op, "|") ||
           S_EQ(op, "&") ||
           S_EQ(op, "++") ||
           S_EQ(op, "--")||
           S_EQ(op, "=") ||
           S_EQ(op, "/=") ||
           S_EQ(op, "*=") ||
           S_EQ(op, "^=") || 
           S_EQ(op, "==") || 
           S_EQ(op, "!=") ||
           S_EQ(op, "->") ||
           S_EQ(op, "**");
}

static struct compile_process *compiler_process()
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

static struct token *token_create(struct token *_token)
{
    // Shared temp token for all tokens, only one token should be created
    // per build token, to avoid memory leaks.
    // Once its pushed to the token stack then its safe to call this function again
    memcpy(&tmp_token, _token, sizeof(tmp_token));
    tmp_token.pos = compiler_file_position();
    return &tmp_token;
}

static struct token *token_make_string()
{
    struct buffer *buf = buffer_create();
    // We expect strings to start with double quotes.
    assert_next_char('"');
    char c = nextc();

    for (; c != '"' && c != EOF; c = nextc())
    {
        if (c == '\\')
        {
            // Theirs an escape about to happen...
            // Ignore for now, but we need to handle this in future
            nextc();
        }
        buffer_write(buf, c);
    }
    // Null terminator.
    buffer_write(buf, 0x00);
    return token_create(&(struct token){TOKEN_TYPE_STRING, .sval = buffer_ptr(buf)});
}

static void *token_ignore_newline()
{
    assert_next_char('\n');
}

static const char *read_op()
{
    char op = nextc();
    struct buffer *buffer = buffer_create();
    buffer_write(buffer, op);
    op = peekc();
    if (is_single_operator(op))
    {
        buffer_write(buffer, op);
        nextc();
    }

    // Null terminator.
    buffer_write(buffer, 0x00);

    const char *buf_ptr = buffer_ptr(buffer);
    if (!op_valid(buf_ptr))
    {
        error("Invalid operator: %s\n", buf_ptr);
    }
    return buf_ptr;
}

static struct token *token_make_operator_for_value(const char* val)
{
    return token_create(&(struct token){TOKEN_TYPE_OPERATOR, .sval=val});
}

static struct token *token_make_operator()
{
    return token_create(&(struct token){TOKEN_TYPE_OPERATOR, .sval=read_op()});
}

const char* read_number_str()
{
    const char* num = NULL;
    struct buffer* buffer = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buffer, c, c >= '0' && c <= '9');

    // Null terminator.
    buffer_write(buffer, 0x00);

    return buffer_ptr(buffer);
}

unsigned long long read_number()
{
    const char* s = read_number_str();
    return atoll(s);
}

static struct token* token_make_number()
{
    return token_create(&(struct token){TOKEN_TYPE_NUMBER, .llnum=read_number()});
}

static struct token* token_make_identifier_or_keyword()
{
    struct buffer* buffer = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buffer, c, (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
    // Null terminator.
    buffer_write(buffer, 0x00);

    if (is_keyword(buffer_ptr(buffer)))
    {
        return token_create(&(struct token){TOKEN_TYPE_KEYWORD, .sval=buffer_ptr(buffer)});
    }
    
    return token_create(&(struct token){TOKEN_TYPE_IDENTIFIER, .sval=buffer_ptr(buffer)});
}

static struct token* token_make_symbol()
{
    char c = nextc();
    return token_create(&(struct token){TOKEN_TYPE_SYMBOL, .cval=c});
}
static struct token* read_token_special()
{
    char c = peekc();
    if (isalpha(c) || c == '_')
    {
        return token_make_identifier_or_keyword();
    }

    return NULL;

}

/**
 * This function expects for the input stream to be already on the comment line
 * For example: Hello world. Rather than //Hello world.
 */
static struct token* token_make_one_line_comment()
{
    // We must read the whole comment, we know when we are done as we will
    // have a new line terminator

    struct buffer* buffer = buffer_create();
    char c = 0;
    LEX_GETC_IF(buffer, c,  c != '\n' && c != EOF);
    return token_create(&(struct token){TOKEN_TYPE_COMMENT, .sval=buffer_ptr(buffer)});
}


/**
 * Note: this function expects the file pointer to be pointing at the comment body.
 * For example we expect the input stream to be: "Hello world" rather than "/*hello world"
 */
static struct token* token_make_multiline_comment()
{
    struct buffer* buffer = buffer_create();
    char c = 0;
    while(1)
    {
        LEX_GETC_IF(buffer, c, c != '*' && c != EOF);
        if (c == EOF)
        {
            // End of file.. but this comment is not closed.
            // Something bad here, it should not be allowed
            error("EOF reached whilst in a multi-line comment. The comment was not terminated! \"%s\" \n", buffer_ptr(buffer));
        }
        if (c == '*')
        {
            // Skip the *
            nextc();

            // We must check to see if this is truly the end of this multi-line comment
            // if it is then we should see a forward slash as the next character.
            if (peekc() == '/')
            {
                // Da, it is
                nextc();
                break;
            }
        }
    }

    return token_create(&(struct token){TOKEN_TYPE_COMMENT, .sval=buffer_ptr(buffer)});
}
static struct token* handle_comment()
{
    char c = peekc();
    if (c == '/')
    {
        nextc();
        if (peekc() == '/')
        {
            nextc();
            // This is a comment token.
            return token_make_one_line_comment();
        }
        else if(peekc() == '*')
        {
            nextc();
            return token_make_multiline_comment();
        }

        // This is not a comment it must just be a normal division operator
        // Let's push back to the input stream the division symbol
        // so that it can be proceesed correctly by the operator token function
        pushc('/');

        return token_make_operator();
    }

    return NULL;
}

static struct token* token_make_quote()
{
    assert_next_char('\'');
    char c = nextc();
    if (c == '\\')
    {
        // We have an escape here.. For now we shall ignore it. This must be handled
        // in the future!
        c = nextc();
    }

    // Characters are basically just small numbers. Treat it as such.
    return token_create(&(struct token){TOKEN_TYPE_NUMBER, .cval=c});
}

static struct token *read_next_token()
{
    struct token *token = NULL;
    char c = peekc();

    token = handle_comment();
    if (token)
    {
        // This was a comment as we now have a comment token
        return token;
    }

    switch (c)
    {

    NUMERIC_CASE:
        token = token_make_number();
        break;

    OPERATOR_CASE_EXCLUDING_DIVISON:
        token = token_make_operator();
        break;
    SYMBOL_CASE:
        token = token_make_symbol();
        break;

    case '\'':
        token = token_make_quote();
    break;
    case '"':
        token = token_make_string();
        break;

    // Spaces, new lines and tabs are ignored..
    case '\n':
    case ' ':
    case '\t':
        nextc();
        token = read_next_token();
        break;
    
    case EOF:
        // aha we are done!
        break;
    default:
        token = read_token_special();
        if (!token)
        {
            compiler_error(current_process, "Invalid token provided. Character %c\n", c);
        }
        break;
    }

    return token;
}

int lex(struct compile_process *process)
{
    current_process = process;

    struct token *token = read_next_token();
    while (token)
    {
        vector_push(process->token_vec, token);
        token = read_next_token();
    }

    return LEXICAL_ANALYSIS_ALL_OK;
}
