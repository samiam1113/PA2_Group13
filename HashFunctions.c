#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include "HashFunctions.h"
#include "locks.h"

// ========== Timestamp and Logging ==========
long long current_timestamp() {
    struct timeval te;
    gettimeofday(&te, NULL);
    long long microseconds = (te.tv_sec * 1000000LL) + te.tv_usec;
    return microseconds;
}

rwlock_t *locks = NULL; // Global array of locks for hash table buckets

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


hashRecord* search(hashRecord** table, size_t table_size, const char* key, int priority)
{
    uint32_t hash = one_at_a_time_hash((const uint8_t*)key, strlen(key));
    size_t index = hash % table_size;

    // Acquire read lock
    rwlock_acquire_readlock(&locks[index], priority);

    char log_buf[256];
    snprintf(log_buf, sizeof(log_buf), "SEARCH,%u,%s", hash, key);
    log_message(priority, log_buf);

    hashRecord* current = table[index];
    while (current != NULL) {
        if (strcmp(current->name, key) == 0) {
             // Found the record
            rwlock_release_readlock(&locks[index], priority);
            return current;
        }
        current = current->next;
    }
    rwlock_release_readlock(&locks[index], priority);

    return NULL; // Record not found
}
// MARK: Insert
// Added insert() 11/18/2025 Arianna R.
int insert(hashRecord **table, size_t table_size, const char *name, uint32_t salary, int priority, uint32_t *outputHash)
{
    // Calling Jenkins hash func
    uint32_t hash = one_at_a_time_hash((const uint8_t*)name, strlen(name));
    // Finding record bucket
    size_t index = hash % table_size;

    // For multi-threading acquire write lock
    rwlock_acquire_writelock(&locks[index], priority);

    char log_buf[256];
    snprintf(log_buf, sizeof(log_buf), "INSERT,%u,%s,%u", hash, name, salary);
    log_message(priority, log_buf);

    hashRecord *current = table[index];
    // Walking to ensure our current doesn't already exist
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            // If name exists in list return error
            rwlock_release_writelock(&locks[index], priority);
            return 0;
        }
        current = current->next;
    }

    // Doesn't exist so make a new node
    hashRecord *newNode = (hashRecord*)malloc(sizeof(hashRecord));
    if (!newNode) {
        rwlock_release_writelock(&locks[index], priority);
        return -1; // Failed
    }

    // Init new node
    newNode->hash = hash;
    strncpy(newNode->name, name, sizeof(newNode->name));
    newNode->name[sizeof(newNode->name) - 1] = '\0';
    newNode->salary = salary;

    // Insert new node at head of bucket list
    newNode->next = table[index];
    table[index] = newNode;

    rwlock_release_writelock(&locks[index], priority);
    *outputHash = hash;
    return 1;
}

// MARK: Update Salary
// Added updateSalary 11/15/2025 CB
// Returns 1 on success, 0 if key not found.
// If successful and old_salary_out != NULL, writes the previous salary there.
int updateSalary(hashRecord **table, size_t table_size,
                 const char *key, uint32_t new_salary,
                 uint32_t *old_salary_out, int priority,
                 uint32_t *outputHash)
{
    // Compute hash and bucket index
    uint32_t hash  = one_at_a_time_hash((const uint8_t *)key, strlen(key));
    size_t   index = hash % table_size;

    // Acquire write lock for this bucket
    rwlock_acquire_writelock(&locks[index], priority);

    char log_buf[256];
    snprintf(log_buf, sizeof(log_buf), "UPDATE,%u,%s,%u", hash, key, new_salary);
    log_message(priority, log_buf);

    hashRecord *current = table[index];

    while (current != NULL) {
        if (strcmp(current->name, key) == 0) {
            // Found the record; capture old salary then update
            if (old_salary_out != NULL) {
                *old_salary_out = current->salary;
            }
            current->salary = new_salary;

            rwlock_release_writelock(&locks[index], priority);
            *outputHash = hash;
            return 1;
        }
        current = current->next;
    }

    // Not found
    rwlock_release_writelock(&locks[index], priority);
    return 0;
}

// MARK: Delete
// Added deleteRecord(), 11/16/2025, Kimari Guthre
// Making assumption that each hashRecord in table is dynamically allocated.
int deleteRecord(hashRecord** table, size_t table_size, const char* key, int priority, hashRecord* deletedRecord) {
    // Following convention of updateSalary(), not relying on search...
    uint32_t hash = one_at_a_time_hash((const uint8_t*)key, strlen(key));
    size_t index = hash % table_size;

    // Acquire write lock for this bucket
    rwlock_acquire_writelock(&locks[index], priority);

    char log_buf[256];
    snprintf(log_buf, sizeof(log_buf), "DELETE,%u,%s", hash, key);
    log_message(priority, log_buf);

    // Since each bucket is a linked list, need to keep past entry to stitch list back together.
    hashRecord* past = NULL;
    hashRecord* current = table[index];

    while (current != NULL) {
        if (strcmp(current->name, key) == 0) {
            // Copy record information to deletedRecord for printing.
            deletedRecord->hash = current->hash;
            strcpy(deletedRecord->name, current->name);
            deletedRecord->salary = current->salary;

            // Found the record, proceed with delete.
            if (past != NULL)
                past->next = current->next;
            else
                table[index] = current->next;
            
            free(current);

            // Release lock and return success.
            rwlock_release_writelock(&locks[index], priority);
            return 1;
        }
        past = current;
        current = current->next;
    }

    // Not found
    rwlock_release_writelock(&locks[index], priority);
    return 0;
}

// MARK: Print
//Added print functions: Jasmine Narayan 11/18
// Sort by hash, then by name for deterministic output
static int _print_row_cmp(const void *a, const void *b) {
    const _print_row_t *ra = (const _print_row_t*)a;
    const _print_row_t *rb = (const _print_row_t*)b;
    if (ra->hash < rb->hash) return -1;
    if (ra->hash > rb->hash) return  1;
    return strcmp(ra->name, rb->name);
}

// Minimal logger using clock() (avoid struct timeval)
/*static long long _now_us_local(void) {
    return (long long)((double)clock() * 1000000.0 / (double)CLOCKS_PER_SEC);
}
static void _log_line_local(FILE *logf, int priority, const char *msg) {
    if (!logf) return;
    fprintf(logf, "%lld THREAD %d %s\n", _now_us_local(), priority, msg);
    fflush(logf);
}*/

// Takes a READ lock per bucket, snapshots all nodes, sorts by hash->name, prints a stable listing
 /*
 * Usage from PRINT thread:
 *   FILE *logf = fopen("hash.log","a"); // or reuse a shared FILE*
 *   print_table(table, table_size, my_priority, logf, stdout);
 */
void print_table(hashRecord **table,
                 size_t table_size,
                 int priority,
                 FILE *out)
{
    if (!table || !out) return;

    // ---- Acquire ALL bucket read locks (silently) in ascending order ----
    for (size_t i = 0; i < table_size; i++) {
        rwlock_acquire_readlock(&locks[i], -1);  // -1 => no per-bucket log
    }
    // Single consolidated log entry for the grader:
    log_message(priority, "READ LOCK ACQUIRED (ALL BUCKETS)");

    // ---- Snapshot while fully read-locked ----
    size_t cap = 64, n = 0;
    _print_row_t *rows = (_print_row_t*)malloc(cap * sizeof(_print_row_t));
    if (!rows) {
        // Release locks before returning
        for (size_t i = table_size; i-- > 0; ) rwlock_release_readlock(&locks[i], -1);
        log_message(priority, "READ LOCK RELEASED (ALL BUCKETS)");
        return;
    }

    for (size_t i = 0; i < table_size; i++) {
        for (hashRecord *cur = table[i]; cur; cur = cur->next) {
            if (n == cap) {
                cap *= 2;
                _print_row_t *tmp = (_print_row_t*)realloc(rows, cap * sizeof(_print_row_t));
                if (!tmp) { free(rows); rows=NULL; break; }
                rows = tmp;
            }
            uint32_t h = cur->hash ? cur->hash
                                   : one_at_a_time_hash((const uint8_t*)cur->name,
                                                        strlen(cur->name));
            rows[n].hash = h;
            strncpy(rows[n].name, cur->name, sizeof(rows[n].name));
            rows[n].name[sizeof(rows[n].name) - 1] = '\0';
            rows[n].salary = cur->salary;
            n++;
        }
        if (!rows) break;
    }

    // ---- Release ALL read locks (silently) in reverse order ----
    for (size_t i = table_size; i-- > 0; ) {
        rwlock_release_readlock(&locks[i], -1);  // -1 => no per-bucket log
    }
    log_message(priority, "READ LOCK RELEASED (ALL BUCKETS)");

    if (!rows) return;

    // ---- Deterministic print ----
    qsort(rows, n, sizeof(_print_row_t), _print_row_cmp);
    fprintf(out, "Current Database:\n");
    for (size_t i = 0; i < n; i++)
        fprintf(out, "%u,%s,%u\n", rows[i].hash, rows[i].name, rows[i].salary);

    free(rows);
}


//if not logging yet, pass NULL for logf, logging is optional
// if we have a locks[]array, uncomment the two lines in the loop to acquire/release READ locks
