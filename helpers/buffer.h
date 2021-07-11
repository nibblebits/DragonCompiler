#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>
#include <stddef.h>

#define BUFFER_REALLOC_AMOUNT 32
struct buffer
{
    char* data;
    int len;
    int msize;
};

struct buffer* buffer_create();
void buffer_extend(struct buffer* buffer, size_t size);
void buffer_printf(struct buffer* buffer, const char* fmt, ...);
void buffer_printf_no_terminator(struct buffer* buffer, const char* fmt, ...);
void buffer_write(struct buffer* buffer, char c);
void* buffer_ptr(struct buffer* buffer);
void buffer_free(struct buffer* buffer);


#endif