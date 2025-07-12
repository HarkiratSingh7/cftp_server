
#include "security.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"

#define BUFFLEN 1024
#define MAX_TRIES 0x1000

char *get_passwd_from_uid(uint32_t uid, struct passwd **result)
{
    if (!result) return NULL;

    struct passwd *pwd = calloc(1, sizeof(struct passwd));
    int e;
    size_t pwd_buffer_length = BUFFLEN;
    char *buffer = calloc(1, pwd_buffer_length);

    for (int i = 0;
         i < MAX_TRIES &&
         (0 != (e = getpwuid_r(uid, pwd, buffer, pwd_buffer_length, result)));
         i++)
    {
        size_t new_len = 2 * pwd_buffer_length;
        if (new_len < pwd_buffer_length)
        {
            ERROR("Size_t value overflow! while doing getpwuid_r call");
            free(buffer);
            return NULL;
        }
        pwd_buffer_length = new_len;
        char *newbuffer = realloc(buffer, new_len);
        if (!newbuffer)
        {
            ERROR("Memory allocation failed!");
            return NULL;
        }
        buffer = newbuffer;
    }
    if (0 != e)
    {
        ERROR("Can't get group id for this file!");
        free(buffer);
        return NULL;
    }

    return buffer;
}

char *get_group_from_gid(uint32_t gid, struct group **result)
{
    if (!result) return NULL;

    struct group *grp = calloc(1, sizeof(struct group));
    int e;
    size_t grp_buffer_length = BUFFLEN;
    char *buffer = calloc(1, grp_buffer_length);

    for (int i = 0;
         i < MAX_TRIES &&
         (0 != (e = getgrgid_r(gid, grp, buffer, grp_buffer_length, result)));
         i++)
    {
        size_t new_len = 2 * grp_buffer_length;
        if (new_len < grp_buffer_length)
        {
            ERROR("Size_t value overflow! while doing getgrgid_r call");
            free(buffer);
            return NULL;
        }

        grp_buffer_length = new_len;
        char *newbuffer = realloc(buffer, new_len);
        if (!newbuffer)
        {
            ERROR("Memory allocation failed!");
            return NULL;
        }
        buffer = newbuffer;
    }
    if (0 != e)
    {
        ERROR("Can't get group id for this file!");
        free(buffer);
        return NULL;
    }

    return buffer;
}

int validate_params(const char *params, char *err_buf)
{
    if (params && strlen(params) > 0)
    {
        /*  Prevent traversal outside the chroot */
        if (strstr(params, "..") != NULL)
        {
            snprintf("%s", 256, "550 Invalid path\r\n");
            return 0;
        }
    }

    return 1;
}
