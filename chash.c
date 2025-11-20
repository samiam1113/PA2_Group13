// chash.c — threaded driver for PA#2
// Build:  make          (see Makefile below)
// Run:    ./chash [commands-file]   (defaults to commands.txt)

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <ctype.h>

#ifdef _WIN32
#include <direct.h>
#endif

// ----- prototypes & globals provided by HashFunctions.c -----
typedef struct hash_struct {
    uint32_t hash;
    char     name[50];
    uint32_t salary;
    struct hash_struct *next;
} hashRecord;

extern FILE *global_log;
void     init_locks(size_t table_size);
void     destroy_locks(size_t table_size);

uint32_t one_at_a_time_hash(const uint8_t* key, size_t length);
int       insert(hashRecord **table, size_t table_size, const char *name, uint32_t salary, int priority);
int       updateSalary(hashRecord **table, size_t table_size, const char *key, uint32_t new_salary, uint32_t *old_salary_out, int priority);
int       delete(hashRecord **table, size_t table_size, const char *key, int priority);
char*     search(hashRecord **table, size_t table_size, const char *key, int priority);
void      print_table(hashRecord **table, size_t table_size, int priority, FILE *logf, FILE *out);
int       compare_rows(const void *a, const void *b);

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
    // switch (t.type) {
    //     case CMD_INSERT: {
    //         int rc = insert(a->table, a->table_sz, t.name, t.value, t.priority);
    //         //printf("INSERTED %s %s\n", t.name, rc==1 ? "Inserted" : (rc==0 ? "Exists" : "Failed"));
    //         printf("INSERTED %d, %s, %d\n", one_at_a_time_hash((const uint8_t*)t.name, strlen(t.name)), t.name, t.value);
    //     } break;
    //     case CMD_UPDATE: {
    //         uint32_t old=0;
    //         int rc = updateSalary(a->table, a->table_sz, t.name, t.value, &old, t.priority);
    //         // if (rc==1) printf("UPDATE %s from %u to %u\n", t.name, old, t.value);
    //         // else       printf("UPDATE %s not found\n", t.name);
    //         printf("UPDATED RECORD %u from %u,%s,%u to %u, %s, %u\n", 
    //             one_at_a_time_hash((const uint8_t*)t.name, strlen(t.name)),
    //             one_at_a_time_hash((const uint8_t*)t.name, strlen(t.name)), t.name, old,
    //             one_at_a_time_hash((const uint8_t*)t.name, strlen(t.name)), t.name, t.value
    //         );
    //         //else       printf("UPDATE %s not found\n", t.name);
    //     } break;
    //     case CMD_DELETE: {
    //         int rc = delete(a->table, a->table_sz, t.name, t.priority);
    //         //printf("DELETE %s %s\n", t.name, rc==1 ? "Deleted" : "Not found");
    //         printf("DELETED %u, %s,\n", one_at_a_time_hash((const uint8_t*)t.name, strlen(t.name)), t.name);
    //         //else       printf("DELETE %s not found\n", t.name);
    //     } break;
    //     case CMD_SEARCH: {
    //         char *p = search(a->table, a->table_sz, t.name, t.priority);
    //         // if (p) printf("SEARCH Found: %s\n", p);
    //         // else   printf("SEARCH %s not found\n", t.name);
    //         // if(p)
    //         // printf("FOUND: %u, %s\n", one_at_a_time_hash((const uint8_t*)t.name, strlen(t.name)), p);
    //         // else   printf("SEARCH %s not found\n", t.name);
    //     } break;
    //     case CMD_PRINT: {
    //         //printf("Current Database:\n");
    //         print_table(a->table, a->table_sz, t.priority, global_log, stdout);
    //         printf("PRINT completed\n");
    //     } break;
    //     default: break;
    // }

    //included prints in hashfunctoins.c functions so just call them here and dont print
    switch (t.type) {
    case CMD_INSERT:
        insert(a->table, a->table_sz, t.name, t.value, t.priority);
        break;
    case CMD_UPDATE: {
        uint32_t old = 0;
        updateSalary(a->table, a->table_sz, t.name, t.value, &old, t.priority);
        break;
    }
    case CMD_DELETE:
        delete(a->table, a->table_sz, t.name, t.priority);
        break;
    case CMD_SEARCH:
        search(a->table, a->table_sz, t.name, t.priority);
        break;
    case CMD_PRINT:
        print_table(a->table, a->table_sz, t.priority, global_log, stdout);
        printf("PRINT completed\n");
        break;
    default:
        break;
}

    return NULL;
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

    global_log = fopen("hash.log","w"); // your HashFunctions.c uses this pointer

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

    if (global_log) { fclose(global_log); global_log=NULL; }
    destroy_locks(table_size);

    // free table
    for (size_t i=0;i<table_size;i++){
        hashRecord *cur=table[i];
        while (cur){ hashRecord *nx=cur->next; free(cur); cur=nx; }
    }
    free(table); free(tasks); free(ths); free(args);
    return 0;
}
