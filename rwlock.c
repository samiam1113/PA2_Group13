#include "rwlock.h"
#include <stdio.h>
#include <stdlib.h>


// typedef struct _rwlock_t {
//     sem_t writelock;
//     sem_t lock;
//     int readers;
// } rwlock_t;

void rwlock_init(rwlock_t *lock) {
    pthread_mutex_init(&lock->lock, NULL);
    pthread_cond_init(&lock->readers, NULL);
    pthread_cond_init(&lock->writers, NULL);
    lock->active_readers = 0;
    lock->active_writers = 0;
    lock->waiting_writers = 0;
}

void rwlock_acquire_readlock(rwlock_t *lock) {
    pthread_mutex_lock(&lock->lock);
    
    while (lock->active_writers > 0 || lock->waiting_writers > 0) {
        pthread_cond_wait(&lock->readers, &lock->lock);
    }
    
    lock->active_readers++;
    pthread_mutex_unlock(&lock->lock);
}

void rwlock_release_readlock(rwlock_t *lock) {
    pthread_mutex_lock(&lock->lock);
    lock->active_readers--;
    
    if (lock->active_readers == 0) {
        pthread_cond_signal(&lock->writers);
    }
    
    pthread_mutex_unlock(&lock->lock);
}

void rwlock_acquire_writelock(rwlock_t *lock) {
    pthread_mutex_lock(&lock->lock);
    
    lock->waiting_writers++;
    while (lock->active_readers > 0 || lock->active_writers > 0) {
        pthread_cond_wait(&lock->writers, &lock->lock);
    }
    lock->waiting_writers--;
    
    lock->active_writers++;
    pthread_mutex_unlock(&lock->lock);
}

void rwlock_release_writelock(rwlock_t *lock) {
    pthread_mutex_lock(&lock->lock);
    lock->active_writers--;
    
    // Wake up waiting writers first (writer preference)
    if (lock->waiting_writers > 0) {
        pthread_cond_signal(&lock->writers);
    } else {
        // No waiting writers, wake up all readers
        pthread_cond_broadcast(&lock->readers);
    }
    
    pthread_mutex_unlock(&lock->lock);
}

int read_loops;
int write_loops;
int counter = 0;

rwlock_t mutex;

void *reader(void *arg) {
    (void)arg;
    int i;
    int local = 0;
    for (i = 0; i < read_loops; i++) {
	rwlock_acquire_readlock(&mutex);
	local = counter;
	rwlock_release_readlock(&mutex);
	printf("read %d\n", local);
    }
    printf("read done: %d\n", local);
    return NULL;
}

void *writer(void *arg) {
    (void)arg;
    int i;
    for (i = 0; i < write_loops; i++) {
	rwlock_acquire_writelock(&mutex);
	counter++;
	rwlock_release_writelock(&mutex);
    }
    printf("write done\n");
    return NULL;
}
