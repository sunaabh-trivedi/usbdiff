#include "json_helper.h"
#include <stdlib.h>

cJSON* create_json(fhashmap_t *map)
{   
    if(!map) return NULL;

    cJSON *files = cJSON_CreateObject();
    if(!files) return NULL;

    // TODO: Add metadata to JSON object
    for(int i = 0; i < HMAP_MAX_ELEMS; i++) {

        fhashentry_t *curr = map->farray[i];
        while(curr) {
            if(cJSON_AddStringToObject(files, curr->filename, curr->filehash) == NULL)  {
                cJSON_Delete(files);
                return NULL;
            }
            curr = curr->next;
        }
    }

    return files;
}

void parse_json(fhashmap_t *map, const char *infile)
{   
    // For now, reserve 1MB for JSON contents
    // TODO: Find a way to stream the data and build the cJSON object incrementally
    size_t rdbuf_size = 1 << 20;
    char *rdbuf = malloc(rdbuf_size);
    if(!rdbuf)  {
        fprintf(stderr, "Failed to allocate space for JSON parsing buffer\n");
    }

    FILE *fp = fopen(infile, "rb");
    if(!fp) {
        fprintf(stderr, "Failed to open JSON file for parsing\n");
        return;
    }

    size_t nread;
    nread = fread(rdbuf, 1, rdbuf_size, fp);
    if(nread < 0)  {
        fprintf(stderr, "Failed to read JSON into parsing buffer\n");
        return;
    }
    
    cJSON *object = cJSON_Parse(rdbuf);
    free(rdbuf);

    // Pick apart cJSON object into fhashmap entries
    cJSON *elem = NULL;
    cJSON_ArrayForEach(elem, object)    {
        if(!cJSON_IsString(elem)) continue;
        fhashmap_add(map, elem->string, elem->valuestring);
    }

}

void print_json(FILE *out, const cJSON *const object)   
{   
    if(!object) {
        fprintf(stderr, "Failed to print cJSON object.\n");
        return;
    }

    char *string = cJSON_Print(object);
    if(!string) {   
        fprintf(stderr, "Failed to print cJSON object.\n");
        return;
    }

    fprintf(out, "%s\n", string);
    cJSON_free(string);
}