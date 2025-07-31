#include "json_helper.h"
#include <stdlib.h>

cJSON* create_json(fhashmap_t *map) {
    if (!map) return NULL;

    cJSON *files = cJSON_CreateObject();
    if (!files) return NULL;

    for (int i = 0; i < HMAP_MAX_ELEMS; i++) {
        fhashentry_t *curr = map->farray[i];
        while (curr) {
            cJSON *entry = cJSON_CreateObject();
            if (!entry) {
                cJSON_Delete(files);
                return NULL;
            }

            cJSON_AddStringToObject(entry, "hash", strdup(curr->filehash));
            cJSON_AddNumberToObject(entry, "size", (double)curr->file_size);
            cJSON_AddNumberToObject(entry, "mtime", (double)curr->mtime);

            if (!cJSON_AddItemToObject(files, curr->filename, entry)) {
                cJSON_Delete(entry);
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
    if(nread == 0)  {
        fprintf(stderr, "Failed to read JSON into parsing buffer\n");
        return;
    }
    
    cJSON *object = cJSON_ParseWithLength(rdbuf, nread);
    free(rdbuf);
    if (!object) {
        fprintf(stderr, "Failed to parse JSON\n");
        return;
    }

    // Pick apart cJSON object into fhashmap entries
    cJSON *elem = NULL;
    cJSON_ArrayForEach(elem, object)    {
        if(!cJSON_IsObject(elem)) continue;

        const char *filename = elem->string;
        cJSON *hash = cJSON_GetObjectItem(elem, "hash");
        cJSON *size = cJSON_GetObjectItem(elem, "size");
        cJSON *mtime = cJSON_GetObjectItem(elem, "mtime");

        if (!cJSON_IsString(hash) || !cJSON_IsNumber(size) || !cJSON_IsNumber(mtime)) continue;

        fhashmap_add(map, filename, hash->valuestring, (long long)size->valuedouble, (long long)mtime->valuedouble);
    }

    cJSON_Delete(object);
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