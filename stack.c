#include "stack.h"
#include <stdlib.h>
#include <memory.h>
#include "misc.h"

struct stack *stack_create(size_t size_per_element)
{
    struct stack *s = malloc(sizeof(struct stack));
    s->data = calloc(size_per_element, STACK_SIZE_OF_RESERVE);
    s->esize = size_per_element;
    s->mtotal = STACK_SIZE_OF_RESERVE;
    s->rtotal = 0;
    s->hindex = 0;
    s->tindex = 0;

    return s;
}

int stack_count(struct stack* stack)
{
    return stack->rtotal;
}

STACK_RESPONSE stack_push_back(struct stack *stack, void *ptr)
{
    if (stack->tindex >= stack->mtotal)
    {
        // We have to resize the data array....
        stack->data = realloc(stack->data, ((stack->rtotal + STACK_SIZE_OF_RESERVE) * stack->esize));
        stack->mtotal += STACK_SIZE_OF_RESERVE;
    }
    
    void *dst_addr = stack->data + (stack->tindex * stack->esize);
    memcpy(dst_addr, ptr, stack->esize);
    stack->tindex++;
    stack->rtotal++;

    return STACK_RESPONSE_OK;
}

STACK_RESPONSE stack_read_at(struct stack *stack, int index, void *__out__ ptr)
{
    if (index >= stack->mtotal)
    {
        return STACK_RESPONSE_OUT_OF_BOUNDS;
    }

    void *addr = stack->data + (index * stack->esize);
    memcpy(ptr, addr, stack->esize);
    return STACK_RESPONSE_OK;
}

STACK_RESPONSE stack_peek_tail(struct stack *stack, void *__out__ ptr)
{
    return stack_read_at(stack, stack->tindex, ptr);
}

STACK_RESPONSE stack_peek_head(struct stack *stack, void *__out__ ptr)
{
    return stack_read_at(stack, stack->hindex, ptr);
}

STACK_RESPONSE stack_pop_tail(struct stack* stack, void* __out__ ptr)
{
    STACK_RESPONSE res = stack_peek_tail(stack, ptr);
    // We will worry about freeing later...
    stack->tindex -=1;
    stack->rtotal -=1;
    return res;
}


STACK_RESPONSE stack_pop_head(struct stack* stack, void* __out__ ptr)
{
    STACK_RESPONSE res = stack_peek_head(stack, ptr);
    // Don't forget to free the memory later on...

    // When reading from the head we read forward.
    stack->hindex +=1;
    stack->rtotal -=1;
    return res;
}

void stack_set_head_position(struct stack *stack, int position)
{
    stack->hindex = position;
}

void stack_set_tail_position(struct stack *stack, int position)
{
    stack->tindex = position;
}

void stack_reset_head_position(struct stack *stack)
{
    stack_set_head_position(stack, 0);
}

void stack_reset_tail_position(struct stack *stack)
{
    stack_set_tail_position(stack, 0);
}
