#ifndef STACK_H
#define STACK_H

#include <stdint.h>
#include <stddef.h>
#include "misc.h"

/**
 * A stack that can be used for all situations that you require a stack.
 * Accepts any sized element.
 * 
 * Warning: Memory is currently not freed when popping from the stack, this needs to be implemented
 * and is a little more complicated because we can push and pop from any side of the stack.
 * Modulas operator will probably play a part in implementing this as well as another compontent or two.
 * Come back to this in the future and fix the stack memory leak.
 */

enum
{
    STACK_RESPONSE_OK,
    STACK_RESPONSE_OUT_OF_BOUNDS
};

typedef int STACK_RESPONSE;

/**
 * We do not want to have to resize the entire stack every time we push
 * because of this we will allocate more room than we actually need :)
 */
#define STACK_SIZE_OF_RESERVE 20
struct stack
{
    // The size per element
    size_t esize;

    // The data pointer to the data.
    void* data;

    // Real total on the stack only for elements we are using
    size_t rtotal;

    // Total items we have allocated in memory, even those on the stack we are not using
    // This makes sense because we don't want to reallocate the entire memory each time
    // that someone pushes to us
    size_t mtotal;

    /**
     * The current tail index we are on. This will get incremented for stack_push,and decremented
     * for stack_pop_back
     */
    int tindex;

    /**
     * The current head index we are on.
     * This will get incremented stack_pop_front
     */
    int hindex;
    
};

struct stack* stack_create(size_t size_per_element);
STACK_RESPONSE stack_push_back(struct stack *stack, void *ptr);
int stack_count(struct stack* stack);

STACK_RESPONSE stack_read_at(struct stack *stack, int index, void *__out__ ptr);
STACK_RESPONSE stack_peek_tail(struct stack *stack, void *__out__ ptr);
STACK_RESPONSE stack_peek_head(struct stack *stack, void *__out__ ptr);
STACK_RESPONSE stack_pop_tail(struct stack* stack, void* __out__ ptr);
STACK_RESPONSE stack_pop_head(struct stack* stack, void* __out__ ptr);


void stack_set_head_position(struct stack *stack, int position);
void stack_set_tail_position(struct stack *stack, int position);
void stack_reset_head_position(struct stack *stack);
void stack_reset_tail_position(struct stack *stack);

#endif