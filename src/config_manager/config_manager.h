#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "configurations.h"

#define CFTP_SERVER_CONFIG_FILE "/etc/cftp_server.conf"

void fill_default_configurations(configurations_t *config);
void read_configurations(const char *file_path, configurations_t *config);

#endif /* CONFIG_MANAGER_H */
