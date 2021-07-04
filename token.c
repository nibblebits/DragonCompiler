#include "compiler.h"
#include <stdbool.h>

bool token_is_operator(struct token* token, const char *op)
{
    return token && token->type == TOKEN_TYPE_OPERATOR && S_EQ(token->sval, op);
}

bool token_is_keyword(struct token* token, const char *keyword)
{
    return token && token->type == TOKEN_TYPE_KEYWORD && S_EQ(token->sval, keyword);
}

bool token_is_symbol(struct token* token, char sym)
{
    return token && token->type == TOKEN_TYPE_SYMBOL && token->cval == sym;
}

 bool token_is_identifier(struct token* token, const char* iden)
 {
     return token && token->type == TOKEN_TYPE_IDENTIFIER && S_EQ(token->sval, iden);
 }
