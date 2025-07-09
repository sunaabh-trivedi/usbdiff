#include "fhashmap.h"
#include <windows.h>
#include <stdio.h>
#include "cJSON.h"
#include "sha-256.h"

fhashmap_t fhashmap;

cJSON* create_json(fhashmap_t *map)
{   
    if(!map) return NULL;

    cJSON *files = cJSON_CreateObject();
    if(!files) return NULL;

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

void parse_json(void)
{

}

void print_json(cJSON *object)   
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

    printf(string);
    printf("\n");
}

static inline char *get_hex_string(char hash[32])  {

    char *hex = malloc(65);
    if(!hex) return NULL;

    for(int i = 0; i < 32; i++) {
        sprintf(hex + i*2, "%02x", (unsigned char)hash[i]);
    }

    hex[64] = '\0';
    return hex;
}

char *compute_sha256(char full_path[MAX_PATH])
{   
    struct Sha_256 sha_256;
    char hash[32]; // 32-byte hash
    char rdbuf[4096]; // Stream buffer, 4KB
    
    sha_256_init(&sha_256, hash);

    FILE *file = fopen(full_path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file\n");
        return NULL;
    }

    size_t nread;
    while((nread = fread(rdbuf, 1, sizeof(rdbuf), file)) > 0)    {
        sha_256_write(&sha_256, rdbuf, nread);
    }

    fclose(file);
    sha_256_close(&sha_256);

    char *hex = get_hex_string(hash);
    return hex;
}

void load_files(const char* dir, fhashmap_t* map) 
{
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*.*", dir);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(search_path, &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to open directory: %s\n", dir);
        return;
    }

    do {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
            continue;

        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char full_path[MAX_PATH];
            snprintf(full_path, sizeof(full_path), "%s\\%s", dir, findData.cFileName);

            char* hash = compute_sha256(full_path);
            if(!hash) 
            {   
                fprintf(stderr, "Failed to compute SHA256 hash for file %s\n", full_path);
                return;
            }

            char* key = _strdup(findData.cFileName);
            char* value = _strdup(hash);
            free(hash); // Can free the original hash string allocated in get_hex_string() since we've copied into value 

            if (key && value) {
                fhashmap_add(map, key, value);
            } else {
                fprintf(stderr, "Failed to allocate memory for key/value\n");
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}

int main(void)
{
    load_files("dummy", &fhashmap);
    fhashmap_print(&fhashmap);

    cJSON *files_object = create_json(&fhashmap);
    if(!files_object)   {
        fprintf(stderr, "Failed to create cJSON object\n");
        return -1;
    }

    print_json(files_object);
}