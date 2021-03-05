#include "misc.h"
#include "lexer.h"
#include "stack.h"
#include <stdio.h>


#warning "debugger seems to be broken..."
int main(int argc, char** argv)
{
   // struct lex_process process;
    //dragon_lex(&process, "dog int abc; int def; long wine;");
    
    // Let's try to create a stack of integers to see if this is working..
    struct stack* stack = stack_create(sizeof(int));
    // OK i see what i did lol

    int r;
    r = 50;
    stack_push_back(stack, (void*) &r);
    r = 28;
    stack_push_back(stack, (void*) &r);
    r = 45;
    stack_push_back(stack, (void*) &r);
    r = 99;
    stack_push_back(stack, (void*) &r);


    int total = stack_count(stack);
    for (int i = 0; i < total; i++)
    {
        int val;
        stack_pop_head(stack, &val);
        printf("%i\n", val);
    }
    return 0;
}