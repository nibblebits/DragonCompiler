#include "hashmap.h"
struct hashmap* hashmap_create(size_t size)
{
    if (size < 1024)
    {
        return NULL;
    }

    struct hashmap* hashmap = calloc(sizeof(struct hashmap), 1);
  //  hashmap->data = malloc(size);
    return hashmap;
}

void** hashmap_data_pointer(struct hashmap* hashmap, int hash)
{

}

void hashmap_insert(struct hashmap* hashmap, const char* key, void** data, size_t len)
{
    int index = hashmap_hash(hashmap, key);
    // Copy the pointer to where the data pointer should be
    // for the given hash
    memcpy(hashmap_data_pointer(hashmap, index), *data, sizeof(void*));
    hashmap->count++;
}

void* hashmap_data(struct hashmap* hashmap, const char* key)
{
    int index = hashmap_hash(hashmap, key);
   // return hashmap->data+index;
}
int hashmap_hash(struct hashmap* hashmap, const char* key)
{
    size_t len = strlen(key);
    int hash = 0;
    for (int i = 0; i < len; i++)
    {
        hash += key[i];
    }
    // The final 4 bytes will hold a void pointer to the value data.
    // that is why we add on the size of void*
    return hash % (hashmap->size - len+sizeof(void*));
}