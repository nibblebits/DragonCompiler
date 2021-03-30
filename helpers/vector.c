
#include "vector.h"
#include <memory.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

static bool vector_in_bounds_for_at(struct vector* vector, int index)
{
    return (index >= 0 && index < vector->rindex);
}


static bool vector_in_bounds_for_pop(struct vector* vector, int index)
{
    return (index >= 0 && index < vector->mindex);
}

static void vector_assert_bounds_for_pop(struct vector* vector, int index)
{
    assert (vector_in_bounds_for_pop(vector, index));
}

struct vector* vector_create(size_t esize)
{
    struct vector* vector = malloc(sizeof(struct vector));
    vector->data = malloc(esize * VECTOR_ELEMENT_INCREMENT);
    vector->mindex = VECTOR_ELEMENT_INCREMENT;
    vector->rindex = 0;
    vector->esize = esize;
}

void vector_free(struct vector* vector)
{
    free(vector->data);
    free(vector);
}

void vector_resize(struct vector* vector)
{
    if (vector->rindex < vector->mindex)
    {
        // No resize required.., we dont allow you to resize lower to avoid
        // protential mistakes and dereferenced pointers.
        return;
    }
    vector->data = realloc(vector->data, ((vector->rindex + VECTOR_ELEMENT_INCREMENT) * vector->esize));
}


void* vector_at(struct vector* vector, int index)
{
    return vector->data+(index * vector->esize);
}

void vector_set_peek_pointer(struct vector* vector, int index)
{
    vector->pindex = index;
}

void* vector_peek(struct vector* vector)
{
    if (!vector_in_bounds_for_at(vector, vector->pindex))
    {
        return NULL;
    }

    void* ptr = vector_at(vector, vector->pindex);
    vector->pindex++;
    return ptr;
}

void vector_push(struct vector* vector, void* elem)
{
    if (vector->rindex >= vector->mindex)
    {
        vector_resize(vector);
    }
    
    void* ptr = vector_at(vector, vector->rindex);
    memcpy(ptr, elem, vector->esize);

    vector->rindex++;
    vector->count++;
}


void vector_pop(struct vector* vector)
{
    // Popping from the vector will just decrement the index, no need to free memory
    // the next push will overwrite it.
    vector->rindex -=1;
    vector->count -=1;

    vector_assert_bounds_for_pop(vector, vector->rindex);
}

void* vector_back(struct vector* vector)
{
    vector_assert_bounds_for_pop(vector, vector->rindex);

    return vector_at(vector, vector->rindex-1);
}

int vector_count(struct vector* vector)
{
    return vector->count;
}