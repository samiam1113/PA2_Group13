#ifndef HASHFUNCTIONS_H
#define HASHFUNCTIONS_H
#include <stdint.h>
#include <stdio.h>

typedef struct hash_struct {
    uint32_t hash;
    char     name[50];
    uint32_t salary;
    struct hash_struct *next;
} hashRecord;

void     init_locks(size_t table_size);
void     destroy_locks(size_t table_size);

uint32_t one_at_a_time_hash(const uint8_t* key, size_t length);
int       insert(hashRecord **table, size_t table_size, const char *name, uint32_t salary, int priority, uint32_t *outputHash);
int       updateSalary(hashRecord **table, size_t table_size, const char *key, uint32_t new_salary, uint32_t *old_salary_out, int priority, uint32_t *outputHash);
int       deleteRecord(hashRecord **table, size_t table_size, const char *key, int priority, hashRecord *deletedRecord);
hashRecord*     search(hashRecord **table, size_t table_size, const char *key, int priority);
void      print_table(hashRecord **table, size_t table_size, int priority, FILE *out);

#endif
