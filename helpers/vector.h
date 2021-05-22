#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// We want at least 20 vector element spaces in reserve before having
// to reallocate memory again
#define VECTOR_ELEMENT_INCREMENT 20

enum
{
    VECTOR_FLAG_PEEK_DECREMENT = 0b00000001
};

struct vector
{
    void* data;
    // The pointer index is the index that will be read next upon calling "vector_peek".
    // This index will then be incremented
    int pindex;
    int rindex;
    int mindex;
    int count;
    int flags;
    size_t esize;
};


struct vector* vector_create(size_t esize);
void vector_free(struct vector* vector);
void* vector_at(struct vector* vector, int index);
void* vector_peek_no_increment(struct vector* vector);
void* vector_peek(struct vector* vector);
void vector_set_flag(struct vector* vector, int flag);
void vector_unset_flag(struct vector* vector, int flag);

/**
 * Peeks into the vector of pointers, returning the pointer value its self
 * 
 * Use this function instead of vector_peek if this is a vector of pointers
 */
void* vector_peek_ptr(struct vector* vector);
void vector_set_peek_pointer(struct vector* vector, int index);
void vector_set_peek_pointer_end(struct vector* vector);
void vector_push(struct vector* vector, void* elem);
void vector_pop(struct vector* vector);
void* vector_back(struct vector* vector);
void* vector_back_ptr(struct vector* vector);
void* vector_back_ptr_or_null(struct vector* vector);

bool vector_empty(struct vector* vector);

int vector_count(struct vector* vector);
#endif