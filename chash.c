// chash.c — sequential driver for PA#2 (no pthreads required)
// Build:
//   gcc -std=c11 -Wall -Wextra -O2 HashFunctions.c chash.c -o chash.exe
//
// Run (defaults to commands.txt in the current folder):
//   .\chash.exe
// Or pass a different file path:
//   .\chash.exe ".\commands-comprehensive-test.txt"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#ifdef _WIN32
#include <direct.h>   // _getcwd
#endif

// ---- types & prototypes from your HashFunctions.c ----
typedef struct hash_struct {
    uint32_t hash;
    char     name[50];
    uint32_t salary;
    struct hash_struct *next;
} hashRecord;

uint32_t one_at_a_time_hash(const uint8_t* key, size_t length);
int       insert(hashRecord **table, size_t table_size, const char *name, uint32_t salary);
int       updateSalary(hashRecord **table, size_t table_size, const char *key, uint32_t new_salary, uint32_t *old_salary_out);
int       delete(hashRecord **table, size_t table_size, const char *key);
char*     search(hashRecord **table, size_t table_size, const char *key);
void      print_table(hashRecord **table, size_t table_size, int priority, FILE *logf, FILE *out);

// ----- helpers -----
static void trim(char *s){
    size_t n = strlen(s);
    while (n && (s[n-1]=='\r' || s[n-1]=='\n' || s[n-1]==' ' || s[n-1]=='\t')) s[--n]='\0';
}

static int split4(char *line, char **a, char **b, char **c, char **d){
    *a = strtok(line, ",");
    *b = strtok(NULL, ",");
    *c = strtok(NULL, ",");
    *d = strtok(NULL, ",");
    return (*a != NULL);
}

// portable replacement for strdup/_strdup
static char* my_strdup(const char* s){
    size_t n = strlen(s) + 1;
    char *p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

int main(int argc, char **argv) {
    // Allow overriding the file on the command line; default to "commands.txt"
    const char *CMD_FILE = (argc > 1) ? argv[1] : "commands.txt";

    FILE *f = fopen(CMD_FILE, "r");
    if (!f) {
#ifdef _WIN32
        char cwd[1024]; _getcwd(cwd, sizeof(cwd));
        fprintf(stderr, "couldn't open commands file '%s'\n", CMD_FILE);
        fprintf(stderr, "current working dir: %s\n", cwd);
#endif
        perror(CMD_FILE);
        return 1;
    }

    // hash table
    size_t table_size = 64;  // any reasonable bucket count is fine
    hashRecord **table = (hashRecord**)calloc(table_size, sizeof(hashRecord*));
    if (!table) { perror("calloc table"); fclose(f); return 1; }

    // Optional log file (pass NULL to print_table to disable logging)
    FILE *logf = fopen("hash.log", "w");

    char buf[512];
    int lineno = 0;

    while (fgets(buf, sizeof(buf), f)) {
        lineno++;
        trim(buf);
        if (buf[0]=='\0' || buf[0]=='#') continue;

        char *dup = my_strdup(buf);
        if (!dup) { fprintf(stderr, "OOM\n"); break; }

        char *cmd, *p2, *p3, *p4;
        if (!split4(dup, &cmd, &p2, &p3, &p4)) { free(dup); continue; }

        if (strcmp(cmd,"threads")==0) {
            // ignored in sequential mode
        }
        else if (strcmp(cmd,"insert")==0) {
            const char *name = p2 ? p2 : "";
            uint32_t sal = (uint32_t)strtoul(p3 ? p3 : "0", NULL, 10);
            int rc = insert(table, table_size, name, sal);
            printf("INSERT %s %s\n", name, rc==1 ? "Inserted" : (rc==0 ? "Exists" : "Failed"));
        }
        else if (strcmp(cmd,"update")==0) {
            const char *name = p2 ? p2 : "";
            uint32_t sal = (uint32_t)strtoul(p3 ? p3 : "0", NULL, 10);
            uint32_t old=0;
            int rc = updateSalary(table, table_size, name, sal, &old);
            if (rc==1) printf("UPDATE %s from %u to %u\n", name, old, sal);
            else       printf("UPDATE %s not found\n", name);
        }
        else if (strcmp(cmd,"delete")==0) {
            const char *name = p2 ? p2 : "";
            int rc = delete(table, table_size, name);
            printf("DELETE %s %s\n", name, rc==1 ? "Deleted" : "Not found");
        }
        else if (strcmp(cmd,"search")==0) {
            const char *name = p2 ? p2 : "";
            char *res = search(table, table_size, name);
            if (res) printf("SEARCH Found: %s\n", res);
            else     printf("SEARCH %s not found\n", name);
        }
        else if (strcmp(cmd,"print")==0) {
            int pri = p4 ? atoi(p4) : 0;
            print_table(table, table_size, pri, logf, stdout);
        }
        else {
            printf("Line %d: unknown command: %s\n", lineno, cmd ? cmd : "(null)");
        }

        free(dup);
    }

    if (logf) fclose(logf);
    fclose(f);

    // cleanup table
    for (size_t i=0;i<table_size;i++){
        hashRecord *cur = table[i];
        while (cur) { hashRecord *n = cur->next; free(cur); cur = n; }
    }
    free(table);
    return 0;
}
