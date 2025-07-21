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


void fhashmap_add(fhashmap_t *map, const char *const filename, const char *const filehash)   
{
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
    for(int i = 0; i < HMAP_MAX_ELEMS; i++) {

        fhashentry_t *curr = map->farray[i];
    
        while(curr) {
            printf("Key: %s, Value: %s\n", curr->filename, curr->filehash);
            curr = curr->next;    
        }
    }
}