#include <stdio.h>
#include <stdlib.h>
#include "helpers/vector.h"
#include "compiler.h"
struct dc_file
{
    // The file pointer
    FILE* fp;

    // The file size
    size_t size;

    // Vector of characters in the file.
    struct vector* vec;
};

struct dc_file* dc_fopen(const char* filename)
{
    int err = 0;
    int sz = 0;
    struct vector* vec = NULL;
    FILE* f = fopen(filename, "r");
    struct dc_file* dc_f = NULL;
    if (!f)
    {
        err = -1;
        goto out;
    }

    fseek(f, 0L, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    vec = vector_create(sizeof(char));
    if(vector_fread(vec, sz, f) < 0)
    {
        err = -1;
        goto out;
    }

    dc_f = malloc(sizeof(struct dc_file));
    dc_f->size = sz;
    dc_f->fp = f;
    dc_f->vec = vec;
out:
    if (err < 0)
    {
        if(vec)
        {
            vector_free(vec);
        }

        if (dc_f)
        {
            free(dc_f);
        }
    }
    return dc_f;
}

void dc_fclose(struct dc_file* file)
{
    fclose(file->fp);
    vector_free(file->vec);
    free(file);
}   

char dc_read(struct dc_file* file)
{   
    char c = (char)vector_peek(file->vec);
    vector_pop(file->vec);
    return c;
}

struct vector* dc_read_until(struct dc_file* file, const char* delims)
{
    struct vector* vec = vector_create(sizeof(char));
    int total_delims = strlen(delims);
    char c = dc_read(file);
    while(!char_is_delim(c, delims))
    {
        vector_push(vec, &c);
    }

    return vec;
}