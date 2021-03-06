#include "buffer.h"
#include <stdlib.h>

struct buffer* buffer_create()
{
    struct buffer* buf = malloc(sizeof(struct buffer));
    buf->data = malloc(BUFFER_REALLOC_AMOUNT);
    buf->len = 0;
    buf->msize = BUFFER_REALLOC_AMOUNT;
    return buf;
}

void buffer_extend(struct buffer* buffer)
{
    buffer->data = realloc(buffer->data, buffer->msize+BUFFER_REALLOC_AMOUNT);
    buffer->msize+=BUFFER_REALLOC_AMOUNT;
}

void buffer_write(struct buffer* buffer, char c)
{
    if (buffer->msize == (buffer->len+1))
    {
        buffer_extend(buffer);
    }

    buffer->data[buffer->len] = c;
    buffer->len++;
}

void* buffer_ptr(struct buffer* buffer)
{
    return buffer->data;
}

void buffer_free(struct buffer* buffer)
{
    free(buffer->data);
    free(buffer);
}
