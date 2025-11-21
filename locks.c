#include <stdio.h>
#include "locks.h"


// Global log file pointer - set this before operations
FILE *global_log = NULL;

// Existing prototypes take int priority, treat negative as "don't/no log"
void log_message(int priority, const char *message) {
    if (!global_log) return;
    fprintf(global_log, "%lld: THREAD %d %s\n", current_timestamp(), priority, message);
    fflush(global_log);
}
static inline void maybe_log(int priority, const char *msg) {
    if (priority >= 0) log_message(priority, msg);
}
void rwlock_init(rwlock_t *rw) {
    rw->readers = 0;
    sem_init(&rw->lock, 0, 1);      // Binary semaphore for protecting readers count
    sem_init(&rw->writelock, 0, 1); // Binary semaphore for write exclusion
}

void rwlock_acquire_readlock(rwlock_t *rw, int priority) {
    maybe_log(priority, "WAITING FOR MY TURN");
    sem_wait(&rw->lock);
    maybe_log(priority, "AWAKENED FOR WORK");
    rw->readers++;
    if (rw->readers == 1) // First reader acquires write lock
        sem_wait(&rw->writelock);
    sem_post(&rw->lock);
    maybe_log(priority, "READ LOCK ACQUIRED");
}

void rwlock_release_readlock(rwlock_t *rw, int priority) {
    sem_wait(&rw->lock);
    rw->readers--;
    if (rw->readers == 0) // Last reader releases write lock
        sem_post(&rw->writelock);
    sem_post(&rw->lock);
    maybe_log(priority, "READ LOCK RELEASED");
}

void rwlock_acquire_writelock(rwlock_t *rw, int priority) {
    maybe_log(priority, "WAITING FOR MY TURN");
    sem_wait(&rw->writelock);
    maybe_log(priority, "AWAKENED FOR WORK");
    maybe_log(priority, "WRITE LOCK ACQUIRED");
}

void rwlock_release_writelock(rwlock_t *rw, int priority) {
    sem_post(&rw->writelock);
    maybe_log(priority, "WRITE LOCK RELEASED");
}

void rwlock_destroy(rwlock_t *rw) {
    sem_destroy(&rw->lock);
    sem_destroy(&rw->writelock);
}
