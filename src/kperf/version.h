
#pragma once

#include <stdio.h>

#define MAJOR_VERSION 0
#define MINOR_VERSION 1
#define PATCH_VERSION 0

char *get_version_str() {
    static char version_str[16];
    sprintf(version_str, "%d.%d.%d", MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    return version_str;
}