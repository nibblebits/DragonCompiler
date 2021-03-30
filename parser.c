#include "compiler.h"
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <stdarg.h>
#include <memory.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

static struct compile_process *current_process;
int parse_next();

static struct token *token_next()
{
    return vector_peek(current_process->token_vec);
}

static struct token* token_next_expected(int type)
{
    struct token* token = token_next();
    if (token->type != type)
        compiler_error(current_process, "Unexpected token\n");
    
    return token;
}

/**
 * Pops the last node we pushed to the vector
 */
static struct node* node_pop()
{
    struct node* last_node = *((struct node**)(vector_back(current_process->node_vec)));
    struct node* last_node_root = *((struct node**)(vector_back(current_process->node_tree_vec)));

    vector_pop(current_process->node_vec);

    if (last_node == last_node_root)
    {
        // We also have pushed this node to the tree root so we need to pop from here too
        vector_pop(current_process->node_tree_vec);
    }

    return last_node;
}

static void node_push(struct node* node)
{
    vector_push(current_process->node_vec, &node);
}

static struct node *node_create(struct node *_node)
{
    struct node *node = malloc(sizeof(struct node));
    memcpy(node, _node, sizeof(struct node));
    node_push(node);
    return node;
}

static struct node *token_to_node(struct token *token)
{
    if (!token)
    {
        return NULL;
    }

    struct node *node = NULL;
    switch (token->type)
    {
    case TOKEN_TYPE_NUMBER:
        node = node_create(&(struct node){NODE_TYPE_NUMBER, .llnum = token->llnum});
        break;

    case TOKEN_TYPE_IDENTIFIER:
        node = node_create(&(struct node){NODE_TYPE_NUMBER, .sval = token->sval});
        break;

    default:
        compiler_error(current_process, "Problem converting token to node. No valid node exists for token of type %i\n", token->type);
    }
}

struct node *make_exp_node(struct node* node_left, struct node* node_right, const char *op)
{
    return node_create(&(struct node){NODE_TYPE_EXPRESSION, .exp.op = op, .exp.left = node_left, .exp.right = node_right});
}

struct node* parse_exp(struct token* op_token)
{
    const char* op = op_token->sval;
    // We must pop the last node as this will be the left operand
    struct node* node_left = node_pop();
    
    // We must parse the right operand
    parse_next();
    struct node* node_right = node_pop();

    return make_exp_node(node_left, node_right, op);
}

int parse_next()
{
    struct token *token = token_next();
    if (!token)
    {
        return -1;
    }
    struct node *node = NULL;
    switch (token->type)
    {
    case TOKEN_TYPE_NUMBER:
    case TOKEN_TYPE_IDENTIFIER:
        node = token_to_node(token);
        break;

    case TOKEN_TYPE_OPERATOR:
        node = parse_exp(token);
    break;  

    default:
        compiler_error(current_process, "Unexpected token\n");
    }

    if (!node)
    {
        return -1;
    }
    return 0;
}

int parse(struct compile_process *process)
{
    current_process = process;
    vector_set_peek_pointer(process->token_vec, 0);
    struct node* node = NULL;
    while (parse_next() == 0)
    {
        // Pop the node that was created on the stack so we can add it to the root of the tree
        // This element we are popping came from parse_next
        struct node* node = node_pop();
        // Push the root element to the tree
        vector_push(process->node_tree_vec, &node);
        // Also push it back to the main node stack that we just popped from
        node_push(node);
    }

    return PARSE_ALL_OK;
}