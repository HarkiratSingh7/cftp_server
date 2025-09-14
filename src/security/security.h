#ifndef SECURITY_H
#define SECURITY_H

#include <grp.h>
#include <pwd.h>
#include <stdint.h>

char *get_passwd_from_uid(uint32_t uid, struct passwd **result);
char *get_group_from_gid(uint32_t gid, struct group **result);

int is_path_safe(const char *params);

#endif
