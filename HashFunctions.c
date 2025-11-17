#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


typedef struct hash_struct
{
  uint32_t hash;
  char name[50];
  uint32_t salary;
  struct hash_struct *next;
} hashRecord;

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

// Added updateSalary 11/15/2025 CB
int updateSalary(hashRecord** table, size_t table_size,
                 const char* key, uint32_t new_salary)
{
    uint32_t hash = one_at_a_time_hash((const uint8_t*)key, strlen(key));
    size_t index = hash % table_size;

    // Acquire write lock for this bucket
    // rwlock_aquire_writelock(&locks[index]);  // UNCOMMENT IF USING LOCKS

    hashRecord* current = table[index];

    while (current != NULL) {
        if (strcmp(current->name, key) == 0) {
            // Found the record; update salary
            current->salary = new_salary;

            // Release write lock and return success
            // rwlock_release_writelock(&locks[index]);  // UNCOMMENT IF USING LOCKS
            return 1;
        }
        current = current->next;
    }

    // Not found
    // rwlock_release_writelock(&locks[index]);  // UNCOMMENT IF USING LOCKS
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
