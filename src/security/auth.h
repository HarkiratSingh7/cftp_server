#ifndef AUTH_H
#define AUTH_H

#include "connection.h"

int user_exists(const char *username, connection_t *connection);
int authenticate_and_switch_user(const char *username,
                                 const char *password,
                                 char *error_buf);

#endif
