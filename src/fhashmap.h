#ifndef FHASHMAP_H
#define FHASHMAP_H

// Hash map of filename to file hash
#define HMAP_MAX_ELEMS 4096

struct fhash_entry  {
    char *filename;
    char *filehash;
    size_t file_size;
    time_t mtime;
    struct fhash_entry *next; // Chaining
};

typedef struct fhash_entry fhashentry_t;

typedef struct
{
    fhashentry_t *farray[HMAP_MAX_ELEMS];

} fhashmap_t;

void fhashmap_add(fhashmap_t* map, const char *const filename, const char *const filehash);
char* fhashmap_lookup(fhashmap_t* map, char* filename);
void fhashmap_print(fhashmap_t *map);
void fhashmap_init(fhashmap_t *map);

#endif