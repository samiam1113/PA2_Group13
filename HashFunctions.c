#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/time.h>
//#include "rwlock.c"


// // Prototypes & globals provided by rwlock.c
// typedef struct _rwlock_t {
//     sem_t writelock;
//     sem_t lock;
//     int readers;
// } rwlock_t;

// void rwlock_init(rwlock_t *rw);
// void rwlock_acquire_readlock(rwlock_t *rw, int priority);
// void rwlock_release_readlock(rwlock_t *rw, int priority);
// void rwlock_acquire_writelock(rwlock_t *rw, int priority);
// void rwlock_release_writelock(rwlock_t *rw, int priority);
// void rwlock_destroy(rwlock_t *rw);

// ========== Timestamp and Logging ==========
long long current_timestamp() {
    struct timeval te;
    gettimeofday(&te, NULL);
    long long microseconds = (te.tv_sec * 1000000LL) + te.tv_usec;
    return microseconds;
}

// Global log file pointer - set this before operations
FILE *global_log = NULL;

// logger function
void log_message(int priority, const char *message) {
    if (!global_log) return;
    fprintf(global_log, "%lld: THREAD %d %s\n", current_timestamp(), priority, message);
    fflush(global_log);
}

// action logger function (like if you insert itll say that on hash.log)
void action_msg(int priority, const char *action, uint32_t hash, const char *name, uint32_t salary) {
    if (!global_log) return;
    fprintf(global_log, "%lld: THREAD %d %s %d %s %d\n", current_timestamp(), priority, action, hash, name, salary);
    fflush(global_log);
}

// ========== RW Lock Implementation (from OSTEP) ==========
typedef struct _rwlock_t {
    sem_t writelock;
    sem_t lock;
    int readers;
} rwlock_t;

void rwlock_init(rwlock_t *rw) {
    rw->readers = 0;
    sem_init(&rw->lock, 0, 1);      // Binary semaphore for protecting readers count
    sem_init(&rw->writelock, 0, 1); // Binary semaphore for write exclusion
}

void rwlock_acquire_readlock(rwlock_t *rw, int priority) {
    log_message(priority, "WAITING FOR READ LOCK");
    sem_wait(&rw->lock);
    rw->readers++;
    if (rw->readers == 1) // First reader acquires write lock
        sem_wait(&rw->writelock);
    sem_post(&rw->lock);
    log_message(priority, "READ LOCK ACQUIRED");
}

void rwlock_release_readlock(rwlock_t *rw, int priority) {
    sem_wait(&rw->lock);
    rw->readers--;
    if (rw->readers == 0) // Last reader releases write lock
        sem_post(&rw->writelock);
    sem_post(&rw->lock);
    log_message(priority, "READ LOCK RELEASED");
}

void rwlock_acquire_writelock(rwlock_t *rw, int priority) {
    log_message(priority, "WAITING FOR WRITE LOCK");
    sem_wait(&rw->writelock);
    log_message(priority, "WRITE LOCK ACQUIRED");
}

void rwlock_release_writelock(rwlock_t *rw, int priority) {
    sem_post(&rw->writelock);
    log_message(priority, "WRITE LOCK RELEASED");
}

void rwlock_destroy(rwlock_t *rw) {
    sem_destroy(&rw->lock);
    sem_destroy(&rw->writelock);
}

rwlock_t *locks = NULL; // Global array of locks for hash table buckets
typedef struct hash_struct
{
  uint32_t hash;
  char name[50];
  uint32_t salary;
  struct hash_struct *next;
} hashRecord;

//collect/sort snapshot
typedef struct {
    uint32_t hash;
    char     name[50];
    uint32_t salary;
} _print_row_t;

uint32_t one_at_a_time_hash(const uint8_t* key, size_t length)
{
    size_t i = 0;
    uint32_t hash = 0;
    while (i != length) {
        hash += key[i++];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

void init_locks(size_t table_size) {
    locks = (rwlock_t*)malloc(table_size * sizeof(rwlock_t));
    if (!locks) {
        fprintf(stderr, "Failed to allocate locks\n");
        exit(1);
    }
    for (size_t i = 0; i < table_size; i++) {
        rwlock_init(&locks[i]);
    }
}

void destroy_locks(size_t table_size) {
    if (locks) {
        for (size_t i = 0; i < table_size; i++) {
            rwlock_destroy(&locks[i]);
        }
        free(locks);
        locks = NULL;
    }
}


char* search(hashRecord** table, size_t table_size, const char* key, int priority)
{
    uint32_t hash = one_at_a_time_hash((const uint8_t*)key, strlen(key));
    size_t index = hash % table_size;

    //acquire read lock and log
    // action_msg(priority, "SEARCH" , hash, key, 0);
    rwlock_acquire_readlock(&locks[index], priority);

    hashRecord* current = table[index];
    //traverses and prints if found
    while (current != NULL) {
        if (strcmp(current->name, key) == 0) {
            //log here for salary
            action_msg(priority, "SEARCH" , hash, key, current->salary);
            printf("Found: %u,%s,%u\n", current->hash, current->name, current->salary);
            rwlock_release_readlock(&locks[index], priority);
            return current->name;
        }
        current = current->next;
    }

    //if not found
    printf("%s not found\n", key);

    rwlock_release_readlock(&locks[index], priority);
    return NULL;
}


// MARK: Insert
// Added insert() 11/18/2025 Arianna R.
int insert(hashRecord **table, size_t table_size, const char *name, uint32_t salary, int priority)
{
    uint32_t hash = one_at_a_time_hash((const uint8_t*)name, strlen(name));
    size_t index = hash % table_size;
    
    //acquire write lock and log
    action_msg(priority, "INSERT" , hash, name, salary);
    rwlock_acquire_writelock(&locks[index], priority);

    hashRecord *current = table[index];
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            rwlock_release_writelock(&locks[index], priority);
            return 0; // already exists
        }
        current = current->next;
    }

    //make a new node and init info
    hashRecord *newNode = (hashRecord*)malloc(sizeof(hashRecord));
    newNode->hash = hash;
    strncpy(newNode->name, name, 50);
    newNode->name[49] = '\0';
    newNode->salary = salary;

    newNode->next = table[index];
    table[index] = newNode;

    rwlock_release_writelock(&locks[index], priority);

    // Output as expected
    printf("Inserted %u,%s,%u\n", hash, name, salary);

    return 1;
}

// MARK: Update Salary
// Added updateSalary 11/15/2025 CB
// Returns 1 on success, 0 if key not found.
// If successful and old_salary_out != NULL, writes the previous salary there.
int updateSalary(hashRecord **table, size_t table_size,
                 const char *key, uint32_t new_salary,
                 uint32_t *old_salary_out, int priority)
{
    uint32_t hash = one_at_a_time_hash((const uint8_t *)key, strlen(key));
    size_t index = hash % table_size;

    //acquire write lock and log
    action_msg(priority, "UPDATE" , hash, key, new_salary);
    rwlock_acquire_writelock(&locks[index], priority);

    hashRecord *current = table[index];
    while (current != NULL) {
        if (strcmp(current->name, key) == 0) {

            uint32_t old = current->salary;
            if (old_salary_out) *old_salary_out = old;

            current->salary = new_salary;

            // Output as expected
            printf("Updated record %u from %u,%s,%u to %u,%s,%u\n",
                hash,
                hash, current->name, old,
                hash, current->name, new_salary
            );

            rwlock_release_writelock(&locks[index], priority);

            return 1;
        }
        current = current->next;
    }

    rwlock_release_writelock(&locks[index], priority);
    return 0;
}

// MARK: Delete
// Added delete(), 11/16/2025, Kimari Guthre
// Making assumption that each hashRecord in table is dynamically allocated.
int delete(hashRecord** table, size_t table_size, const char* key, int priority)
{
    uint32_t hash = one_at_a_time_hash((const uint8_t*)key, strlen(key));
    size_t index = hash % table_size;
    //acquire write lock
    rwlock_acquire_writelock(&locks[index], priority);

    hashRecord *prev = NULL;
    hashRecord *cur  = table[index];

    while (cur != NULL) {
        if (strcmp(cur->name, key) == 0) {

            if (prev == NULL) table[index] = cur->next;
            else prev->next = cur->next;
            //log here for proper salary
            action_msg(priority, "DELETE" , hash, key, cur->salary);
            printf("Deleted record for %u,%s,%u\n", cur->hash, cur->name, cur->salary);

            free(cur);
            rwlock_release_writelock(&locks[index], priority);
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }

    rwlock_release_writelock(&locks[index], priority);
    return 0;
}

// MARK: Print
//Added print functions: Jasmine Narayan 11/18
// Sort by hash, then by name for deterministic output
// static int _print_row_cmp(const void *a, const void *b) {
//     const _print_row_t *ra = (const _print_row_t*)a;
//     const _print_row_t *rb = (const _print_row_t*)b;
//     if (ra->hash < rb->hash) return -1;
//     if (ra->hash > rb->hash) return  1;
//     return strcmp(ra->name, rb->name);
// }

// // Minimal logger using clock() (avoid struct timeval)
//  static long long _now_us_local(void) {
//      return (long long)((double)clock() * 1000000.0 / (double)CLOCKS_PER_SEC);
//  }
//  static void _log_line_local(FILE *logf, int priority, const char *msg) {
//      if (!logf) return;
//      fprintf(logf, "%lld THREAD %d %s\n", _now_us_local(), priority, msg);
//      fflush(logf);
//  }

// // Takes a READ lock per bucket, snapshots all nodes, sorts by hash->name, prints a stable listing
//  /*
//  * Usage from PRINT thread:
//  *   FILE *logf = fopen("hash.log","a"); // or reuse a shared FILE*
//  *   print_table(table, table_size, my_priority, logf, stdout);
//  */
// void print_table(hashRecord **table,
//                  size_t table_size,
//                  int priority,
//                  FILE *logf,
//                  FILE *out)
// {
//     if (!table || !out) return;

//     size_t cap = 64, n = 0;
//     _print_row_t *rows = (_print_row_t*)malloc(cap * sizeof(_print_row_t));
//     if (!rows) return;

//     for (size_t i = 0; i < table_size; i++) {
//         // Acquire READ lock for this bucket if you have locks[]
//         rwlock_acquire_readlock(&locks[i], priority);
//         _log_line_local(logf, priority, "READ LOCK ACQUIRED");

//         for (hashRecord *cur = table[i]; cur; cur = cur->next) {
//             if (n == cap) {
//                 cap *= 2;
//                 _print_row_t *tmp = (_print_row_t*)realloc(rows, cap * sizeof(_print_row_t));
//                 if (!tmp) { /* OOM */ break; }
//                 rows = tmp;
//             }

//             // Use stored hash if present; otherwise compute defensively
//             uint32_t h = cur->hash ? cur->hash
//                                    : one_at_a_time_hash((const uint8_t*)cur->name,
//                                                         strlen(cur->name));

//             rows[n].hash = h;
//             // Copy name safely (struct has name[50])
//             strncpy(rows[n].name, cur->name, sizeof(rows[n].name));
//             rows[n].name[sizeof(rows[n].name) - 1] = '\0';
//             rows[n].salary = cur->salary;
//             n++;
//         }

//         rwlock_release_readlock(&locks[i], priority);
//         _log_line_local(logf, priority, "READ LOCK RELEASED");
//     }

//     // ordering
//     qsort(rows, n, sizeof(_print_row_t), _print_row_cmp);

//     fprintf(out, "Current Database:\n");
//     for (size_t i = 0; i < n; i++) {
//         fprintf(out, "%u,%s,%u\n", rows[i].hash, rows[i].name, rows[i].salary);
//     }

//     free(rows);
// }

// helper function for print_table
int compare_rows(const void *a, const void *b) {
    const _print_row_t *rowA = (const _print_row_t*)a;
    const _print_row_t *rowB = (const _print_row_t*)b;
    if (rowA->hash < rowB->hash) return -1;
    else if (rowA->hash > rowB->hash) return 1;
    else return 0;
}

void print_table(hashRecord **table, size_t table_size, int priority) {
    // Acquire read locks for all buckets
    for (size_t i = 0; i < table_size; i++)
        rwlock_acquire_readlock(&locks[i], priority);

    action_msg(priority, "PRINT TABLE", 0, "", 0);

    // Count total records
    size_t total_records = 0;
    for (size_t i = 0; i < table_size; i++) {
        hashRecord *cur = table[i];
        while (cur) {
            total_records++;
            cur = cur->next;
        }
    }

    if (total_records == 0) {
        printf("Current Database:\n<empty>\n");
        for (size_t i = 0; i < table_size; i++)
            rwlock_release_readlock(&locks[i], priority);
        return;
    }

    // Collect all records in a temporary array
    _print_row_t *rows = malloc(total_records * sizeof(_print_row_t));
    if (!rows) {
        fprintf(stderr, "Failed to allocate memory for print_table\n");
        for (size_t i = 0; i < table_size; i++)
            rwlock_release_readlock(&locks[i], priority);
        return;
    }

    size_t idx = 0;
    for (size_t i = 0; i < table_size; i++) {
        hashRecord *cur = table[i];
        while (cur) {
            rows[idx].hash = cur->hash;
            strncpy(rows[idx].name, cur->name, sizeof(rows[idx].name));
            rows[idx].name[sizeof(rows[idx].name)-1] = '\0';
            rows[idx].salary = cur->salary;
            idx++;
            cur = cur->next;
        }
    }

    // Release all locks before printing
    for (size_t i = 0; i < table_size; i++)
        rwlock_release_readlock(&locks[i], priority);

    // Sort by hash as expected
    qsort(rows, total_records, sizeof(_print_row_t),
          (int(*)(const void*, const void*))compare_rows);

    // Print everything in database
    printf("Current Database:\n");
    for (size_t i = 0; i < total_records; i++)
        printf("%u,%s,%u\n", rows[i].hash, rows[i].name, rows[i].salary);

    free(rows);
}

