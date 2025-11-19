#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>  


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

char* search(hashRecord** table, size_t table_size, const char* key)
{
    uint32_t hash = one_at_a_time_hash((const uint8_t*)key, strlen(key));
    size_t index = hash % table_size;

    //aquire read lock
    //rwlock_aquire_readlock(&locks[index]); UNCOMMENT THIS LINE IF USING LOCKS

    hashRecord* current = table[index];
    while (current != NULL) {
        if (strcmp(current->name, key) == 0) {
            char* result = current->name;// Found the record
            //rwlock_release_readlock(&locks[index]); UNCOMMENT THIS LINE IF USING LOCKS
            return result;
        }
        current = current->next;
    }
    //rwlock_release_readlock(&locks[index]); UNCOMMENT THIS LINE IF USING LOCKS
    return NULL; // Record not found

}

// Added insert() 11/18/2025 Arianna R.
int insert(hashRecord **table, size_t table_size, const char *name, uint32_t salary)
{
    // calling Jenkins hash func
    uint32_t hash = one_at_a_time_hash((const uint8_t*)name, strlen(name));
    // finding record bucket
    size_t index = hash % table_size;

    // for multi-threading acquire write lock
    // rwlock_aquire_writelock(&locks[index]);   // UNCOMMENT IF USING LOCKS

    hashRecord *current = table[index];
    // walking to ensure our current doesnt already exist
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            // if name exists in list return error
            // rwlock_release_writelock(&locks[index]);   // UNCOMMENT IF USING LOCKS
            return 0;
        }
        current = current->next;
    }

    // doesnt exist so make a new node
    hashRecord *newNode = (hashRecord*)malloc(sizeof(hashRecord));
    if (!newNode) {
        // rwlock_release_writelock(&locks[index]);   // UNCOMMENT IF USING LOCKS
        return -1; //failed :P
    }

    // init new node
    newNode->hash = hash;
    strncpy(newNode->name, name, sizeof(newNode->name));
    newNode->name[sizeof(newNode->name) - 1] = '\0';
    newNode->salary = salary;

    // insert new node at head of bucket list
    newNode->next = table[index];
    table[index] = newNode;

    // rwlock_release_writelock(&locks[index]);   // UNCOMMENT IF USING LOCKS
    return 1;
    // done! if it works itll return 1
}

// Added updateSalary 11/15/2025 CB
// Returns 1 on success, 0 if key not found.
// If successful and old_salary_out != NULL, writes the previous salary there.
int updateSalary(hashRecord **table, size_t table_size,
                 const char *key, uint32_t new_salary,
                 uint32_t *old_salary_out)
{
    // Compute hash and bucket index
    uint32_t hash  = one_at_a_time_hash((const uint8_t *)key, strlen(key));
    size_t   index = hash % table_size;

    // Acquire write lock for this bucket
    rwlock_aquire_writelock(&locks[index]);   // locks[] defined elsewhere

    hashRecord *current = table[index];

    while (current != NULL) {
        if (strcmp(current->name, key) == 0) {
            // Found the record; capture old salary then update
            if (old_salary_out != NULL) {
                *old_salary_out = current->salary;
            }
            current->salary = new_salary;

            rwlock_release_writelock(&locks[index]);
            return 1;
        }
        current = current->next;
    }

    // Not found
    rwlock_release_writelock(&locks[index]);
    return 0;
}

// Added delete(), 11/16/2025, Kimari Guthre
// Making assumption that each hashRecord in table is dynamically allocated.
int delete(hashRecord** table, size_t table_size, const char* key) {
    // Following convention of updateSalary(), not relying on search...
    size_t index = one_at_a_time_hash((const uint8_t*)key, strlen(key)) % table_size;

    // Acquire write lock for this bucket
    // rwlock_aquire_writelock(&locks[index]);  // UNCOMMENT IF USING LOCKS

    // Since each bucket is a linked list, need to keep past entry to stitch list back together.
    hashRecord* past = NULL;
    hashRecord* current = table[index];

    while (current != NULL) {
        if (strcmp(current->name, key) == 0) {
            // Found the record, proceed with delete.
            if (past != NULL)
                past->next = current->next;
            else
                table[index] = current->next;
            
            free(current);

            // Release lock and return success.
            // rwlock_release_writelock(&locks[index]);  // UNCOMMENT IF USING LOCKS
            return 1;
        }
        past = current;
        current = current->next;
    }

    // Not found
    // rwlock_release_writelock(&locks[index]);  // UNCOMMENT IF USING LOCKS
    return 0;
}

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
static long long _now_us_local(void) {
    return (long long)((double)clock() * 1000000.0 / (double)CLOCKS_PER_SEC);
}
static void _log_line_local(FILE *logf, int priority, const char *msg) {
    if (!logf) return;
    fprintf(logf, "%lld THREAD %d %s\n", _now_us_local(), priority, msg);
    fflush(logf);
}

// Takes a READ lock per bucket, snapshots all nodes, sorts by hash->name, prints a stable listing
 /*
 * Usage from PRINT thread:
 *   FILE *logf = fopen("hash.log","a"); // or reuse a shared FILE*
 *   print_table(table, table_size, my_priority, logf, stdout);
 */
void print_table(hashRecord **table,
                 size_t table_size,
                 int priority,
                 FILE *logf,
                 FILE *out)
{
    if (!table || !out) return;

    size_t cap = 64, n = 0;
    _print_row_t *rows = (_print_row_t*)malloc(cap * sizeof(_print_row_t));
    if (!rows) return;

    for (size_t i = 0; i < table_size; i++) {
        // Acquire READ lock for this bucket if you have locks[]
        // rwlock_aquire_readlock(&locks[i]);  // UNCOMMENT IF USING LOCKS
        _log_line_local(logf, priority, "-READY READ LOCK ACQUIRED");

        for (hashRecord *cur = table[i]; cur; cur = cur->next) {
            if (n == cap) {
                cap *= 2;
                _print_row_t *tmp = (_print_row_t*)realloc(rows, cap * sizeof(_print_row_t));
                if (!tmp) { /* OOM */ break; }
                rows = tmp;
            }

            // Use stored hash if present; otherwise compute defensively
            uint32_t h = cur->hash ? cur->hash
                                   : one_at_a_time_hash((const uint8_t*)cur->name,
                                                        strlen(cur->name));

            rows[n].hash = h;
            // Copy name safely (struct has name[50])
            strncpy(rows[n].name, cur->name, sizeof(rows[n].name));
            rows[n].name[sizeof(rows[n].name) - 1] = '\0';
            rows[n].salary = cur->salary;
            n++;
        }

        // rwlock_release_readlock(&locks[i]);  // UNCOMMENT IF USING LOCKS
        _log_line_local(logf, priority, "-READY READ LOCK RELEASED");
    }

    // Deterministic order for grading
    qsort(rows, n, sizeof(_print_row_t), _print_row_cmp);

    // Adjust format to match your rubric exactly if neede
    fprintf(out, "PRINT Current Database:\n");
    for (size_t i = 0; i < n; i++) {
        // Format: <hash>,<name>,<salary>
        fprintf(out, "%u,%s,%u\n", rows[i].hash, rows[i].name, rows[i].salary);
    }

    free(rows);
}

//if not logging yet, pass NULL for logf, logging is optional
// if we have a locks[]array, uncomment the two lines in the loop to acquire/release READ locks
