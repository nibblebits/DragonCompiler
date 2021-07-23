#ifndef HASHMAP_H
#define HASHMAP_H

#include <stddef.h>
#include <memory.h>
#include <stdlib.h>

#define HASHMAP_DEFAULT_SIZE 1024*512
#define HASHMAP_MINIMUM_SIZE 1024

struct hashmap_data
{
    // Pointer to the value
    void* value;

    // The key name
    char key[];
};

struct hashmap
{
    struct hashmap_data data;
    size_t size;
    size_t count;
};


struct hashmap* hashmap_create(size_t size);
int hashmap_hash(struct hashmap* hashmap, const char* key);
void hashmap_insert(struct hashmap* hashmap, const char* key, void** data, size_t len);
void* hashmap_data(struct hashmap* hashmap, const char* key);

#endif