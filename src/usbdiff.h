#ifndef USBDIFF_H
#define USBDIFF_H

#include <windows.h>

#define MAX_DIFFS 1024

typedef struct {
    char filename[MAX_PATH];
    enum { MODIFIED, DELETED } status;
} filediff_t;

#endif