#include "cJSON.h"
#include "fhashmap.h"
#include <stdio.h>

typedef struct {
    char *buffer;
    size_t buffer_size;
    size_t buffer_used;
    size_t buffer_capacity;
} stream_buffer_t;

// Create cJSON object from existing fhashmap_t map
cJSON* create_json(fhashmap_t *map);

// Parse JSON from infile into an internal fhashmap_t map
void parse_json(fhashmap_t *map, const char *infile);

// Print JSON from cJSON object to output  stream (stdout, stderr, any file, etc.)
void print_json(FILE *out, const cJSON *const object);
