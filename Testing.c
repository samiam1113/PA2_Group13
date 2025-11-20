// tests_unit.c — direct tests for insert/update/delete/search
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

// ---- types & prototypes from HashFunctions.c ----
typedef struct hash_struct {
  uint32_t hash; char name[50]; uint32_t salary; struct hash_struct *next;
} hashRecord;

uint32_t one_at_a_time_hash(const uint8_t* key, size_t length);
int       insert(hashRecord **table, size_t table_size, const char *name, uint32_t salary);
int       updateSalary(hashRecord **table, size_t table_size, const char *key, uint32_t new_salary, uint32_t *old_salary_out);
int       delete(hashRecord **table, size_t table_size, const char *key);
char*     search(hashRecord **table, size_t table_size, const char *key);
void      print_table(hashRecord **table, size_t table_size, int priority, FILE *logf, FILE *out);

// ---- (optional) stubs to satisfy locks if they exist in HashFunctions.c ----
typedef struct { int _dummy; } rwlock_t;
extern rwlock_t *locks;

// helper: bucket append (only if you want manual seeding; we’ll use insert() here)
static void free_all(hashRecord **table, size_t table_size) {
    for (size_t i=0;i<table_size;i++) {
        hashRecord *cur = table[i];
        while (cur) { hashRecord *n=cur->next; free(cur); cur=n; }
    }
    free(table);
}

#define CHECK(msg,cond)  do{ printf("[%-6s] %s\n", (cond)?"PASS":"FAIL", msg); }while(0)

int main(void) {
    size_t table_size = 8;
    hashRecord **table = (hashRecord**)calloc(table_size, sizeof(hashRecord*));
    locks = (rwlock_t*)calloc(table_size, sizeof(rwlock_t)); // ok if stubs, harmless otherwise

    // 1) insert
    CHECK("insert new A", insert(table, table_size, "Alice", 1000) == 1);
    CHECK("insert new B", insert(table, table_size, "Bob",   2000) == 1);
    CHECK("insert duplicate Alice -> 0", insert(table, table_size, "Alice", 1234) == 0);

    // 2) search
    CHECK("search Alice", search(table, table_size, "Alice") != NULL);
    CHECK("search Carol (missing)", search(table, table_size, "Carol") == NULL);

    // 3) update
    uint32_t old=0;
    CHECK("update Alice -> 1500", updateSalary(table, table_size, "Alice", 1500, &old) == 1);
    CHECK("update recorded old salary", old == 1000);
    CHECK("update missing Carol -> 0", updateSalary(table, table_size, "Carol", 3333, &old) == 0);

    // 4) delete
    CHECK("delete Bob", delete(table, table_size, "Bob") == 1);
    CHECK("delete Bob again -> 0", delete(table, table_size, "Bob") == 0);

    // 5) final print (should show only Alice with updated salary)
    print_table(table, table_size, /*priority=*/1, /*logf=*/NULL, stdout);

    free_all(table, table_size);
    free(locks); locks=NULL;
    return 0;
}
