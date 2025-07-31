#include "fhashmap.h"
#include <stdlib.h>
#include <stdio.h>

static unsigned int hash_string(const char* str) 
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash % HMAP_MAX_ELEMS;
}

inline void fhashmap_init(fhashmap_t *map) {
    if(!map) {
        fprintf(stderr, "fhashmap_init: Failed to access hashmap\n");
        return;
    }
    memset(map->farray, 0, sizeof(map->farray));
}


void fhashmap_add(fhashmap_t* map, const char *filename, const char *filehash, long long file_size, long long mtime)
{   
    if(!map) {
        fprintf(stderr, "fhashmap_add: Failed to access hashmap\n");
        return;
    }

    unsigned int hash = hash_string(filename);
    
    fhashentry_t *entry = map->farray[hash];
    if(entry == NULL)   {
        entry = malloc(sizeof(fhashentry_t));
        if(!entry) 
        {   
            fprintf(stderr, "Failed to add %s to hashmap\n", filename);
            return;
        }

        entry->filename = strdup(filename);
        entry->filehash = strdup(filehash);
        entry->file_size = file_size;
        entry->mtime = mtime;
        entry->next = NULL;

        map->farray[hash] = entry;
    }
    else    {
        fhashentry_t *curr = entry;
        while(curr->next) curr = curr->next;

        fhashentry_t *new = malloc(sizeof(fhashentry_t)); 
        if(!new)
        {   
            fprintf(stderr, "Failed to add %s to hashmap\n", filename);
            return;
        }

        new->filename = strdup(filename);
        new->filehash = strdup(filehash);
        new->file_size = file_size;
        new->mtime = mtime;
        new->next = NULL;

        curr->next = new;
    }
}

fhashentry_t* fhashmap_lookup(fhashmap_t* map, char* filename)
{   
    if(!map) {
        fprintf(stderr, "fhashmap_lookup: Failed to access hashmap\n");
        return;
    }
    unsigned int hash = hash_string(filename);

    fhashentry_t *entry = map->farray[hash];
    if(!entry)   {
        return NULL;
    }

    fhashentry_t *curr = entry;
    while(curr) {
        if(!strcmp(curr->filename, filename))  {
            return curr;
        }

        curr = curr->next;
    }

    return NULL;
}

void fhashmap_print(fhashmap_t *map)    
{   
    if(!map) {
        fprintf(stderr, "fhashmap_print: Failed to access hashmap\n");
        return;
    }

    for(int i = 0; i < HMAP_MAX_ELEMS; i++) {

        fhashentry_t *curr = map->farray[i];
    
        while(curr) {
            printf("Key: %s, Value: %s\n", curr->filename, curr->filehash);
            curr = curr->next;    
        }
    }
}

void fhashmap_free(fhashmap_t *map)
{
    for(int i = 0; i < HMAP_MAX_ELEMS; ++i) {
        fhashentry_t *entry = map->farray[i];
        while(entry) {
            fhashentry_t *next = entry->next;
            free((char *)entry->filename); // Cast to non-const if needed
            free((char *)entry->filehash);
            free(entry);
            entry = next;
        }
        map->farray[i] = NULL;
    }
}
