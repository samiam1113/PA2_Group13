#ifndef HASH_TABLE_H
#define HASH_TABLE_H
#include <stdio.h>
#include <stdlib.h>
#include "rwlock.h"
#include <stdint.h>
#include "hash_logger.h"

typedef struct hash_struct {
    uint32_t hash;
    char name[50];
    uint32_t salary;
    struct hash_struct *next;
} hashRecord;

typedef struct {
    hashRecord **table;
    size_t size;
    rwlock_t *locks;
    pthread_rwlock_t lock;
} hashTable;

uint32_t one_at_a_time_hash(const uint8_t* key, size_t length);
void initTable(hashTable *t);
void destroyTable(hashTable *t);
int insert(hashRecord **table, size_t table_size, const char *name, uint32_t salary, int priority, rwlock_t *locks);
char* search(hashRecord** table, size_t table_size, const char* key, rwlock_t *locks, int priority);
int updateSalary(hashRecord** table, size_t table_size, const char* key, uint32_t new_salary, rwlock_t *locks, int priority);
int delete(hashRecord** table, size_t table_size, const char* key, rwlock_t *locks, int priority);
void print_table(hashRecord **table, size_t table_size, int priority, FILE *logf, FILE *out, rwlock_t *locks);

#endif
