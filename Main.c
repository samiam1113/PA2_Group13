// main.c — tiny harness to test PRINT only
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

// ---- Types & function prototypes from HashFunctions.c ----
typedef struct hash_struct {
    uint32_t hash;
    char     name[50];
    uint32_t salary;
    struct hash_struct *next;
} hashRecord;

uint32_t one_at_a_time_hash(const uint8_t* key, size_t length);
void print_table(hashRecord **table, size_t table_size, int priority, FILE *logf, FILE *out);

// ---- TEMP rwlock stub hook (matches stubs you added in HashFunctions.c) ----
typedef struct { int _dummy; } rwlock_t;
extern rwlock_t *locks;

// ---------- helper to append a record directly to the correct bucket ----------
static void append_direct(hashRecord **table, size_t table_size,
                          const char *name, uint32_t salary)
{
    uint32_t h   = one_at_a_time_hash((const uint8_t*)name, strlen(name));
    size_t   idx = h % table_size;

    hashRecord *n = (hashRecord*)calloc(1, sizeof(hashRecord));
    n->hash = h;
    strncpy(n->name, name, sizeof(n->name));
    n->name[sizeof(n->name)-1] = '\0';
    n->salary = salary;

    n->next = table[idx];   // push-front into bucket
    table[idx] = n;
}

int main(void) {
    // 1) create table
    size_t table_size = 8; // small test table
    hashRecord **table = (hashRecord**)calloc(table_size, sizeof(hashRecord*));
    if (!table) { perror("calloc table"); return 1; }

    // 2) allocate the (stub) locks array so references link cleanly
    locks = (rwlock_t*)calloc(table_size, sizeof(rwlock_t));
    if (!locks) { perror("calloc locks"); free(table); return 1; }

    // 3) insert a few rows
    append_direct(table, table_size, "Shigeru Miyamoto", 85000);
    append_direct(table, table_size, "Hideo Kojima",     90000);
    append_direct(table, table_size, "Gabe Newell",      88000);
    append_direct(table, table_size, "Todd Howard",      77000);
    append_direct(table, table_size, "Koji Kondo",       86000);

    // 4) run PRINT (with a log file; pass NULL instead of logf to disable)
    FILE *logf = fopen("hash.log", "w");
    print_table(table, table_size, /*priority=*/5, logf, stdout);
    if (logf) fclose(logf);

    // 5) cleanup
    for (size_t i = 0; i < table_size; i++) {
        hashRecord *cur = table[i];
        while (cur) { hashRecord *n = cur->next; free(cur); cur = n; }
    }
    free(table);
    free(locks);
    locks = NULL;

    return 0;
}
