#include "platform/files.h"

#include <filesystem.h>
#include <stdio.h>
#include <stdlib.h>

bool files_init(void)
{
    return nitroFSInit(NULL);
}

uint8_t *files_load(const char *path, size_t *size)
{
    char full[128];
    snprintf(full, sizeof(full), "nitro:/%s", path);
    FILE *f = fopen(full, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = length > 0 ? malloc((size_t)length) : NULL;
    if (data && fread(data, 1, (size_t)length, f) != (size_t)length) {
        free(data);
        data = NULL;
    }
    fclose(f);
    *size = data ? (size_t)length : 0;
    return data;
}
