#ifndef HASH_LOGGER_H
#define HASH_LOGGER_H

#include <stddef.h>

void logger_init(const char *fname);
void logger_close(void);
long long current_timestamp();
void log_thread_msg(int priority, const char *msg);

#endif 