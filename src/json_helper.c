#include "json_helper.h"
#include <stdlib.h>

#define CHUNK_SIZE (4 * 1024)  // 4KB chunks
#define MAX_BUFFER_SIZE (64 * 1024)  // Maximum buffer size before we must parse

static stream_buffer_t* init_stream_buffer(size_t initial_capacity) {
    stream_buffer_t *sb = malloc(sizeof(stream_buffer_t));
    if (!sb) return NULL;
    
    sb->buffer = malloc(initial_capacity);
    if (!sb->buffer) {
        free(sb);
        return NULL;
    }
    
    sb->buffer_size = 0;
    sb->buffer_used = 0;
    sb->buffer_capacity = initial_capacity;
    return sb;
}

static int expand_buffer(stream_buffer_t *sb, size_t additional_size) {
    size_t needed_capacity = sb->buffer_size + additional_size;
    if (needed_capacity <= sb->buffer_capacity) {
        return 1; // No expansion needed
    }
    
    size_t new_capacity = sb->buffer_capacity * 2;
    while (new_capacity < needed_capacity) {
        new_capacity *= 2;
    }
    
    char *new_buffer = realloc(sb->buffer, new_capacity);
    if (!new_buffer) {
        return 0; // Allocation failed
    }
    
    sb->buffer = new_buffer;
    sb->buffer_capacity = new_capacity;
    return 1;
}

static void free_stream_buffer(stream_buffer_t *sb) {
    if(!sb) return;

    free(sb->buffer);
    free(sb);
}

static int append_to_buffer(stream_buffer_t *sb, const char *data, size_t data_size) {
    if (!expand_buffer(sb, data_size)) {
        return 0;
    }
    
    memcpy(sb->buffer + sb->buffer_size, data, data_size);
    sb->buffer_size += data_size;
    return 1;
}

// Find the end of a complete JSON object in the buffer
// Returns the position after the complete JSON object, or -1 if incomplete
static ssize_t find_complete_json_object(const char *buffer, size_t buffer_size, size_t start_pos) {
    if (start_pos >= buffer_size) return -1;
    
    int brace_count = 0;
    int bracket_count = 0;
    int in_string = 0;
    int escaped = 0;
    size_t i;
    
    // Skip whitespace to find start of JSON object
    while (start_pos < buffer_size && 
           (buffer[start_pos] == ' ' || buffer[start_pos] == '\t' || 
            buffer[start_pos] == '\n' || buffer[start_pos] == '\r')) {
        start_pos++;
    }
    
    if (start_pos >= buffer_size) return -1;
    
    // Check if this looks like a JSON object or array
    if (buffer[start_pos] != '{' && buffer[start_pos] != '[') {
        return -1;
    }
    
    for (i = start_pos; i < buffer_size; i++) {
        char c = buffer[i];
        
        if (escaped) {
            escaped = 0;
            continue;
        }
        
        if (c == '\\' && in_string) {
            escaped = 1;
            continue;
        }
        
        if (c == '"') {
            in_string = !in_string;
            continue;
        }
        
        if (in_string) {
            continue;
        }
        
        switch (c) {
            case '{':
                brace_count++;
                break;
            case '}':
                brace_count--;
                if (brace_count == 0 && bracket_count == 0) {
                    return i + 1; // Found complete object
                }
                break;
            case '[':
                bracket_count++;
                break;
            case ']':
                bracket_count--;
                if (brace_count == 0 && bracket_count == 0) {
                    return i + 1; // Found complete array
                }
                break;
        }
        
        if (brace_count < 0 || bracket_count < 0) {
            return -1; // Malformed JSON
        }
    }
    
    return -1; // Incomplete JSON object
}

static cJSON* merge_json_objects(cJSON *target, cJSON *source) {
    if (!target || !source) return target;
    
    if (!cJSON_IsObject(target) || !cJSON_IsObject(source)) {
        return target;
    }
    
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, source) {
        cJSON *duplicate = cJSON_Duplicate(item, 1);
        if (duplicate) {
            // If key already exists, replace it
            cJSON_DeleteItemFromObject(target, item->string);
            cJSON_AddItemToObject(target, item->string, duplicate);
        }
    }
    
    return target;
}

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
        fprintf(stderr, "parse_json: Failed to allocate space for JSON parsing buffer\n");
    }

    FILE *fp = fopen(infile, "rb");
    if(!fp) {
        fprintf(stderr, "parse_json: Failed to open JSON file for parsing\n");
        return;
    }

    size_t nread;
    nread = fread(rdbuf, 1, rdbuf_size, fp);
    if(nread == 0)  {
        fprintf(stderr, "parse_json: Failed to read JSON into parsing buffer\n");
        return;
    }
    
    cJSON *object = cJSON_ParseWithLength(rdbuf, nread);
    free(rdbuf);
    if (!object) {
        fprintf(stderr, "parse_json: Failed to parse JSON\n");
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

int parse_json_stream(fhashmap_t *map, const char *infile) 
{
    FILE *fp = fopen(infile, "rb");
    if (!fp) {
        fprintf(stderr, "parse_json_stream: Failed to open JSON file for streaming parse\n");
        return 0;
    }
    
    stream_buffer_t *sb = init_stream_buffer(CHUNK_SIZE * 2);
    if (!sb) {
        fprintf(stderr, "parse_json_stream: Failed to initialize stream buffer\n");
        fclose(fp);
        return 0;
    }
    
    char chunk[CHUNK_SIZE];
    size_t bytes_read;
    cJSON *merged_object = cJSON_CreateObject();
    int success = 1;
    
    if (!merged_object) {
        fprintf(stderr, "parse_json_stream: Failed to create merged JSON object\n");
        success = 0;
        goto cleanup;
    }
    
    // Stream data from file in 4KB chunks
    while ((bytes_read = fread(chunk, 1, CHUNK_SIZE, fp)) > 0) {
        // Add chunk to buffer
        if (!append_to_buffer(sb, chunk, bytes_read)) {
            fprintf(stderr, "parse_json_stream: Failed to append data to buffer\n");
            success = 0;
            break;
        }
        
        // Process complete JSON objects in the buffer
        size_t processed_pos = 0;
        ssize_t complete_pos;
        
        while ((complete_pos = find_complete_json_object(sb->buffer, sb->buffer_size, processed_pos)) > 0) {
            // Extract the complete JSON object
            size_t object_length = complete_pos - processed_pos;
            char *json_str = malloc(object_length + 1);
            if (!json_str) {
                fprintf(stderr, "parse_json_stream: Failed to allocate memory for JSON string\n");
                success = 0;
                goto cleanup;
            }
            
            memcpy(json_str, sb->buffer + processed_pos, object_length);
            json_str[object_length] = '\0';
            
            // Parse the JSON object
            cJSON *parsed_object = cJSON_Parse(json_str);
            free(json_str);
            
            if (!parsed_object) {
                fprintf(stderr, "parse_json_stream: Failed to parse JSON object: %s\n", cJSON_GetErrorPtr());
                processed_pos = complete_pos; // Skip this object and continue
                continue;
            }
            
            // Merge with main object
            merge_json_objects(merged_object, parsed_object);
            cJSON_Delete(parsed_object);
            
            processed_pos = complete_pos;
        }
        
        // Move unprocessed data to beginning of buffer
        if (processed_pos > 0) {
            size_t remaining = sb->buffer_size - processed_pos;
            memmove(sb->buffer, sb->buffer + processed_pos, remaining);
            sb->buffer_size = remaining;
        }
        
        // // Check if buffer is getting too large without finding complete objects
        // if (sb->buffer_size > MAX_BUFFER_SIZE) {
        //     fprintf(stderr, "Buffer overflow: JSON object too large or malformed\n");
        //     // success = 0;
        //     // break;
        // }
    }
    
    // Process any remaining complete JSON objects in buffer
    if (success && sb->buffer_size > 0) {
        ssize_t complete_pos = find_complete_json_object(sb->buffer, sb->buffer_size, 0);
        if (complete_pos > 0) {
            char *json_str = malloc(complete_pos + 1);
            if (json_str) {
                memcpy(json_str, sb->buffer, complete_pos);
                json_str[complete_pos] = '\0';
                
                cJSON *parsed_object = cJSON_Parse(json_str);
                if (parsed_object) {
                    merge_json_objects(merged_object, parsed_object);
                    cJSON_Delete(parsed_object);
                }
                free(json_str);
            }
        } else if (sb->buffer_size > 0) {
            // Check if remaining data is just whitespace
            int only_whitespace = 1;
            for (size_t i = 0; i < sb->buffer_size; i++) {
                if (sb->buffer[i] != ' ' && sb->buffer[i] != '\t' && 
                    sb->buffer[i] != '\n' && sb->buffer[i] != '\r') {
                    only_whitespace = 0;
                    break;
                }
            }
            if (!only_whitespace) {
                fprintf(stderr, "Warning: Incomplete JSON data at end of file\n");
            }
        }
    }
    
    if(success) {
        // Pick apart cJSON object into fhashmap entries
        cJSON *elem = NULL;
        cJSON_ArrayForEach(elem, merged_object)    {
            if(!cJSON_IsObject(elem)) continue;

            const char *filename = elem->string;
            cJSON *hash = cJSON_GetObjectItem(elem, "hash");
            cJSON *size = cJSON_GetObjectItem(elem, "size");
            cJSON *mtime = cJSON_GetObjectItem(elem, "mtime");

            if (!cJSON_IsString(hash) || !cJSON_IsNumber(size) || !cJSON_IsNumber(mtime)) continue;

            fhashmap_add(map, filename, hash->valuestring, (long long)size->valuedouble, (long long)mtime->valuedouble);
        }
    }

cleanup:
    if (merged_object) {
        cJSON_Delete(merged_object);
    }
    
    free_stream_buffer(sb);

    fclose(fp);
    return success;
}

void print_json(FILE *out, const cJSON *const object)   
{   
    if(!object) {
        fprintf(stderr, "print_json: Failed to print cJSON object.\n");
        return;
    }

    char *string = cJSON_Print(object);
    if(!string) {   
        fprintf(stderr, "print_json: Failed to print cJSON object.\n");
        return;
    }

    fprintf(out, "%s\n", string);
    cJSON_free(string);
}