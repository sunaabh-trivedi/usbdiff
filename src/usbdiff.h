#ifndef USBDIFF_H
#define USBDIFF_H

#include <windows.h>

#define MAX_DIFFS 1024
#define MAX_FILES 5192

typedef struct {
    char filename[MAX_PATH];
    enum { MODIFIED, DELETED } status;
} filediff_t;

typedef struct {
    char filenames[MAX_FILES][MAX_PATH];
    size_t len;
} filelist_t;

#endif