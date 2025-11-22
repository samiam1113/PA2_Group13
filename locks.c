#include <stdio.h>
#include <pthread.h>
#include "locks.h"

// Exposed in chash.c via `extern FILE *global_log;`
FILE *global_log = NULL;

// Global counters and guard
static int total_lock_acquisitions = 0;
static int total_lock_releases     = 0;
static pthread_mutex_t counter_mu  = PTHREAD_MUTEX_INITIALIZER;

// Central logger (no-op if global_log == NULL)
void log_message(int priority, const char *message) {
    if (!global_log) return;
    // Format matches the professor’s: "<ts>: THREAD <id> <text>"
    fprintf(global_log, "%lld: THREAD %d %s\n", current_timestamp(), priority, message);
    fflush(global_log);
}

// Local helper: only log when priority >= 0
static inline void maybe_log(int priority, const char *msg) {
    if (priority >= 0) log_message(priority, msg);
}

// ------------- RW lock implementation -------------
void rwlock_init(rwlock_t *rw) {
    rw->readers = 0;
    sem_init(&rw->lock,      0, 1);
    sem_init(&rw->writelock, 0, 1);
}

void rwlock_acquire_readlock(rwlock_t *rw, int priority) {
    maybe_log(priority, "WAITING FOR MY TURN");
    sem_wait(&rw->lock);
    maybe_log(priority, "AWAKENED FOR WORK");

    rw->readers++;
    if (rw->readers == 1) {
        sem_wait(&rw->writelock);
    }
    sem_post(&rw->lock);

    maybe_log(priority, "READ LOCK ACQUIRED");

    if (priority >= 0) {
        pthread_mutex_lock(&counter_mu);
        total_lock_acquisitions++;
        pthread_mutex_unlock(&counter_mu);
    }
}

void rwlock_release_readlock(rwlock_t *rw, int priority) {
    sem_wait(&rw->lock);
    rw->readers--;
    if (rw->readers == 0) {
        sem_post(&rw->writelock);
    }
    sem_post(&rw->lock);

    maybe_log(priority, "READ LOCK RELEASED");

    if (priority >= 0) {
        pthread_mutex_lock(&counter_mu);
        total_lock_releases++;
        pthread_mutex_unlock(&counter_mu);
    }
}

void rwlock_acquire_writelock(rwlock_t *rw, int priority) {
    maybe_log(priority, "WAITING FOR MY TURN");
    sem_wait(&rw->writelock);
    maybe_log(priority, "AWAKENED FOR WORK");
    maybe_log(priority, "WRITE LOCK ACQUIRED");

    if (priority >= 0) {
        pthread_mutex_lock(&counter_mu);
        total_lock_acquisitions++;
        pthread_mutex_unlock(&counter_mu);
    }
}

void rwlock_release_writelock(rwlock_t *rw, int priority) {
    sem_post(&rw->writelock);
    maybe_log(priority, "WRITE LOCK RELEASED");

    if (priority >= 0) {
        pthread_mutex_lock(&counter_mu);
        total_lock_releases++;
        pthread_mutex_unlock(&counter_mu);
    }
}

void rwlock_destroy(rwlock_t *rw) {
    sem_destroy(&rw->lock);
    sem_destroy(&rw->writelock);
}

// ------------- counters exposed to chash.c -------------
void increment_lock_acquisition(void) {
    pthread_mutex_lock(&counter_mu);
    total_lock_acquisitions++;
    pthread_mutex_unlock(&counter_mu);
}

void increment_lock_release(void) {
    pthread_mutex_lock(&counter_mu);
    total_lock_releases++;
    pthread_mutex_unlock(&counter_mu);
}

int get_total_acquisitions(void) {
    pthread_mutex_lock(&counter_mu);
    int v = total_lock_acquisitions;
    pthread_mutex_unlock(&counter_mu);
    return v;
}

int get_total_releases(void) {
    pthread_mutex_lock(&counter_mu);
    int v = total_lock_releases;
    pthread_mutex_unlock(&counter_mu);
    return v;
}
