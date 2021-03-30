#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>
#include <stdint.h>

// We want at least 20 vector element spaces in reserve before having
// to reallocate memory again
#define VECTOR_ELEMENT_INCREMENT 20
struct vector
{
    void* data;
    // The pointer index is the index that will be read next upon calling "vector_peek".
    // This index will then be incremented
    int pindex;
    int rindex;
    int mindex;
    int count;
    size_t esize;
};


struct vector* vector_create(size_t esize);
void vector_free(struct vector* vector);
void* vector_at(struct vector* vector, int index);
void* vector_peek_no_increment(struct vector* vector);
void* vector_peek(struct vector* vector);
void vector_set_peek_pointer(struct vector* vector, int index);
void vector_push(struct vector* vector, void* elem);
void vector_pop(struct vector* vector);
void* vector_back(struct vector* vector);
int vector_count(struct vector* vector);
#endif