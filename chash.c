#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define _TIMESPEC_DEFINED
#include <pthread.h>
#include <sys/time.h>
#include "HashFunctions.h"
#include "hash_logger.h"

#define INPUT_FILE "commands.txt"
#define OUTPUT_FILE "console_output.txt"

hashTable table;

typedef struct {
    char action[10];
    char name[50];
    unsigned int salary;
    int priority;
} command;

command *parse_command(char *command_line) {
    // Simple parser to extract command and parameters
    command *cmd = (command *)malloc(sizeof(command));
    memset(cmd, 0, sizeof(command));

    char *line = strtok(command_line, ",");
    if(!line) {
        free(cmd);
        return NULL;
    }
    strcpy(cmd->action, line);

    if (strcmp(cmd->action, "insert") == 0) {
        char *name = strtok(NULL, ",");
        char *salary_str = strtok(NULL, ",");
        char *priority_str = strtok(NULL, ",");
        if (name && salary_str && priority_str) {
            strncpy(cmd->name, name, sizeof(cmd->name)-1);
            cmd->salary = (unsigned int)atoi(salary_str);
            cmd->priority = atoi(priority_str);
        }
    } else if (strcmp(cmd->action, "search") == 0) {
        char *name = strtok(NULL, ",");
        char *priority_str = strtok(NULL, ",");
        if (name && priority_str) {
            strncpy(cmd->name, name, sizeof(cmd->name)-1);
            cmd->priority = atoi(priority_str);
        }
    } else if (strcmp(cmd->action, "update") == 0) {
        char *name = strtok(NULL, ",");
        char *salary_str = strtok(NULL, ",");
        char *priority_str = strtok(NULL, ",");
        if (name && salary_str && priority_str) {
            strncpy(cmd->name, name, sizeof(cmd->name)-1);
            cmd->salary = (unsigned int)atoi(salary_str);
            cmd->priority = atoi(priority_str);
        }
    } else if (strcmp(cmd->action, "print") == 0) {
        char *priority_str = strtok(NULL, ",");
        if (priority_str) {
            cmd->priority = atoi(priority_str);
        }
    } else if (strcmp(cmd->action, "delete") == 0) {
        char *name = strtok(NULL, ",");
        char *priority_str = strtok(NULL, ",");
        if (name && priority_str) {
            strncpy(cmd->name, name, sizeof(cmd->name)-1);
            cmd->priority = atoi(priority_str);
        }
    } else {
        free(cmd);
        return NULL;
    }
    return cmd;
}

void *execution(void *arg) {
    command *cmd = (command *)arg;

    //log command with hash_logger_log
    log_thread_msg(cmd->priority, cmd->action);

    //call each function
    if (strcmp(cmd->action, "insert") == 0) {
        insert(table.table, table.size, cmd->name, cmd->salary, cmd->priority, table.locks);
    } else if (strcmp(cmd->action, "search") == 0) {
        search(table.table, table.size, cmd->name, table.locks, cmd->priority);
    } else if (strcmp(cmd->action, "update") == 0) {
        updateSalary(table.table, table.size, cmd->name, cmd->salary, table.locks, cmd->priority);
    } else if (strcmp(cmd->action, "print") == 0) {
        print_table(table.table, table.size, cmd->priority, NULL, stdout, table.locks);
    } else if (strcmp(cmd->action, "delete") == 0) {
        delete(table.table, table.size, cmd->name, table.locks, cmd->priority);
    }
    free(cmd);
    return NULL;
}

int main() {
    // Initialize hash table
    initTable(&table);
    // Initialize logger
    logger_init("hash.log");
    // Open input and output files
    FILE *inputFile = fopen(INPUT_FILE, "r");

    pthread_t threads[100];
    int thread_count = 0;
    FILE *outputFile = fopen(OUTPUT_FILE, "w");
    if (!inputFile || !outputFile) {
        perror("File opening failed");
        exit(1);
    } 

    char line[256];
    while (fgets(line, sizeof(line), inputFile)) {
        // Process each command
        fprintf(outputFile, "Processing command: %s", line);
        command *cmd = parse_command(line);
        if (!cmd) {
            fprintf(outputFile, "Invalid command format: %s\n", line);
            continue;
        }
        // Create a thread for each command
        pthread_create(&threads[thread_count++], NULL, execution, (void *)cmd);
    }
    fclose(inputFile);

    // Wait for all threads to finish
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    fclose(outputFile);
    // Destroy hash table
    destroyTable(&table);
    // Close logger
    logger_close();
    return 0;
}