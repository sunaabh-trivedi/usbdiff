#include "fhashmap.h"
#include <stdlib.h>
#include <string.h>
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


void fhashmap_add(fhashmap_t *map, const char *const filename, const char *const filehash)   
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

        entry->filename = filename;
        entry->filehash = filehash;
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

        new->filename = filename;
        new->filehash = filehash;
        new->next = NULL;

        curr->next = new;
    }
}

char* fhashmap_lookup(fhashmap_t* map, char* filename)
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
            return curr->filehash;
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