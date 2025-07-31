#ifndef USBDIFF_H
#define USBDIFF_H


#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define MAX_PATH 1024
#define FOREGROUND_RED   "\033[31m"
#define FOREGROUND_GREEN "\033[32m"
#define RESET_COLOR      "\033[0m"
#define _strdup strdup
#endif

#define MAX_DIFFS 1024
#define MAX_FILES 5192

#define DEBUG 0

typedef struct {
    char filename[MAX_PATH];
    enum { MODIFIED, DELETED } status;
} filediff_t;

typedef struct  {
    char filename[MAX_PATH];
    long long file_size;
    long long mtime;
} file_t;

typedef struct {
    file_t files[MAX_FILES];
    size_t len;
} filelist_t;

#endif