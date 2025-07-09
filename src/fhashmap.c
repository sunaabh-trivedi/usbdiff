#include "fhashmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static unsigned int hash_string(char* str) 
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    return hash % HMAP_MAX_ELEMS;
}


void fhashmap_add(fhashmap_t *map, char *filename, char *filehash)   
{
    unsigned int hash = hash_string(filename);
    
    fhashentry_t *entry = map->farray[hash];
    if(entry == NULL)   {
        entry = malloc(sizeof(fhashentry_t));
        if(!entry) return;

        entry->filename = filename;
        entry->filehash = filehash;
        entry->next = NULL;

        map->farray[hash] = entry;
    }
    else    {
        fhashentry_t *curr = entry;
        while(curr != NULL) curr = curr->next;

        curr = malloc(sizeof(fhashentry_t)); 
        if(!curr) return;

        curr->filename = filename;
        curr->filehash = filehash;
        curr->next = NULL;
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