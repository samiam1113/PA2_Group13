// chash.c — threaded driver for PA#2
// Build:  make
// Run:    ./chash [commands-file]   (defaults to commands.txt)

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <ctype.h>
#include "HashFunctions.h"
#include "locks.h"
#include <stdbool.h>

#ifdef _WIN32
#include <direct.h>
#endif

extern FILE *global_log;

// ---------- start-order control (CV) ----------
static pthread_mutex_t turn_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  turn_cv = PTHREAD_COND_INITIALIZER;
static int current_turn = 0;

// ---------- command parsing ----------
typedef enum { CMD_THREADS, CMD_INSERT, CMD_UPDATE, CMD_DELETE, CMD_SEARCH, CMD_PRINT, CMD_UNKNOWN } cmd_t;

typedef struct {
    cmd_t type;
    int   turn;          // file order
    int   priority;      // last field
    char  name[80];      // when relevant
    uint32_t value;      // salary when relevant
} task_t;

static cmd_t parse_cmd(const char *s) {
    if (!s) return CMD_UNKNOWN;
    if (!strcmp(s,"threads")) return CMD_THREADS;
    if (!strcmp(s,"insert"))  return CMD_INSERT;
    if (!strcmp(s,"update"))  return CMD_UPDATE;
    if (!strcmp(s,"delete"))  return CMD_DELETE;
    if (!strcmp(s,"search"))  return CMD_SEARCH;
    if (!strcmp(s,"print"))   return CMD_PRINT;
    return CMD_UNKNOWN;
}

static void trim(char *s){
    size_t n=strlen(s);
    while (n && (s[n-1]=='\r'||s[n-1]=='\n'||s[n-1]==' '||s[n-1]=='\t')) s[--n]='\0';
}

static int split4(char *line, char **a, char **b, char **c, char **d){
    *a = strtok(line, ",");
    *b = strtok(NULL, ",");
    *c = strtok(NULL, ",");
    *d = strtok(NULL, ",");
    return (*a!=NULL);
}

static char* my_strdup(const char* s){
    size_t n = strlen(s)+1;
    char *p = (char*)malloc(n);
    if (p) memcpy(p,s,n);
    return p;
}

// ---------- worker ----------
typedef struct {
    task_t      t;
    hashRecord **table;
    size_t       table_sz;
} arg_t;

static void* worker(void *vp) {
    arg_t *a = (arg_t*)vp;
    task_t t = a->t;

    // deterministic start: wait for our turn, then immediately signal next
    pthread_mutex_lock(&turn_mu);
    while (current_turn != t.turn)
        pthread_cond_wait(&turn_cv, &turn_mu);
    current_turn++;
    pthread_cond_broadcast(&turn_cv);
    pthread_mutex_unlock(&turn_mu);

    // execute
    switch (t.type) {
        case CMD_INSERT: {
            uint32_t outputHash = 0;
            int rc = insert(a->table, a->table_sz, t.name, t.value, t.priority, &outputHash);
            if (rc == 1)
                printf("Inserted %u,%s,%d\n", outputHash, t.name, t.value);
            else
                printf(rc==0 ? "Insert failed, entry exists\n" : "Insert failed, new node could not be made\n");
        } break;
        case CMD_UPDATE: {
            uint32_t old=0;
            uint32_t outputHash = 0;
            int rc = updateSalary(a->table, a->table_sz, t.name, t.value, &old, t.priority, &outputHash);
            if (rc == 1) 
                printf("Updated record %u from %u,%s,%u to %u,%s,%u\n", outputHash, outputHash, t.name, old, outputHash, t.name, t.value);
            else
                printf("%s not found.\n", t.name);
        } break;
        case CMD_DELETE: {
            hashRecord deletedRecord;
            int rc = deleteRecord(a->table, a->table_sz, t.name, t.priority, &deletedRecord);
            if (rc == 1)
                printf("Deleted record for %u,%s,%u\n", deletedRecord.hash, deletedRecord.name, deletedRecord.salary);
            else
                printf("%s not found.\n", t.name);
        } break;
        case CMD_SEARCH: {
            hashRecord *p = search(a->table, a->table_sz, t.name, t.priority);
            if (p) printf("Found: %u,%s,%d\n", p->hash, p->name, p->salary);
            else   printf("%s not found.\n", t.name);
        } break;
        case CMD_PRINT: {
            print_table(a->table, a->table_sz, t.priority, stdout);
        } break;
        default: break;
    }
    return NULL;
}

// Comparison function for sorting final table by hash
static int cmp_by_hash(const void *a, const void *b) {
    const hashRecord *ra = *(const hashRecord **)a;
    const hashRecord *rb = *(const hashRecord **)b;
    if (ra->hash < rb->hash) return -1;
    if (ra->hash > rb->hash) return 1;
    return strcmp(ra->name, rb->name);
}

// ---------- main ----------
int main(int argc, char **argv) {
    const char *CMD_FILE = (argc > 1) ? argv[1] : "commands.txt";
    FILE *f = fopen(CMD_FILE, "r");
    if (!f) {
#ifdef _WIN32
        char cwd[1024]; _getcwd(cwd, sizeof(cwd));
        fprintf(stderr, "couldn't open commands file '%s'\nCWD: %s\n", CMD_FILE, cwd);
#endif
        perror(CMD_FILE);
        return 1;
    }

    // table + locks + log
    size_t table_size = 64;
    hashRecord **table = (hashRecord**)calloc(table_size, sizeof(hashRecord*));
    if (!table) { perror("calloc table"); fclose(f); return 1; }
    init_locks(table_size);

    global_log = fopen("hash.log","w");

    // parse commands
   task_t *tasks=NULL; size_t n=0, cap=64;
   tasks = (task_t*)calloc(cap, sizeof(task_t));

    char line[512];
    while (fgets(line,sizeof(line),f)) {
        trim(line);
        if (!line[0] || line[0]=='#') continue;

        char *dup = my_strdup(line);
        char *a,*b,*c,*d; split4(dup,&a,&b,&c,&d);
        cmd_t type = parse_cmd(a);
        if (type==CMD_UNKNOWN) { free(dup); continue; }

        if (n==cap) { cap*=2; tasks=(task_t*)realloc(tasks,cap*sizeof(task_t)); }
        tasks[n].type = type;
        tasks[n].turn = (int)n;
        tasks[n].priority = d ? atoi(d) : 0;
        tasks[n].name[0] = '\0';
        tasks[n].value = 0;

        if (type==CMD_INSERT || type==CMD_UPDATE || type==CMD_DELETE || type==CMD_SEARCH) {
            if (b) strncpy(tasks[n].name, b, sizeof(tasks[n].name)-1);
        }
        if (type==CMD_INSERT || type==CMD_UPDATE) {
            tasks[n].value = (uint32_t)strtoul(c?c:"0",NULL,10);
        }

        free(dup); n++;
    }
    fclose(f);

    // create threads
    pthread_t *ths = (pthread_t*)calloc(n,sizeof(pthread_t));
    arg_t     *args= (arg_t*)calloc(n,sizeof(arg_t));
    for (size_t i=0;i<n;i++) {
        args[i].t = tasks[i]; args[i].table = table; args[i].table_sz = table_size;
        pthread_create(&ths[i], NULL, worker, &args[i]);
    }
    for (size_t i=0;i<n;i++) pthread_join(ths[i], NULL);
    
    // Always do a final PRINT per assignment spec (even if the last command was PRINT)
    print_table(table, table_size, -1, stdout);
    fflush(stdout);

    // Print statistics and final sorted table to hash.log
    if (global_log) {
        fprintf(global_log, "Number of lock acquisitions: %d\n", get_total_acquisitions());
        fprintf(global_log, "Number of lock releases: %d\n", get_total_releases());
        fprintf(global_log, "Final Table:\n");
        
        // Collect all records
        hashRecord **all_records = NULL;
        size_t record_count = 0, record_cap = 64;
        all_records = (hashRecord**)malloc(record_cap * sizeof(hashRecord*));
        
        for (size_t i = 0; i < table_size; i++) {
            hashRecord *cur = table[i];
            while (cur) {
                if (record_count == record_cap) {
                    record_cap *= 2;
                    all_records = (hashRecord**)realloc(all_records, record_cap * sizeof(hashRecord*));
                }
                all_records[record_count++] = cur;
                cur = cur->next;
            }
        }
        
        // Sort by hash
        qsort(all_records, record_count, sizeof(hashRecord*), cmp_by_hash);
        
        // Print sorted records
        for (size_t i = 0; i < record_count; i++) {
            fprintf(global_log, "%u,%s,%u\n", 
                    all_records[i]->hash, 
                    all_records[i]->name, 
                    all_records[i]->salary);
        }
        
        free(all_records);
        fclose(global_log);
        global_log = NULL;
    }

    destroy_locks(table_size);

    // free table
    for (size_t i=0;i<table_size;i++){
        hashRecord *cur=table[i];
        while (cur){ hashRecord *nx=cur->next; free(cur); cur=nx; }
    }
    free(table); free(tasks); free(ths); free(args);
    return 0;
}
