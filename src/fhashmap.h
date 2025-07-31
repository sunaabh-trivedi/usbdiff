#ifndef FHASHMAP_H
#define FHASHMAP_H

#include <string.h>

// Hash map of filename to file hash
#define HMAP_MAX_ELEMS 4096

struct fhash_entry  {
    char *filename;
    char *filehash;
    long long file_size;
    long long mtime;
    struct fhash_entry *next; // Chaining
};

typedef struct fhash_entry fhashentry_t;

typedef struct
{
    fhashentry_t *farray[HMAP_MAX_ELEMS];

} fhashmap_t;

void fhashmap_add(fhashmap_t* map, const char *filename, const char *filehash, long long file_size, long long mtime);
fhashentry_t* fhashmap_lookup(fhashmap_t* map, char* filename);
void fhashmap_print(fhashmap_t *map);
void fhashmap_init(fhashmap_t *map);
void fhashmap_free(fhashmap_t *map);

#endif