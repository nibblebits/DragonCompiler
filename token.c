#include "compiler.h"
#include "helpers/buffer.h"
#include <stdbool.h>

void tokens_join_buffer_write_token(struct buffer *fmt_buf, struct token *token)
{

    switch (token->type)
    {
    case TOKEN_TYPE_IDENTIFIER:
    case TOKEN_TYPE_OPERATOR:
    case TOKEN_TYPE_KEYWORD:
        buffer_printf(fmt_buf, "%s", token->sval);
        break;

    case TOKEN_TYPE_STRING:
        buffer_printf(fmt_buf, "\"%s\"", token->sval);
        break;

    case TOKEN_TYPE_NUMBER:
        buffer_printf(fmt_buf, "%lld", token->llnum);
        break;

    case TOKEN_TYPE_NEWLINE:
        buffer_printf(fmt_buf, "\n");
        break;

    case TOKEN_TYPE_SYMBOL:
        buffer_printf(fmt_buf, "%c", token->cval);
        break;
    default:
        FAIL_ERR("BUG: Incompatible token");
    }
}

struct vector *tokens_join_vector(struct compile_process *compiler, struct vector *token_vec)
{
    struct buffer *buf = buffer_create();
    vector_set_peek_pointer(token_vec, 0);
    struct token *token = vector_peek(token_vec);

    while (token)
    {
        tokens_join_buffer_write_token(buf, token);
        token = vector_peek(token_vec);
    }

    // Finished ? Then lets lex it
    struct lex_process* lex_process = tokens_build_for_string(compiler, buffer_ptr(buf));
    assert(lex_process);
    return lex_process->token_vec;
}

bool token_is_operator(struct token *token, const char *op)
{
    return token && token->type == TOKEN_TYPE_OPERATOR && S_EQ(token->sval, op);
}

bool token_is_keyword(struct token *token, const char *keyword)
{
    return token && token->type == TOKEN_TYPE_KEYWORD && S_EQ(token->sval, keyword);
}

bool token_is_symbol(struct token *token, char sym)
{
    return token && token->type == TOKEN_TYPE_SYMBOL && token->cval == sym;
}

bool token_is_identifier(struct token *token, const char *iden)
{
    return token && token->type == TOKEN_TYPE_IDENTIFIER && S_EQ(token->sval, iden);
}
