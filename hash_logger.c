#define _TIMESPEC_DEFINED
#include "hash_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>


static FILE *g_logf = NULL;
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;

void logger_init(const char *fname) {
    pthread_mutex_lock(&g_log_lock);
    if (!g_logf) {
        g_logf = fopen(fname, "a");
        if (!g_logf) g_logf = stdout;
    }
    pthread_mutex_unlock(&g_log_lock);
}

void logger_close(void) {
    pthread_mutex_lock(&g_log_lock);
    if (g_logf && g_logf != stdout) fclose(g_logf);
    g_logf = NULL;
    pthread_mutex_unlock(&g_log_lock);
}

//given
long long current_timestamp() {  
  struct timeval te;  
  gettimeofday(&te, NULL); // get current time  
  long long microseconds = (te.tv_sec * 1000000) + te.tv_usec; // calculate milliseconds  
  return microseconds;  
} 

//message to hash.log
void log_thread_msg(int priority, const char *msg) {
    pthread_mutex_lock(&g_log_lock);
    if (!g_logf) g_logf = stdout;
    fprintf(g_logf, "%lld: THREAD %d %s\n", current_timestamp(), (int)priority, msg);
    fflush(g_logf);
    pthread_mutex_unlock(&g_log_lock);
}
