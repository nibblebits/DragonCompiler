#include "compiler.h"
#include <memory.h>
#include <stdlib.h>
#include <assert.h>

struct scope* scope_alloc()
{
    struct scope* scope = calloc(sizeof(struct scope), 1);
    scope->entities = vector_create(sizeof(void*));
    vector_set_peek_pointer_end(scope->entities);
    // We want peeks to this vector to decrement not increment
    // I.e we read from last in
    vector_set_flag(scope->entities, VECTOR_FLAG_PEEK_DECREMENT);

    return scope;
}

void scope_dealloc(struct scope* scope)
{
    vector_free(scope->entities);
    free(scope);
}

struct scope* scope_create_root(struct compile_process* process)
{
    // Assert no root is currently set
    assert(!process->scope.root);
    assert(!process->scope.current);

    struct scope* root_scope = scope_alloc();
    process->scope.root = root_scope;
    process->scope.current = root_scope;
}

struct scope* scope_new(struct compile_process* process)
{
    assert(process->scope.root);
    assert(process->scope.current);

    struct scope* new_scope = scope_alloc();
    // Let's set ourselves as the new scope but we will maintain
    // the previous scope as a parent so we can always go up..
    new_scope->parent = process->scope.current;
    process->scope.current = new_scope;
}

void scope_iteration_start(struct scope* scope)
{
    vector_set_peek_pointer(scope->entities, 0);

    // Decrementing vectors our iteration should start at the end.
    if (scope->entities->flags & VECTOR_FLAG_PEEK_DECREMENT)
    {
        vector_set_peek_pointer_end(scope->entities);
    }
}

void scope_iteration_end(struct  scope* scope)
{
   // Nothing to do..
}

void* scope_iterate_back(struct scope* scope)
{
    if (vector_count(scope->entities) == 0)
        return NULL;

    return *(void**)(vector_peek(scope->entities));

}
void* scope_last_entity_at_scope(struct scope* scope)
{
    if (!vector_count(scope->entities))
    {
        if (scope->parent)
        {
            return scope_last_entity_at_scope(scope->parent);
        }

        return NULL;
    }

    return *(void**)(vector_back(scope->entities));
}

void* scope_last_entity(struct compile_process* process)
{
    void* last = scope_last_entity_at_scope(process->scope.current);
    if (last)
    {
        return last;
    }

    struct scope* parent = process->scope.current->parent;
    if (!last && parent)
    {
        return scope_last_entity_at_scope(parent);
    }

    return NULL;
}

void scope_push(struct compile_process* process, void* ptr)
{
    vector_push(process->scope.current->entities, &ptr);
}

void scope_finish(struct compile_process* process)
{
    struct scope* new_current_scope = process->scope.current->parent;
    scope_dealloc(process->scope.current);
    process->scope.current = new_current_scope;

    if (process->scope.root && 
        !process->scope.current)
    {
        // We have a root scope but no current scope, this must mean we just finished
        // the root scope, we have no more scopes. we have deallocated the roo
        // set the root as NULL
        process->scope.root = NULL;
    }
}

