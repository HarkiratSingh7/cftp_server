#include "auth.h"

#include <crypt.h>
#include <grp.h>
#include <openssl/ssl.h>
#include <pwd.h>
#include <shadow.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define ROOT "root"

int user_exists(const char *username, connection_t *connection)
{
    if ((strncmp(username, ROOT, strlen(ROOT)) != 0) &&
        (getpwnam(username) != NULL))
    {
        strncpy(connection->username, username, 256);
        return 1;
    }
    return 0;
}

int authenticate_and_switch_user(const char *username,
                                 const char *password,
                                 char *error_buf)
{
    struct passwd *pwd = getpwnam(username);
    if (!pwd)
    {
        if (error_buf)
        {
            snprintf(error_buf, 256, "530 User '%s' not found\r\n", username);
        }
        return 0;
    }

    struct spwd *shadow_entry;

    shadow_entry = getspnam(username);
    if (!shadow_entry)
    {
        if (error_buf)
        {
            snprintf(error_buf, 256, "530 User '%s' not found\r\n", username);
        }
        return 0;
    }

    char *salt = shadow_entry->sp_pwdp;
    char *encrypted_passwd = crypt(password, salt);

    if (strcmp(encrypted_passwd, shadow_entry->sp_pwdp) != 0)
    {
        if (error_buf)
        {
            snprintf(error_buf,
                     256,
                     "530 Incorrect password for user '%s'\r\n",
                     username);
        }
        return 0;
    }

    /*  Set up chroot jail to user's home directory */
    if (chroot(pwd->pw_dir) != 0)
    {
        perror("chroot failed");
        if (error_buf)
        {
            snprintf(
                error_buf, 256, "530 chroot failed for '%s'\r\n", username);
        }
        return 0;
    }

    /*  Change working directory to new root */
    if (chdir("/") != 0)
    {
        perror("chdir failed");
        if (error_buf)
        {
            snprintf(error_buf, 256, "530 chdir failed for '%s'\r\n", username);
        }
        return 0;
    }

    if (setgid(pwd->pw_gid) != 0 || setuid(pwd->pw_uid) != 0)
    {
        perror("setgid/setuid failed");
        if (error_buf)
        {
            snprintf(
                error_buf, 256, "530 permission denied for '%s'\r\n", username);
        }
        return 0;
    }

    if (error_buf)
    {
        snprintf(error_buf,
                 256,
                 "230 User '%s' authenticated successfully\r\n",
                 username);
    }
    return 1;
}
