// tests_unit.c — direct tests for insert/update/delete/search
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "HashFunctions.h"

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

    uint32_t dummyHash;
    // 1) insert
    CHECK("insert new A", insert(table, table_size, "Alice", 1000, 1, &dummyHash) == 1);
    CHECK("insert new B", insert(table, table_size, "Bob",   2000, 2, &dummyHash) == 1);
    CHECK("insert duplicate Alice -> 0", insert(table, table_size, "Alice", 1234, 3, &dummyHash) == 0);

    // 2) search
    CHECK("search Alice", search(table, table_size, "Alice", 4) != NULL);
    CHECK("search Carol (missing)", search(table, table_size, "Carol", 5) == NULL);

    // 3) update
    uint32_t old=0;
    CHECK("update Alice -> 1500", updateSalary(table, table_size, "Alice", 1500, &old, 6, &dummyHash) == 1);
    CHECK("update recorded old salary", old == 1000);
    CHECK("update missing Carol -> 0", updateSalary(table, table_size, "Carol", 3333, &old, 7, &dummyHash) == 0);

    hashRecord dummyRecord;
    // 4) delete
    CHECK("delete Bob", deleteRecord(table, table_size, "Bob", 8, &dummyRecord) == 1);
    CHECK("delete Bob again -> 0", deleteRecord(table, table_size, "Bob", 9, &dummyRecord) == 0);

    // 5) final print (should show only Alice with updated salary)
    print_table(table, table_size, /*priority=*/1, stdout);

    free_all(table, table_size);
    free(locks); locks=NULL;
    return 0;
}
