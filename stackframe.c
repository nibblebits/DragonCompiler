#include "compiler.h"
#include <assert.h>

void stackframe_sub(struct node *func_node, int type, const char *name, size_t amount)
{
    assert((amount % STACK_PUSH_SIZE) == 0);
    size_t total_pushes = amount / STACK_PUSH_SIZE;
    for (size_t i = 0; i < total_pushes; i++)
    {
        stackframe_push(func_node, &(struct stack_frame_element){.type = type, .name = name});
    }
}

void stackframe_add(struct node *func_node, size_t amount)
{
    assert((amount % STACK_PUSH_SIZE) == 0);
    size_t total_pushes = amount / STACK_PUSH_SIZE;
    for (size_t i = 0; i < total_pushes; i++)
    {
        stackframe_pop(func_node);
    }
}

void stackframe_push(struct node *func_node, struct stack_frame_element *element)
{
    struct stack_frame *frame = &func_node->func.frame;
    // Stack grows downwards
    element->offset_from_sp = -(vector_count(frame->elements) * STACK_PUSH_SIZE);
    vector_push(frame->elements, element);
}

struct stack_frame_element *stackframe_back(struct node *func_node)
{
    struct stack_frame *frame = &func_node->func.frame;
    return vector_back(frame->elements);
}

void stackframe_pop(struct node *func_node)
{
    struct stack_frame *frame = &func_node->func.frame;
    vector_pop(frame->elements);
}

void stackframe_pop_expecting(struct node *func_node, int expecting_type, const char *expecting_name)
{
    struct stack_frame *frame = &func_node->func.frame;
    struct stack_frame_element *last_element = stackframe_back(func_node);
    assert(last_element->type == expecting_type && S_EQ(last_element->name, expecting_name));
    stackframe_pop(func_node);
}

void stackframe_assert_empty(struct node *func_node)
{
    struct stack_frame *frame = &func_node->func.frame;
    assert(vector_count(frame->elements) == 0);
}

struct stack_frame_element *stackframe_get_for_tag_name(struct node *func_node, int type, const char *name)
{
    struct stack_frame *frame = &func_node->func.frame;
    vector_set_peek_pointer(frame->elements, 0);
    struct stack_frame_element *current = vector_peek(frame->elements);
    while (current)
    {
        if (current->type == type && S_EQ(current->name, name))
            return current;

        current = vector_peek(frame->elements);
    }

    return NULL;
}