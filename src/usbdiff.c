#include "fhashmap.h"
#include <stdio.h>
#include "usbdiff.h"
#include "json_helper.h"
#include "sha-256.h"
#include <omp.h>

#define DEBUG 0

fhashmap_t curr_fhashmap;
fhashmap_t prev_fhashmap;

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

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir, findData.cFileName);

        #if DEBUG
        printf("Scanning file: %s\n", full_path);
        #endif

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Recurse into subdirectory
            load_files(full_path, map);
        } else {
            char* hash = compute_sha256(full_path);
            if (!hash) {
                fprintf(stderr, "Failed to compute SHA256 hash for file %s\n", full_path);
                continue;
            }

            char* key = _strdup(full_path);
            char* value = _strdup(hash);
            free(hash);

            if (key && value) {
                fhashmap_add(map, key, value);
            } else {
                fprintf(stderr, "Failed to allocate memory for key/value\n");
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}

filediff_t* map_diff(fhashmap_t *curr_map, fhashmap_t *prev_map)
{
    filediff_t *diff = malloc(sizeof(filediff_t) * MAX_DIFFS);
    int diff_count = 0;

    for(int i = 0; i < HMAP_MAX_ELEMS; i++) {
        fhashentry_t *prev_map_entry = prev_map->farray[i];

        while(prev_map_entry)    {

            char *curr_hash = fhashmap_lookup(curr_map, prev_map_entry->filename);

            // File Unchanged
            if (curr_hash && strcmp(prev_map_entry->filehash, curr_hash) == 0) {
                prev_map_entry = prev_map_entry->next;
                continue;
            }

            // File Modified or Deleted
            if (diff_count < MAX_DIFFS) {
                if (curr_hash) {
                    diff[diff_count].status = MODIFIED;
                } else {
                    diff[diff_count].status = DELETED;
                }

                strncpy(diff[diff_count].filename, prev_map_entry->filename, MAX_PATH - 1);
                diff[diff_count].filename[MAX_PATH - 1] = '\0';
                diff_count++;
            }

            prev_map_entry = prev_map_entry->next;
        }


        // New files created
        fhashentry_t *curr_map_entry = curr_map->farray[i];
        while(curr_map_entry)    {

            char *prev_hash = fhashmap_lookup(prev_map, curr_map_entry->filename);

            if (diff_count < MAX_DIFFS) {
                if (!prev_hash) {
                    diff[diff_count].status = MODIFIED;
                    strncpy(diff[diff_count].filename, curr_map_entry->filename, MAX_PATH - 1);
                    diff[diff_count].filename[MAX_PATH - 1] = '\0';
                    diff_count++;
                }
            }

            curr_map_entry = curr_map_entry->next;
        }
    }

    if(diff_count == 0)  {
        printf("No changes to directory.\n");
        return NULL;
    }
    
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    printf("Diffs: \n");
    for(int i = 0; i < diff_count; i++) {

        CONSOLE_SCREEN_BUFFER_INFO info;
        if(!GetConsoleScreenBufferInfo(hConsole, &info)) return NULL;

        if(diff[i].status == MODIFIED) { SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN); printf("+\t"); }
        if(diff[i].status == DELETED)  { SetConsoleTextAttribute(hConsole, FOREGROUND_RED); printf("-\t"); }

        printf("%s\n", diff[i].filename);

        SetConsoleTextAttribute(hConsole, info.wAttributes);

    }
    return diff;
}

int main(int argc, char **argv)
{   

    if(argc != 2)    {
        printf("Usage: ./usbdiff <directory>\n");
        return 1;
    }

    char *directory = argv[1];

    // Load snapshot of current directory into hashmap
    load_files((const char *) directory, &curr_fhashmap);

    #if DEBUG
    printf("Current Hashmap: \n");
    fhashmap_print(&curr_fhashmap);
    printf("-----------------------------------\n");
    #endif

    // Compare with JSON (previous snapshot of directory) to find changes
    parse_json(&prev_fhashmap, ".usbdiff.json");

    #if DEBUG
    printf("Previous Hashmap: \n");
    fhashmap_print(&prev_fhashmap);
    printf("-----------------------------------\n");
    #endif

    // Compare prev and curr hashmaps
    filediff_t *diffs = map_diff(&curr_fhashmap, &prev_fhashmap);
    if(!diffs)  {
        return 0;
    }

    free(diffs);

    // Update JSON with changes made to directory 
    cJSON *files_object = create_json(&curr_fhashmap);
    if(!files_object)   {
        fprintf(stderr, "Failed to create cJSON object\n");
        return -1;
    }

    const char *outfile = ".usbdiff.json";
    FILE *fp = fopen(outfile, "w");

    if(!fp) {
        fprintf(stderr, "Failed to write to JSON output file\n");
    }
    
    print_json(fp, files_object);

    return 0;
}