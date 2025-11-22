#ifndef LOCKS_H
#define LOCKS_H

#include <semaphore.h>
#include <sys/time.h>
#include <stdint.h>

// Timestamp comes from HashFunctions.c
long long current_timestamp(void);

// ----- Reader/Writer lock (OSTEP-style) -----
typedef struct _rwlock_t {
    sem_t writelock;   // writers get exclusive access
    sem_t lock;        // protects readers counter
    int   readers;     // number of active readers
} rwlock_t;

// Log one line to hash.log if logging is enabled
void log_message(int priority, const char *message);

// RW-lock API
void rwlock_init(rwlock_t *rw);
void rwlock_acquire_readlock(rwlock_t *rw, int priority);
void rwlock_release_readlock(rwlock_t *rw, int priority);
void rwlock_acquire_writelock(rwlock_t *rw, int priority);
void rwlock_release_writelock(rwlock_t *rw, int priority);
void rwlock_destroy(rwlock_t *rw);

// Counters used by chash.c and print_table()
void increment_lock_acquisition(void);
void increment_lock_release(void);
int  get_total_acquisitions(void);
int  get_total_releases(void);

#endif
