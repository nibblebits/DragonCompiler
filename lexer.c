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
#define LEX_GETC_IF(buffer, c, EXP)     \
    for (c = peekc(); EXP; c = peekc()) \
    {                                   \
        buffer_write(buffer, c);        \
        nextc();                        \
    }

static struct token tmp_token;
static struct lex_process *lex_process;

const char *read_number_str();
unsigned long long read_number();
static struct token *lexer_last_token();

void error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    compiler_error(lex_process->compiler, fmt, args);
    va_end(args);
}

void lex_new_expression()
{
    lex_process->current_expression_count++;
    if (lex_process->current_expression_count == 1)
    {
        // This is the first expression in series? Then we must initialize the buffer
        lex_process->parentheses_buffer = buffer_create();
    }

    // If the last token is a comma or identifier then this is a new argument string buffer.
    struct token* last_token = lexer_last_token();
    if (last_token && (last_token->type == TOKEN_TYPE_IDENTIFIER || token_is_operator(last_token, ",")))
    {
        // The last token was an identifier.. This means this is some sort of function call i.e
        //ABC(
        // Where we are at the left bracket.
        lex_process->argument_string_buffer = buffer_create();
    }
}

void lex_finish_expression()
{
    lex_process->current_expression_count--;
    if (lex_process->current_expression_count < 0)
    {
        error("You closed an expression before opening one");
    }
    
}

bool lex_is_in_expression()
{
    return lex_process->current_expression_count > 0;
}

char lexer_string_buffer_next_char(struct lex_process *process)
{
    struct buffer *buf = lex_process_private(process);
    return buffer_read(buf);
}
char lexer_string_buffer_peek_char(struct lex_process *process)
{
    struct buffer *buf = lex_process_private(process);
    return buffer_peek(buf);
}

void lexer_string_buffer_push_char(struct lex_process *process, char c)
{
    struct buffer *buf = lex_process_private(process);
    buffer_write(buf, c);
}

struct lex_process_functions lexer_string_buffer_functions = {
    .next_char = lexer_string_buffer_next_char,
    .peek_char = lexer_string_buffer_peek_char,
    .push_char = lexer_string_buffer_push_char};

static char nextc()
{
    char c = lex_process->function->next_char(lex_process);
    if (lex_is_in_expression())
    {
        buffer_write(lex_process->parentheses_buffer, c);
        if (lex_process->argument_string_buffer)
        {
            buffer_write(lex_process->argument_string_buffer, c);
        }
    }
    lex_process->pos.col++;
    if (c == '\n')
    {
        lex_process->pos.col = 0;
        lex_process->pos.line++;
    }
    return c;
}

static char peekc()
{
    return lex_process->function->peek_char(lex_process);
}

static void pushc(char c)
{
    return lex_process->function->push_char(lex_process, c);
}

bool is_keyword(const char *str)
{
    return S_EQ(str, "unsigned") ||
           S_EQ(str, "signed") ||
           S_EQ(str, "char") ||
           S_EQ(str, "short") ||
           S_EQ(str, "int") ||
           S_EQ(str, "float") ||
           S_EQ(str, "double") ||
           S_EQ(str, "long") ||
           S_EQ(str, "void") ||
           S_EQ(str, "struct") ||
           S_EQ(str, "union") ||
           S_EQ(str, "static") ||
           S_EQ(str, "__ignore_typecheck__") ||
           S_EQ(str, "return") ||
           S_EQ(str, "include") ||
           S_EQ(str, "sizeof") ||
           S_EQ(str, "if") ||
           S_EQ(str, "else") ||
           S_EQ(str, "while") ||
           S_EQ(str, "for") ||
           S_EQ(str, "do") ||
           S_EQ(str, "break") ||
           S_EQ(str, "continue") ||
           S_EQ(str, "switch") ||
           S_EQ(str, "case") ||
           S_EQ(str, "default") ||
           S_EQ(str, "goto") ||
           S_EQ(str, "typedef") ||
           S_EQ(str, "const") ||
           S_EQ(str, "extern") ||
           S_EQ(str, "restrict");
}

bool keyword_is_datatype(const char *str)
{
    return S_EQ(str, "void") ||
           S_EQ(str, "char") ||
           S_EQ(str, "int") ||
           S_EQ(str, "short") ||
           S_EQ(str, "float") ||
           S_EQ(str, "double") ||
           S_EQ(str, "long") ||
           S_EQ(str, "struct") ||
           S_EQ(str, "union");
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
           op == '!' ||
           op == '(' ||
           op == '[' ||
           op == ',' ||
           op == '.' ||
           op == '~' ||
           op == '?';
}

static bool op_treated_as_one(char op)
{
    return op == '(' || op == '[' || op == ',' || op == '.' || op == '*' || op == '?';
}

static bool op_valid(const char *op)
{
    return S_EQ(op, "+") ||
           S_EQ(op, "-") ||
           S_EQ(op, "*") ||
           S_EQ(op, "/") ||
           S_EQ(op, "!") ||
           S_EQ(op, "^") ||
           S_EQ(op, "+=") ||
           S_EQ(op, "-=") ||
           S_EQ(op, "*=") ||
           S_EQ(op, ">>=") ||
           S_EQ(op, "<<=") ||
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
           S_EQ(op, "--") ||
           S_EQ(op, "=") ||
           S_EQ(op, "*=") ||
           S_EQ(op, "^=") ||
           S_EQ(op, "==") ||
           S_EQ(op, "!=") ||
           S_EQ(op, "->") ||
           S_EQ(op, "**") ||
           S_EQ(op, "(") ||
           S_EQ(op, "[") ||
           S_EQ(op, ",") ||
           S_EQ(op, ".") ||
           S_EQ(op, "...") ||
           S_EQ(op, "~") ||
           S_EQ(op, "?") ||
           S_EQ(op, "%");
}

static struct lex_process *lex_get_process()
{
    return lex_process;
}

static struct pos lex_file_position()
{
    return lex_process->pos;
}

static struct token *lexer_last_token()
{
    return vector_back_or_null(lex_process->token_vec);
}

void lexer_pop_token()
{
    vector_pop(lex_process->token_vec);
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
    tmp_token.pos = lex_file_position();
    if (lex_is_in_expression())
    {
        assert(lex_process->parentheses_buffer);
        tmp_token.between_brackets = buffer_ptr(lex_process->parentheses_buffer);
        
        if (lex_process->argument_string_buffer)
        {
            tmp_token.between_arguments = buffer_ptr(lex_process->argument_string_buffer);
        }
    }
    return &tmp_token;
}

static void lex_handle_escape_number(struct buffer *buf)
{
    long long number = read_number();
    if (number > 255)
    {
        error("Characters must be betwene 0-255 wide chars are not yet supported");
    }
    buffer_write(buf, number);
}

char lex_get_escaped_char(char c)
{

    char co = 0x00;
    switch (c)
    {
    case 'n':
        // New line escape?
        co = '\n';
        break;
    case '\\':
        co = '\\';

    case 't':
        co = '\t';
    break;

    case '\'':
        co = '\'';
    break;

    default:
        error("Unknown escape token %c\n", c);
    }
    return co;
}
static void lex_handle_escape(struct buffer *buf)
{
    char c = peekc();
    if (isdigit(c))
    {
        // We have a number?
        lex_handle_escape_number(buf);
        return;
    }

    char co = lex_get_escaped_char(c);

    buffer_write(buf, co);
    // Pop off the char
    nextc();
}
static struct token *token_make_string(char start_delim, char end_delim)
{
    struct buffer *buf = buffer_create();
    // We expect strings to start with double quotes.
    assert_next_char(start_delim);
    char c = nextc();

    for (; c != end_delim && c != EOF; c = nextc())
    {
        if (c == '\\')
        {
            lex_handle_escape(buf);
            continue;
        }
        buffer_write(buf, c);
    }
    // Null terminator.
    buffer_write(buf, 0x00);
    return token_create(&(struct token){TOKEN_TYPE_STRING, .sval = buffer_ptr(buf)});
}

static struct token *token_make_newline()
{
    nextc();
    return token_create(&(struct token){TOKEN_TYPE_NEWLINE});
}

void lexer_validate_binary_string(const char *number_str)
{
    size_t len = strlen(number_str);
    for (int i = 0; i < len; i++)
    {
        if (number_str[i] != '0' && number_str[i] != '1')
        {
            compiler_error(lex_process->compiler, "Expecting a binary string but we found a character that is not a hex value %c\n", number_str[i]);
        }
    }
}

/**
 * Flushes the buffer back to the character stream, except the first operator.
 * Ignores the null terminator
 */
void read_op_flush_back_keep_first(struct buffer *buffer)
{
    const char *data = buffer_ptr(buffer);
    int len = buffer->len;
    for (int i = len - 1; i >= 1; i--)
    {
        if (data[i] == 0x00)
            continue;
        pushc(data[i]);
    }
}

static const char *read_op()
{
    bool single_operator = true;
    char op = nextc();
    struct buffer *buffer = buffer_create();
    buffer_write(buffer, op);

    if (op == '*' && peekc() == '=')
    {
        // This is *= so even though its treated as one operator, in this case its two.
        buffer_write(buffer, peekc());
        // Skip the "=" we just wrote it.
        nextc();
    }
    else if (!op_treated_as_one(op))
    {
        for (int i = 0; i < 2; i++)
        {
            op = peekc();
            if (is_single_operator(op))
            {
                buffer_write(buffer, op);
                nextc();
                single_operator = false;
            }
        }

    }

    char *ptr = buffer_ptr(buffer);
    // Null terminator.
    buffer_write(buffer, 0x00);
    if (!single_operator)
    {
        // In the event the operator is not valid we will need to split the operators
        // back into single ones again, we accomplish this by flushing the buffer except
        // the first byte. We then assign null terminator to the second byte of the buffer
        // causing the string to terminator after the first operator.
        if (!op_valid(ptr))
        {
            read_op_flush_back_keep_first(buffer);
            // Create a new null terminator to ignore the rest of the buffer.
            ptr[1] = 0x00;
        }
    }
    else if (!op_valid(ptr))
    {
        compiler_error(lex_process->compiler, "The operator %s is invalid\n", ptr);
    }
    return ptr;
}

static struct token *token_make_operator_for_value(const char *val)
{
    return token_create(&(struct token){TOKEN_TYPE_OPERATOR, .sval = val});
}

static struct token *token_make_operator_or_string()
{
    char op = peekc();
    if (op == '<')
    {
        // It's possible we have an include statement lets just check that if so we will have a string
        struct token *last_token = lexer_last_token();
        if (token_is_keyword(last_token, "include"))
        {
            // Aha so we have something like this "include <stdio.h>"
            // We are at the "stdio.h>" bit so we need to treat this as a string
            return token_make_string('<', '>');
        }
    }

    struct token *token = token_create(&(struct token){TOKEN_TYPE_OPERATOR, .sval = read_op()});
    if (op == '(')
    {
        lex_new_expression();
    }

    return token;
}

const char *read_hex_number_str()
{
    const char *num = NULL;
    struct buffer *buffer = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buffer, c, is_hex_char(c));

    // Null terminator.
    buffer_write(buffer, 0x00);

    return buffer_ptr(buffer);
}

const char *read_number_str()
{
    const char *num = NULL;
    struct buffer *buffer = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buffer, c, (c >= '0' && c <= '9') || c == '.');

    // Null terminator.
    buffer_write(buffer, 0x00);

    return buffer_ptr(buffer);
}

unsigned long long read_number()
{
    const char *s = read_number_str();
    return atoll(s);
}

static int lexer_number_type(char c)
{
    int number_type = NUMBER_TYPE_NORMAL;
    if (c == 'L')
    {
        number_type = NUMBER_TYPE_LONG;
    }
    else if (c == 'f')
    {
        number_type = NUMBER_TYPE_FLOAT;
    }
    return number_type;
}

static struct token *token_make_number_for_value(unsigned long val)
{
    int number_type = lexer_number_type(peekc());
    if (number_type != NUMBER_TYPE_NORMAL)
    {
        // This is not a normal number type, therefore we have a character
        // that we need to pop off
        nextc();
    }
    return token_create(&(struct token){TOKEN_TYPE_NUMBER, .llnum = val, .num.type = number_type});
}

static struct token *token_make_number()
{
    return token_make_number_for_value(read_number());
}

static struct token *token_make_identifier_or_keyword()
{
    struct buffer *buffer = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buffer, c, (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
    // Null terminator.
    buffer_write(buffer, 0x00);

    if (is_keyword(buffer_ptr(buffer)))
    {
        return token_create(&(struct token){TOKEN_TYPE_KEYWORD, .sval = buffer_ptr(buffer)});
    }

    return token_create(&(struct token){TOKEN_TYPE_IDENTIFIER, .sval = buffer_ptr(buffer)});
}

static struct token *token_make_symbol()
{
    char c = peekc();
    if (c == ')')
    {
        lex_finish_expression();
    }

    c = nextc();
    struct token *token = token_create(&(struct token){TOKEN_TYPE_SYMBOL, .cval = c});
    return token;
}
static struct token *read_token_special()
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
static struct token *token_make_one_line_comment()
{
    // We must read the whole comment, we know when we are done as we will
    // have a new line terminator

    struct buffer *buffer = buffer_create();
    char c = 0;
    LEX_GETC_IF(buffer, c, c != '\n' && c != EOF);
    return token_create(&(struct token){TOKEN_TYPE_COMMENT, .sval = buffer_ptr(buffer)});
}

/**
 * Note: this function expects the file pointer to be pointing at the comment body.
 * For example we expect the input stream to be: "Hello world" rather than "/*hello world"
 */
static struct token *token_make_multiline_comment()
{
    struct buffer *buffer = buffer_create();
    char c = 0;
    while (1)
    {
        LEX_GETC_IF(buffer, c, c != '*' && c != EOF);
        if (c == EOF)
        {
            // End of file.. but this comment is not closed.
            // Something bad here, it should not be allowed
            error("EOF reached whilst in a multi-line comment. The comment was not terminated! \"%s\" \n", (const char *)buffer_ptr(buffer));
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

    return token_create(&(struct token){TOKEN_TYPE_COMMENT, .sval = buffer_ptr(buffer)});
}
static struct token *handle_comment()
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
        else if (peekc() == '*')
        {
            nextc();
            return token_make_multiline_comment();
        }

        // This is not a comment it must just be a normal division operator
        // Let's push back to the input stream the division symbol
        // so that it can be proceesed correctly by the operator token function
        pushc('/');

        return token_make_operator_or_string();
    }

    return NULL;
}

static struct token *token_make_quote()
{
    assert_next_char('\'');
    char c = nextc();
    if (c == '\\')
    {
        // We have an escape here.
        c = nextc();
        c = lex_get_escaped_char(c);
    }

    assert_next_char('\'');
    // Characters are basically just small numbers. Treat it as such.
    return token_create(&(struct token){TOKEN_TYPE_NUMBER, .cval = c});
}

static struct token *read_next_token();
static struct token *handle_whitespace()
{
    // Let's get the previous token and set the whitespace boolean#
    struct token *last_token = lexer_last_token();
    if (last_token)
    {
        last_token->whitespace = true;
    }

    nextc();
    return read_next_token();
}

static struct token *token_make_special_number_binary()
{
    // Skip the "b"
    nextc();

    // Now we should have a binary number we must convert it to a decimal
    // and store it
    unsigned long number = 0;
    // Let's read the whole number
    const char *number_str = read_number_str();
    // Validate that it is a binary number
    lexer_validate_binary_string(number_str);

    // Okay we have a binary number, covnert it to an integer
    number = strtol(number_str, NULL, 2);
    return token_make_number_for_value(number);
}

static struct token *token_make_special_number_hexadecimal()
{
    // Skip the "x"
    nextc();

    // Now we should have a binary number we must convert it to a decimal
    // and store it
    unsigned long number = 0;
    // Let's read the whole number
    const char *number_str = read_hex_number_str();
    // No need to validate the HEX number its impossible for us to have anything else

    // Okay we have a binary number, covnert it to an integer
    number = strtol(number_str, NULL, 16);
    return token_make_number_for_value(number);
}

static struct token *token_make_special_number()
{
    struct token *token = NULL;
    struct token *last_token = lexer_last_token();
    if (!last_token || !(last_token->type == TOKEN_TYPE_NUMBER && last_token->llnum == 0))
    {
        // This must be an identifier since we have no last token
        // or that last number is not zero
        return token_make_identifier_or_keyword();
    }

    // Let's pop off the last token as we dont' need it
    // it was just used to describe that their will be a hex or binary value proceeding
    lexer_pop_token();

    // What do we have here?
    char c = peekc();
    if (c == 'x')
    {
        token = token_make_special_number_hexadecimal();
    }
    else if (c == 'b')
    {
        token = token_make_special_number_binary();
    }

    return token;
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
        token = token_make_operator_or_string();
        break;
    SYMBOL_CASE:
        token = token_make_symbol();
        break;
    // I.e 0x55837 or 0b0011100
    case 'x':
    case 'b':
        token = token_make_special_number();
        break;
    case '\'':
        token = token_make_quote();
        break;
    case '"':
        token = token_make_string('"', '"');
        break;
    case '\n':
        token = token_make_newline();
        break;
    // Spaces, tabs are ignored..
    case ' ':
    case '\t':
        token = handle_whitespace();
        break;

    case EOF:
        // aha we are done!
        break;
    default:
        token = read_token_special();
        if (!token)
        {
            compiler_error(lex_process->compiler, "Invalid token provided. Character %c\n", c);
        }
        break;
    }

    return token;
}

int lex(struct lex_process *process)
{
    process->current_expression_count = 0;
    process->parentheses_buffer = NULL;
    process->argument_string_buffer = NULL;
    lex_process = process;
    // Copy filename to the lex process
    lex_process->pos.filename = process->compiler->cfile.abs_path;

    struct token *token = read_next_token();
    while (token)
    {
        vector_push(lex_process->token_vec, token);
        token = read_next_token();
    }

    return LEXICAL_ANALYSIS_ALL_OK;
}

struct lex_process *tokens_build_for_string(struct compile_process *compiler, const char *str)
{
    struct buffer *buffer = buffer_create();
    buffer_printf(buffer, str);
    struct lex_process *process = lex_process_create(compiler, &lexer_string_buffer_functions, buffer);
    if (!process)
    {
        return NULL;
    }

    if (lex(process) != LEXICAL_ANALYSIS_ALL_OK)
    {
        return NULL;
    }

    return process;
}