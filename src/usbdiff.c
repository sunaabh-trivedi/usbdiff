#include "fhashmap.h"
#include <stdio.h>
#include "usbdiff.h"
#include "json_helper.h"
#include "sha-256.h"
#include <omp.h>
#include <stdlib.h>

int list_add(filelist_t *list, const char *path, long long size, long long mtime) 
{   

    #if DEBUG
    printf("Loading %s with size %lu and mtime %lu\n", path, size, mtime);
    #endif

    if (list->len < MAX_FILES) {
        strncpy(list->files[list->len].filename, path, MAX_PATH - 1);
        list->files[list->len].filename[MAX_PATH-1] = '\0';
        list->files[list->len].file_size = size;
        list->files[list->len].mtime = mtime;
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
        printf("%s\n", list->files[i].filename);
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

char *compute_sha256(const char *full_path)
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
#ifdef _WIN32
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
            ULARGE_INTEGER ull;
            ull.LowPart  = findData.ftLastWriteTime.dwLowDateTime;
            ull.HighPart = findData.ftLastWriteTime.dwHighDateTime;

            long long mtime = (long long)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
            long long size = ((long long)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;

            if(list_add(list, full_path, size, mtime)) return;
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);

#else // POSIX
    DIR *dp = opendir(dir);
    if (!dp) return;

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);

        struct stat path_stat;
        if (stat(full_path, &path_stat) == -1) continue;

        if (S_ISDIR(path_stat.st_mode)) {
            collect_files_list(full_path, list);
        } else {
            if(list_add(list, full_path, (long long)path_stat.st_size, path_stat.st_mtime)) return;
        }
    }

    closedir(dp);
#endif
}

int load_files(const filelist_t *const list, fhashmap_t *curr_map, fhashmap_t *prev_map)
{   
    if(!list || !curr_map || !prev_map) return -1;
    
    #pragma omp parallel for schedule(dynamic, 8)
    for(int i = 0; i < list->len; i++)  {
        file_t *file = &list->files[i];

        const char *filename = file->filename;
        long long file_size = file->file_size;
        long long mtime = file->mtime;

        int reuse_hash = 0;

        #pragma omp critical
        {
            fhashentry_t *entry = fhashmap_lookup(prev_map, filename);

            if (entry && file_size == entry->file_size && mtime == entry->mtime) {
                fhashmap_add(curr_map, filename, strdup(entry->filehash), entry->file_size, entry->mtime);
                reuse_hash = 1;
            }
        }

        if (reuse_hash) continue;

        char *hash = compute_sha256(filename);
        if(!hash) {
            fprintf(stderr, "Couldn't hash %s, skipping\n", filename);
            continue;
        }
        
        #pragma omp critical
        fhashmap_add(curr_map, filename, strdup(hash), file_size, mtime);

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

            fhashentry_t *curr_entry = fhashmap_lookup(curr_map, prev_map_entry->filename);

            // File Unchanged
            if (curr_entry && strcmp(prev_map_entry->filehash, curr_entry->filehash) == 0) {
                prev_map_entry = prev_map_entry->next;
                continue;
            }

            // File Modified or Deleted
            if (diff_count < MAX_DIFFS) {
                if (curr_entry) {
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

            fhashentry_t *prev_entry = fhashmap_lookup(prev_map, curr_map_entry->filename);

            if (diff_count < MAX_DIFFS) {
                if (!prev_entry) {
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
        return 0;
    }
    
    return diff_count;
}

void print_diff(filediff_t *diff, size_t diff_count) {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(hConsole, &info)) return;
    WORD default_attr = info.wAttributes;

    printf("Diffs:\n");
    for (size_t i = 0; i < diff_count; i++) {
        if (diff[i].status == MODIFIED) {
            SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
            printf("+\t");
        } else if (diff[i].status == DELETED) {
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
            printf("-\t");
        }

        printf("%s\n", diff[i].filename);
        SetConsoleTextAttribute(hConsole, default_attr); // Reset to previous text attributes
    }

#else // Linux/macOS
    printf("Diffs:\n");
    for (size_t i = 0; i < diff_count; i++) {
        if (diff[i].status == MODIFIED) {
            printf(FOREGROUND_GREEN "+\t");
            printf("%s\n" RESET_COLOR, diff[i].filename);
        } else if (diff[i].status == DELETED) {
            printf(FOREGROUND_RED "-\t");
            printf("%s\n" RESET_COLOR, diff[i].filename);
        }

    }
#endif
}

void ensure_directory_exists(const char *path) 
{
    char tmp[PATH_MAX];
    snprintf(tmp, PATH_MAX, "%s", path);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = '\0';

#ifdef _WIN32
            _mkdir(tmp); // Windows
#else
            mkdir(tmp, 0755); // POSIX
#endif
            *p = c;
        }
    }

#ifdef _WIN32
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
}

int copy_file(const char *src, const char *dst) 
{
    FILE *in = fopen(src, "rb");
    if (!in) {
        fprintf(stderr, "Failed to open source file: %s\n", src);
        return -1;
    }

    // Ensure parent directory of dst exists
    char dst_parent[PATH_MAX];
    snprintf(dst_parent, sizeof(dst_parent), "%s", dst);
    
    char *last_sep = strrchr(dst_parent, '/');
#ifdef _WIN32
    char *last_backslash = strrchr(dst_parent, '\\');
    if (last_backslash && (!last_sep || last_backslash > last_sep)) {
        last_sep = last_backslash;
    }
#endif
    
    if (last_sep) {
        *last_sep = '\0';
        ensure_directory_exists(dst_parent);
    }

    FILE *out = fopen(dst, "wb");
    if (!out) {
        fprintf(stderr, "copy_file: Failed to create destination file: %s\n", dst);
        fclose(in);
        return -1;
    }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fprintf(stderr, "copy_file: Failed to write to destination file: %s\n", dst);
            fclose(in); 
            fclose(out);
            return -1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

const char *make_relative_path(const char *full_path, const char *base_path) 
{
    size_t base_len = strlen(base_path);
    
    // Ensure base_path ends with separator for proper comparison
    char normalized_base[PATH_MAX];
    snprintf(normalized_base, sizeof(normalized_base), "%s", base_path);
    
    // Add trailing separator if not present
    if (base_len > 0 && normalized_base[base_len-1] != '/' && normalized_base[base_len-1] != '\\') {
#ifdef _WIN32
        strncat(normalized_base, "\\", PATH_MAX - base_len - 1);
#else
        strncat(normalized_base, "/", PATH_MAX - base_len - 1);
#endif
        base_len++;
    }
    
#ifdef _WIN32
    // Case-insensitive match on Windows
    if (_strnicmp(full_path, normalized_base, base_len) == 0) {
#else
    if (strncmp(full_path, normalized_base, base_len) == 0) {
#endif
        return full_path + base_len;
    }
    
    // If no match, try without the trailing separator
    base_len = strlen(base_path);
#ifdef _WIN32
    if (_strnicmp(full_path, base_path, base_len) == 0) {
#else
    if (strncmp(full_path, base_path, base_len) == 0) {
#endif
        const char *rel = full_path + base_len;
        if (*rel == '\\' || *rel == '/') rel++;
        return rel;
    }
    
    return full_path; // fallback
}

int main(int argc, char **argv)
{   
    char *copy_to_dir = NULL;
    char *directory = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--copy-to") == 0 && i + 1 < argc) {
            copy_to_dir = argv[++i];
        } else if (argv[i][0] != '-') {
            directory = argv[i];
        }
    }

    if (!directory) {
        printf("Usage: ./usbdiff [--copy-to <dir>] <directory>\n");
        return 1;
    }

    // If JSON doesn't exist, create one
    FILE *fp = fopen(".usbdiff.json", "r");
    if(!fp) {
        FILE *new_fp = fopen(".usbdiff.json", "w");
        if(!new_fp) {
            fprintf(stderr, "Failed to create .usbdiff.json\n");
            return -1;
        } else fclose(new_fp);

    }   else fclose(fp);
    
    fhashmap_t prev_fhashmap;
    fhashmap_t curr_fhashmap;

    fhashmap_init(&prev_fhashmap);
    fhashmap_init(&curr_fhashmap);

    parse_json_stream(&prev_fhashmap, ".usbdiff.json");

    filelist_t list;
    list.len = 0;
    
    // Load snapshot of current directory into hashmap
    collect_files_list((const char *) directory, &list);

    #if DEBUG
    list_print(&list);
    #endif

    load_files(&list, &curr_fhashmap, &prev_fhashmap);

    printf("Scanned %i files\n", list.len);

    #if DEBUG
    printf("Current Hashmap: \n");
    fhashmap_print(&curr_fhashmap);
    printf("-----------------------------------\n");
    #endif

    #if DEBUG
    printf("Previous Hashmap: \n");
    fhashmap_print(&prev_fhashmap);
    printf("-----------------------------------\n");
    #endif

    // Compare prev and curr hashmaps
    filediff_t *diffs = malloc(sizeof(filediff_t)*MAX_DIFFS);
    if(!diffs)  {
        fprintf(stderr, "Failed to allocate memory for diffs\n");
        fhashmap_free(&prev_fhashmap);
        fhashmap_free(&curr_fhashmap);
        return 1;
    }

    size_t diff_count = map_diff(diffs, &curr_fhashmap, &prev_fhashmap);
    if(diff_count == 0)  {
        free(diffs);
        fhashmap_free(&prev_fhashmap);
        fhashmap_free(&curr_fhashmap);
        return 0;
    }

    print_diff(diffs, diff_count);

    if(copy_to_dir) {
        printf("\nCopying modified files to: %s\n", copy_to_dir);
        
        for(size_t i = 0; i < diff_count; i++) {
            if(diffs[i].status != MODIFIED) continue;

            const char *rel_path = make_relative_path(diffs[i].filename, directory);

            char dst_path[PATH_MAX];
            snprintf(dst_path, sizeof(dst_path), "%s%c%s", copy_to_dir, 
#ifdef _WIN32
                     '\\',
#else
                     '/',
#endif
                     rel_path);

            if(copy_file(diffs[i].filename, dst_path) != 0)   {
                fprintf(stderr, "Failed to copy %s to %s\n", diffs[i].filename, dst_path);
            }
            else {
                printf("Copied: %s -> %s\n", rel_path, dst_path);
            }
        }
    }

    free(diffs);

    // Update JSON with changes made to directory 
    cJSON *files_object = create_json(&curr_fhashmap);
    if(!files_object)   {
        fprintf(stderr, "Failed to create cJSON object\n");
        fhashmap_free(&prev_fhashmap);
        fhashmap_free(&curr_fhashmap);
        return 1;
    }

    const char *outfile = ".usbdiff.json";
    fp = fopen(outfile, "w");

    if(!fp) {
        fprintf(stderr, "Failed to write to JSON output file\n");
        cJSON_Delete(files_object);
        fhashmap_free(&prev_fhashmap);
        fhashmap_free(&curr_fhashmap);
        return 1;
    }
    
    print_json(fp, files_object);
    fclose(fp);
    
    cJSON_Delete(files_object);
    fhashmap_free(&prev_fhashmap);
    fhashmap_free(&curr_fhashmap);

    return 0;
}