#include "fhashmap.h"
#include <stdio.h>
#include "usbdiff.h"
#include "json_helper.h"
#include "sha-256.h"
#include <omp.h>

#define DEBUG 0

fhashmap_t curr_fhashmap;
fhashmap_t prev_fhashmap;

int list_add(filelist_t *list, char *path) 
{
    if (list->len < MAX_FILES) {
        strncpy(list->filenames[list->len],
                (path), MAX_PATH - 1);
        list->filenames[list->len][MAX_PATH-1] = '\0';
        list->len++;
    } else {
        fprintf(stderr, "filelist_add: MAX_FILES reached\n");
        return 1;
    }

    return 0;
}

void list_print(filelist_t *list)
{   
    printf("List: \n");
    for(int i = 0; i < list->len; i++)  {
        printf("%s\n", list->filenames[i]);
    }
    printf("-------------------\n");
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

void collect_files_list(const char *dir, filelist_t *list) 
{
    char search_path[MAX_PATH];
    snprintf(search_path, MAX_PATH, "%s\\*", dir);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(search_path, &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
            continue;

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir, findData.cFileName);

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            collect_files_list(full_path, list);
        } else {
            list_add(list, _strdup(full_path));
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}

int load_files(const filelist_t *const list, fhashmap_t *map)
{   
    if(!list || !map) return 1;
    
    #pragma omp parallel for schedule(dynamic, 8)
    for(int i = 0; i < list->len; i++)  {
        char *filename = list->filenames[i];
        char *hash = compute_sha256(filename);
        if(!hash) {
            fprintf(stderr, "Couldn't hash %s, skipping\n", filename);
            continue;
        }
        
        #pragma omp critical
        fhashmap_add(map, filename, _strdup(hash));

        free(hash);
    }

    return 0;
}

size_t map_diff(filediff_t *diff, fhashmap_t *curr_map, fhashmap_t *prev_map)
{
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
    
    return diff_count;
}

void print_diff(filediff_t *diff, size_t diff_count)   
{
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
}

int main(int argc, char **argv)
{   
    if(argc != 2)    {
        printf("Usage: ./usbdiff <directory>\n");
        return 1;
    }
    
    char *directory = argv[1];
    
    filelist_t list;
    list.len = 0;
    
    // Load snapshot of current directory into hashmap
    collect_files_list((const char *) directory, &list);
    load_files(&list, &curr_fhashmap);

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
    filediff_t *diffs = malloc(sizeof(filediff_t)*MAX_DIFFS);
    if(!diffs)  {
        return 0;
    }

    size_t diff_count = map_diff(diffs, &curr_fhashmap, &prev_fhashmap);
    print_diff(diffs, diff_count);

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